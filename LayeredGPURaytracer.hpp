#pragma once

#include <cuda_runtime_api.h>
#include <cstdint>
#include <cstdlib>

#include "AbstractRaytracer.hpp"
#include "util.hpp"

/// Color constants for debugging, unused
namespace colors {
    __device__ const mm::vec3
        BLACK = {0.0f, 0.0f, 0.0f},
        WHITE = {1.0f, 1.0f, 1.0f},
        RED = {1.0f, 0.0f, 0.0f},
        GREEN = {0.0f, 1.0f, 0.0f},
        BLUE = {0.0f, 0.0f, 1.0f},
        CYAN = {0.0f, 1.0f, 1.0f},
        MAGENTA = {1.0f, 0.0f, 1.0f},
        YELLOW = {1.0f, 1.0f, 0.0f}
    ;
}

__global__ void dummy_kernel_v2() {}

/// Represents stored data per layer, kept on GPU
struct LayeredIntersectionData {
    Intersection intersection;
    mm::vec3 direction; // of ray that generated intersection, need to store for shading
    mm::vec3 baseColor; // phong color w/ shadows but w/o reflections or refractions, on down pass, reflections and refractions get mixed in
    float source_refraction; // of ray that generated intersection

    __device__ [[nodiscard]] static inline LayeredIntersectionData invalid() {
        return {Intersection::invalid(), mm::vec3(0.0f), mm::vec3(0.0f), 1.0f};
    }
};

// Compute only the direct intersection color, no reflections/refractions, a subset of CPU's shade()
__device__ [[nodiscard]] mm::vec3 cuda_shade_v2_base_color(
    const Intersection& intersection, const Ray& ray,
    const unsigned int shapec, const Shape* __restrict__ shapev,
    const unsigned int lightc, const Light* __restrict__ lightv
) {
    mm::vec3 color(0.0f, 0.0f, 0.0f);

    if(!intersection.is_valid()) {
        return color;
    }

    const Material& material = *intersection.material;
    const mm::vec3
        I = mm::vec3::normalize(ray.direction),
        V = -I,
        p = intersection.pos
    ;

    color += material.ambient*0.05f;

    for(const Light* light = lightv; light < lightv + lightc; light++) {
        if(float transmission = Light::transmission(p, *light, shapec, shapev); transmission > 0.0f) {
            const mm::vec3
                N = mm::vec3::normalize(intersection.normal),
                L = mm::vec3::normalize(light->pos - p),
                R = -L + 2.0f*N*mm::vec3::dot(N, L)
            ;

            color += material.diffuse*transmission*light->diffuse*mm::max(mm::vec3::dot(L, N), 0.0f);
            color += material.specular*transmission*light->specular*mm::pow(mm::max(mm::vec3::dot(V, R), 0.0f), material.shininess);
            color += material.ambient*transmission*light->ambient;
        }
    }

    return color;
}

// Pop a layer and merge down
template<bool BUFFER_OBJECTS> __global__ void cuda_raytracer_v2_merge_down(
    const unsigned int layerStart, const unsigned int layerSize,
    const unsigned int shapec, const Shape* __restrict__ shapev,
    const unsigned int lightc, const Light* __restrict__ lightv,
    LayeredIntersectionData* layers
) {
    extern __shared__ Shape shapev_buf[];

    // If enabled, copy objects to shared mem
    if constexpr (BUFFER_OBJECTS) {
        for(int i = threadIdx.y*blockDim.x + threadIdx.x; i < shapec; i += blockDim.x*blockDim.y) {
            shapev_buf[i] = shapev[i];
        }
        __syncthreads();
    }

    // Index within layer
    const unsigned int idx = blockIdx.x*blockDim.x + threadIdx.x;

    // Combine the reflections and refractions down a layer with the old base color, like part of CPU's shade()
    if(idx < layerSize) {
        const Intersection intersection = layers[layerStart + idx].intersection;
        if(intersection.is_valid()) {

            const Material& material = *intersection.material;
            const mm::vec3 I = mm::vec3::normalize(layers[layerStart + idx].direction);
    
            // Flip if back face/inside
            const bool front_facing = mm::vec3::dot(I, intersection.normal) < 0.0f;
            const mm::vec3 N = (2.0f*front_facing - 1.0f)*mm::vec3::normalize(intersection.normal);
            const float 
                n1 = front_facing ? layers[layerStart + idx].source_refraction : material.indexOfRefraction,
                n2 = front_facing ? material.indexOfRefraction : layers[layerStart + idx].source_refraction
            ;
    
            // Compute Fresnel reflection coefficient using Schlick's Approximation
            // https://en.wikipedia.org/wiki/Schlick%27s_approximation
            const float R0 = mm::pow((n1 - n2)/(n1 + n2), 2.0f);
            const float R = R0 + (1 - R0)*mm::pow(1 - mm::clamp(-mm::vec3::dot(I, N), -1.0f, 1.0f), 5.0f);
    
            // Combine Fresnel weights with explicit material weights
            // I'm not sure the right way to combine the explicit weights with the Fresnel ones, but this looks good, so it's good enough
            const float 
                reflectivity = material.reflectivity,
                refractivity = mm::clamp((1.0f - R)*(1.0f - material.alpha), 0.0f, 1.0f)
            ;
    
            const mm::vec3 reflection = reflectivity > 0.0f ? layers[layerStart + layerSize + idx*2].baseColor : mm::vec3(0.0f);
            
            const float k = 1.0f - (n1/n2)*(n1/n2)*(1.0f - mm::vec3::dot(N, I)*mm::vec3::dot(N, I));
            const mm::vec3 refraction = material.alpha < 1.0f && k >= 0.0f ? layers[layerStart + layerSize + idx*2 + 1].baseColor : mm::vec3(0.0f);
    
            layers[layerStart + idx].baseColor = (reflection*reflectivity + refraction*refractivity + (1.0f - reflectivity - refractivity)*layers[layerStart + idx].baseColor).map(mm::clamp, 0.0f, 1.0f);
        }
    }
}

// For each previous hit, spawn a reflection and refraction
template<bool BUFFER_OBJECTS> __global__ void cuda_raytracer_v2_spawn_next(
    const unsigned int layerStart, const unsigned int layerSize,
    const unsigned int shapec, const Shape* __restrict__ shapev,
    const unsigned int lightc, const Light* __restrict__ lightv,
    LayeredIntersectionData* layers
) {
    extern __shared__ Shape shapev_buf[];

    // If enabled, copy objects to shared mem
    if constexpr (BUFFER_OBJECTS) {
        for(int i = threadIdx.y*blockDim.x + threadIdx.x; i < shapec; i += blockDim.x*blockDim.y) {
            shapev_buf[i] = shapev[i];
        }
        __syncthreads();
    }

    // Index within layer
    const unsigned int idx = blockIdx.x*blockDim.x + threadIdx.x;

    if(idx < layerSize) {
        const Intersection intersection = layers[layerStart + idx].intersection;

        if(intersection.is_valid()) {
            const Material& material = *intersection.material;
            const mm::vec3 I = mm::vec3::normalize(layers[layerStart + idx].direction), p = intersection.pos;
    
            // Flip if back face/inside
            const bool front_facing = mm::vec3::dot(I, intersection.normal) < 0.0f;
            const mm::vec3 N = (2.0f*front_facing - 1.0f)*mm::vec3::normalize(intersection.normal);
            const float 
                n1 = front_facing ? layers[layerStart + idx].source_refraction : material.indexOfRefraction,
                n2 = front_facing ? material.indexOfRefraction : layers[layerStart + idx].source_refraction
            ;
    
            // Compute Fresnel reflection coefficient using Schlick's Approximation
            // https://en.wikipedia.org/wiki/Schlick%27s_approximation
            const float R0 = mm::pow((n1 - n2)/(n1 + n2), 2.0f);
            const float R = R0 + (1 - R0)*mm::pow(1 - mm::clamp(-mm::vec3::dot(I, N), -1.0f, 1.0f), 5.0f);

            // Combine Fresnel weights with explicit material weights
            // I'm not sure the right way to combine the explicit weights with the Fresnel ones, but this looks good, so it's good enough
            const float 
                reflectivity = material.reflectivity,
                _refractivity = mm::clamp((1.0f - R)*(1.0f - material.alpha), 0.0f, 1.0f)
            ;

            // Spawn reflection and store in layers
            if(reflectivity > 0.0f) {
                const Ray reflection_ray(p, mm::vec3::normalize(I - 2.0f*N*mm::vec3::dot(I, N)));
                const Intersection reflection_intersection = Intersection::of(reflection_ray, shapec, shapev);
                layers[layerStart + layerSize + idx*2] = {reflection_intersection, reflection_ray.direction, cuda_shade_v2_base_color(reflection_intersection, reflection_ray, shapec, shapev, lightc, lightv), n1};
            } else {
                layers[layerStart + layerSize + idx*2] = LayeredIntersectionData::invalid(); // Since layers don't get cleared, need to fill with empty data if no hit
            }
            
            // Spawn refraction and store in layers
            const float k = 1.0f - (n1/n2)*(n1/n2)*(1.0f - mm::vec3::dot(N, I)*mm::vec3::dot(N, I));
            if(material.alpha < 1.0f && k >= 0.0f) {
                const Ray refraction_ray(p, (n1/n2)*I - N*((n1/n2)*mm::vec3::dot(N, I) + mm::sqrt(k)));
                const Intersection refraction_intersection = Intersection::of(refraction_ray, shapec, shapev);
                layers[layerStart + layerSize + idx*2 + 1] = {refraction_intersection, refraction_ray.direction, cuda_shade_v2_base_color(refraction_intersection, refraction_ray, shapec, shapev, lightc, lightv), n2};
            } else {
                layers[layerStart + layerSize + idx*2 + 1] = LayeredIntersectionData::invalid();
            }
        } else {
            layers[layerStart + layerSize + idx*2] = LayeredIntersectionData::invalid();
            layers[layerStart + layerSize + idx*2 + 1] = LayeredIntersectionData::invalid();
        }
    }
}

/// Spawn a view ray like the foreach x,y of CPU version
template<bool BUFFER_OBJECTS> __global__ void cuda_raytracer_v2_spawn_initial_view_rays(
    const unsigned int WIDTH, const unsigned int HEIGHT,
    const float ANTIALIASING_SAMPLES, const float ANTIALIASING_STRIDE,
    const mm::vec3 LOCAL_X, const mm::vec3 LOCAL_Y, const mm::vec3 LOCAL_Z,
    const float HFOV, const float VFOV, const mm::vec3 EYE_POS,
    const unsigned int shapec, const Shape* __restrict__ shapev,
    const unsigned int lightc, const Light* __restrict__ lightv,
    LayeredIntersectionData* layers
) {
    extern __shared__ Shape shapev_buf[];

    // If enabled, copy objects to shared mem
    if constexpr (BUFFER_OBJECTS) {
        for(int i = threadIdx.y*blockDim.x + threadIdx.x; i < shapec; i += blockDim.x*blockDim.y) {
            shapev_buf[i] = shapev[i];
        }
        __syncthreads();
    }

    const unsigned int px = blockIdx.x*blockDim.x + threadIdx.x;
    const unsigned int py = blockIdx.y*blockDim.y + threadIdx.y;

    if(px < WIDTH && py < HEIGHT) {
        const unsigned int idx = (py*WIDTH + px)*ANTIALIASING_SAMPLES;

        unsigned int sampleIdx = 0;
        for(float dx = ANTIALIASING_STRIDE/2.0f; dx < 1.0f; dx += ANTIALIASING_STRIDE) {
            for(float dy = ANTIALIASING_STRIDE/2.0f; dy < 1.0f; dy += ANTIALIASING_STRIDE) {
                const float x = (2.0f*(px + dx) - (float) WIDTH)/(float) WIDTH*mm::tan(HFOV/2);
                const float y = (2.0f*(py + dy) - (float) HEIGHT)/(float) HEIGHT*mm::tan(VFOV/2);
                
                const Ray ray(EYE_POS, mm::vec3::normalize(LOCAL_Z + x*LOCAL_X - y*LOCAL_Y));
                const Intersection intersection = Intersection::of(ray, shapec, shapev);

                // Instead of tracing to completion, only do first hit and store for later
                layers[idx + sampleIdx++] = {intersection, ray.direction, cuda_shade_v2_base_color(intersection, ray, shapec, shapev, lightc, lightv), 1.0f};
            }
        }
    }
}

/// Average samples and write to the image array as the last step
__global__ void cuda_raytracer_v2_finalize(
    const unsigned int WIDTH, const unsigned int HEIGHT,
    const float ANTIALIASING_SAMPLES,
    LayeredIntersectionData* layers, unsigned char* bytes
) {
    const unsigned int px = blockIdx.x*blockDim.x + threadIdx.x;
    const unsigned int py = blockIdx.y*blockDim.y + threadIdx.y;

    if(px < WIDTH && py < HEIGHT) {
        const unsigned int idx = (py*WIDTH + px)*AbstractRayTracer::CHANNELS;
        mm::vec3 color(0.0f, 0.0f, 0.0f);

        for(unsigned int sampleIdx = 0; sampleIdx < ANTIALIASING_SAMPLES; sampleIdx++) {
            color += (1.0f/ANTIALIASING_SAMPLES)*layers[(py*WIDTH + px)*(unsigned int) ANTIALIASING_SAMPLES + sampleIdx].baseColor;
        }

        bytes[idx + 0] = static_cast<unsigned char>(color.r()*255);
        bytes[idx + 1] = static_cast<unsigned char>(color.g()*255);
        bytes[idx + 2] = static_cast<unsigned char>(color.b()*255);
        bytes[idx + 3] = static_cast<unsigned char>(1.0f*255);
    }
}

class LayeredGPURaytracer final : public AbstractRayTracer {
    private:
        cudaStream_t _stream;
        unsigned char* _d_bytes;

        LayeredIntersectionData* _d_layers;

        Shape* SHAPEV;
        Shape* _d_SHAPEV;
        const unsigned int SHAPEC;
        const unsigned int DSHAPEC;

        Light* _d_LIGHTV;
        const unsigned int LIGHTC;

    public:
        const unsigned int BLOCK_WIDTH, BLOCK_HEIGHT;  
        const bool PARTITION_OBJECTS, BUFFER_OBJECTS;

        LayeredGPURaytracer(
            const unsigned int width, const unsigned int height,
            const unsigned int antialiasing,
            const unsigned int shapec, Shape* shapev, const unsigned int dshapec,
            const unsigned int lightc, const Light* lightv,
            const bool partition_objects = false, const bool buffer_objects = false,
            const unsigned int block_width = 16, const unsigned int block_height = 16
        ) : 
            AbstractRayTracer(width, height, antialiasing),
            PARTITION_OBJECTS(partition_objects), BUFFER_OBJECTS(buffer_objects),
            BLOCK_WIDTH(block_width), BLOCK_HEIGHT(block_height),
            SHAPEV(shapev), SHAPEC(shapec), DSHAPEC(dshapec), LIGHTC(lightc)
        {
            cudaStreamCreate(&this->_stream);
            cudaMalloc(&this->_d_bytes, this->size());

            cudaMalloc(&this->_d_SHAPEV, sizeof(Shape)*this->SHAPEC);
            cudaMalloc(&this->_d_LIGHTV, sizeof(Light)*this->LIGHTC);

            cudaMalloc(&this->_d_layers, sizeof(LayeredIntersectionData)*(
                // sum(HEIGHT*WIDTH*SAMPLES*pow(2, layer) foreach layer in 0..ITERATIONS), full binary tree
                (this->HEIGHT*this->WIDTH*this->ANTIALIASING.SAMPLES)*((1 << (AbstractRayTracer::ITERATIONS + 1)) - 1)
            ));

            cudaMemcpy(this->_d_SHAPEV, shapev, sizeof(Shape)*this->SHAPEC, cudaMemcpyHostToDevice);
            cudaMemcpy(this->_d_LIGHTV, lightv, sizeof(Light)*this->LIGHTC, cudaMemcpyHostToDevice);
        
            // Verify CUDA compiled correctly
            dummy_kernel_v2<<<1,1>>>();
            cudaError_t error;
            if((error = cudaGetLastError()) != cudaSuccess || (error = cudaDeviceSynchronize()) != cudaSuccess) {
                util::error("CUDA Error: %s (%s:%d)", cudaGetErrorString(error), __FILE__, __LINE__);
                exit(13);
            }
        }

        virtual ~LayeredGPURaytracer() override {
            cudaFree(this->_d_layers);
            cudaFree(this->_d_LIGHTV);
            cudaFree(this->_d_SHAPEV);
            cudaFree(this->_d_bytes);
            cudaStreamDestroy(this->_stream);
        }

        virtual void run(const Camera& camera, const mm::vec3& localX, const mm::vec3& localY, const mm::vec3& localZ) override {
            // Copy objects again after update (CPURaytracer doesn't have to do this since the data lives on the CPU where the
            // animation happens unlike here where the data lives on the GPU). Alernative solutions include doing updates on the
            // GPU instead which is good for particles but not input based changes.
            
            // There's no reason you wouldn't partition your objects like this irl (assuming you know ahead of time which are dynamic).
            // The only real complexity comes while loading data and tracking a second pointer and/or offset; afterwards, it's as simple
            // as only copying part of the data.
            if(this->PARTITION_OBJECTS) {
                cudaMemcpy(this->_d_SHAPEV, this->SHAPEV, sizeof(Shape)*this->DSHAPEC, cudaMemcpyHostToDevice);
            } else {
                cudaMemcpy(this->_d_SHAPEV, this->SHAPEV, sizeof(Shape)*this->SHAPEC, cudaMemcpyHostToDevice);
            }
            // TODO: restore object buffering
            
            if(this->BUFFER_OBJECTS) {
                // Launch initial view rays
                cuda_raytracer_v2_spawn_initial_view_rays<true><<<
                    dim3((this->WIDTH + this->BLOCK_WIDTH - 1)/this->BLOCK_WIDTH, (this->HEIGHT + this->BLOCK_HEIGHT - 1)/this->BLOCK_HEIGHT),
                    dim3(this->BLOCK_WIDTH, this->BLOCK_HEIGHT),
                    this->BUFFER_OBJECTS ? sizeof(Shape)*this->SHAPEC : 0
                >>>(
                    this->WIDTH, this->HEIGHT,
                    this->ANTIALIASING.SAMPLES, this->ANTIALIASING.STRIDE,
                    localX, localY, localZ,
                    camera.hfov(this->WIDTH, this->HEIGHT), camera.vfov, camera.eye_pos,
                    this->SHAPEC, this->_d_SHAPEV,
                    this->LIGHTC, this->_d_LIGHTV,
                    this->_d_layers
                );

                // Propagate reflections and refractions from layer `bounce` into layer `bounce + 1`
                for(unsigned int bounce = 0; bounce < AbstractRayTracer::ITERATIONS; bounce++) {
                    // Start offset of layer, size of this layer
                    const unsigned int
                        layerStart = (this->WIDTH*this->HEIGHT*this->ANTIALIASING.SAMPLES)*((1 << bounce) - 1),
                        layerSize = (this->WIDTH*this->HEIGHT*this->ANTIALIASING.SAMPLES)*(1 << bounce)
                    ;

                    cuda_raytracer_v2_spawn_next<true><<<
                        (layerSize + (this->BLOCK_WIDTH*this->BLOCK_HEIGHT) - 1)/(this->BLOCK_WIDTH*this->BLOCK_HEIGHT),
                        this->BLOCK_WIDTH*this->BLOCK_HEIGHT, 
                        this->BUFFER_OBJECTS ? sizeof(Shape)*this->SHAPEC : 0
                    >>>(
                        layerStart, layerSize,
                        this->SHAPEC, this->_d_SHAPEV,
                        this->LIGHTC, this->_d_LIGHTV,
                        this->_d_layers
                    );
                }

                // Merge `baseColor` from relections and refractions in layer `bounce + 1` into layer `bounce`
                for(unsigned int bounce = AbstractRayTracer::ITERATIONS - 1; ; bounce--) {
                    const unsigned int
                        layerStart = (this->WIDTH*this->HEIGHT*this->ANTIALIASING.SAMPLES)*((1 << bounce) - 1),
                        layerSize = (this->WIDTH*this->HEIGHT*this->ANTIALIASING.SAMPLES)*(1 << bounce)
                    ;

                    cuda_raytracer_v2_merge_down<true><<<
                        (layerSize + (this->BLOCK_WIDTH*this->BLOCK_HEIGHT) - 1)/(this->BLOCK_WIDTH*this->BLOCK_HEIGHT),
                        this->BLOCK_WIDTH*this->BLOCK_HEIGHT,
                        this->BUFFER_OBJECTS ? sizeof(Shape)*this->SHAPEC : 0
                    >>>(
                        layerStart, layerSize,
                        this->SHAPEC, this->_d_SHAPEV,
                        this->LIGHTC, this->_d_LIGHTV,
                        this->_d_layers
                    );

                    if(bounce == 0) {
                        break;
                    }
                }
            } else {
                // Launch initial view rays
                cuda_raytracer_v2_spawn_initial_view_rays<false><<<
                    dim3((this->WIDTH + this->BLOCK_WIDTH - 1)/this->BLOCK_WIDTH, (this->HEIGHT + this->BLOCK_HEIGHT - 1)/this->BLOCK_HEIGHT),
                    dim3(this->BLOCK_WIDTH, this->BLOCK_HEIGHT),
                    this->BUFFER_OBJECTS ? sizeof(Shape)*this->SHAPEC : 0
                >>>(
                    this->WIDTH, this->HEIGHT,
                    this->ANTIALIASING.SAMPLES, this->ANTIALIASING.STRIDE,
                    localX, localY, localZ,
                    camera.hfov(this->WIDTH, this->HEIGHT), camera.vfov, camera.eye_pos,
                    this->SHAPEC, this->_d_SHAPEV,
                    this->LIGHTC, this->_d_LIGHTV,
                    this->_d_layers
                );

                // Propagate reflections and refractions from layer `bounce` into layer `bounce + 1`
                for(unsigned int bounce = 0; bounce < AbstractRayTracer::ITERATIONS; bounce++) {
                    // Start offset of layer, size of this layer
                    const unsigned int
                        layerStart = (this->WIDTH*this->HEIGHT*this->ANTIALIASING.SAMPLES)*((1 << bounce) - 1),
                        layerSize = (this->WIDTH*this->HEIGHT*this->ANTIALIASING.SAMPLES)*(1 << bounce)
                    ;

                    cuda_raytracer_v2_spawn_next<false><<<
                        (layerSize + (this->BLOCK_WIDTH*this->BLOCK_HEIGHT) - 1)/(this->BLOCK_WIDTH*this->BLOCK_HEIGHT),
                        this->BLOCK_WIDTH*this->BLOCK_HEIGHT, 
                        this->BUFFER_OBJECTS ? sizeof(Shape)*this->SHAPEC : 0
                    >>>(
                        layerStart, layerSize,
                        this->SHAPEC, this->_d_SHAPEV,
                        this->LIGHTC, this->_d_LIGHTV,
                        this->_d_layers
                    );
                }

                // Merge `baseColor` from relections and refractions in layer `bounce + 1` into layer `bounce`
                for(unsigned int bounce = AbstractRayTracer::ITERATIONS - 1; ; bounce--) {
                    const unsigned int
                        layerStart = (this->WIDTH*this->HEIGHT*this->ANTIALIASING.SAMPLES)*((1 << bounce) - 1),
                        layerSize = (this->WIDTH*this->HEIGHT*this->ANTIALIASING.SAMPLES)*(1 << bounce)
                    ;

                    cuda_raytracer_v2_merge_down<false><<<
                        (layerSize + (this->BLOCK_WIDTH*this->BLOCK_HEIGHT) - 1)/(this->BLOCK_WIDTH*this->BLOCK_HEIGHT),
                        this->BLOCK_WIDTH*this->BLOCK_HEIGHT,
                        this->BUFFER_OBJECTS ? sizeof(Shape)*this->SHAPEC : 0
                    >>>(
                        layerStart, layerSize,
                        this->SHAPEC, this->_d_SHAPEV,
                        this->LIGHTC, this->_d_LIGHTV,
                        this->_d_layers
                    );

                    if(bounce == 0) {
                        break;
                    }
                }
            }

            // Average AA rays
            cuda_raytracer_v2_finalize<<<
                dim3((this->WIDTH + this->BLOCK_WIDTH - 1)/this->BLOCK_WIDTH, (this->HEIGHT + this->BLOCK_HEIGHT - 1)/this->BLOCK_HEIGHT),
                dim3(this->BLOCK_WIDTH, this->BLOCK_HEIGHT)
            >>>(
                this->WIDTH, this->HEIGHT,
                this->ANTIALIASING.SAMPLES,
                this->_d_layers, this->_d_bytes
            );

            // Copy data out
            cudaMemcpy(this->_bytes, this->_d_bytes, this->size(), cudaMemcpyDeviceToHost);
        }
};