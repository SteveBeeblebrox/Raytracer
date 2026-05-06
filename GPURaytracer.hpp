#pragma once

#include <cuda_runtime_api.h>
#include <cstdint>

#include "AbstractRaytracer.hpp"

// TODO:
// enum class OptFlags : uint8_t {
//     NONE = 0
// };

template<unsigned int bounces> __device__ [[nodiscard]] mm::vec3 cuda_shade_v1(
    const Intersection& intersection, const Ray& ray, const float source_refraction,
    const unsigned int shapec, const Shape* shapev,
    const unsigned int lightc, const Light* lightv
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
    
    if(material.reflectivity > 0.0f || material.alpha < 1.0f) {
        // Flip if back face/inside
        const bool front_facing = mm::vec3::dot(I, intersection.normal) < 0.0f;
        const mm::vec3 N = (2.0f*front_facing - 1.0f)*mm::vec3::normalize(intersection.normal);
        const float 
            n1 = front_facing ? source_refraction : material.indexOfRefraction,
            n2 = front_facing ? material.indexOfRefraction : source_refraction
        ;

        // Compute Fresnel reflection coefficient using Schlick's Approximation
        // https://en.wikipedia.org/wiki/Schlick%27s_approximation
        const float R0 = mm::pow((n1 - n2)/(n1 + n2), 2.0f);
        const float R = R0 + (1 - R0)*mm::pow(1 - mm::clamp(-mm::vec3::dot(I, N), -1.0f, 1.0f), 5.0f);

        // Combine Fresnel weights with explicit material weights
        // I'm not sure the right way to combine the explicit weights with the Fresnel ones, but this looks good, so it's good enough
        const float 
            reflectivity = mm::clamp(R + material.reflectivity, 0.0f, 1.0f),
            refractivity = mm::clamp((1.0f - R)*(1.0f - material.alpha), 0.0f, 1.0f)
        ;
            
        const Ray reflection_ray(p, mm::vec3::normalize(I - 2.0f*N*mm::vec3::dot(I, N)));
        const mm::vec3 reflection = reflectivity > 0.0f ? cuda_shade_v1<bounces + 1>(Intersection::of(reflection_ray, shapec, shapev), reflection_ray, n1, shapec, shapev, lightc, lightv) : mm::vec3(0.0f);

        // Snell's law for refraction
        // https://en.wikipedia.org/wiki/Snell%27s_law#Vector_form
        // See https://registry.khronos.org/OpenGL-Refpages/gl4/html/refract.xhtml
        const float k = 1.0f - (n1/n2)*(n1/n2)*(1.0f - mm::vec3::dot(N, I)*mm::vec3::dot(N, I));
        const Ray refraction_ray(p, (n1/n2)*I - N*((n1/n2)*mm::vec3::dot(N, I) + mm::sqrt(k)));
        const mm::vec3 refraction = material.alpha < 1.0f && k >= 0.0f ? cuda_shade_v1<bounces + 1>(Intersection::of(refraction_ray, shapec, shapev), refraction_ray, n2, shapec, shapev, lightc, lightv) : mm::vec3(0.0f);

        color = reflection*reflectivity + refraction*refractivity + (1.0f - reflectivity - refractivity)*color; 
    }

    return color.map(mm::clamp, 0.0f, 1.0f);
}

template<> __device__ [[nodiscard]] mm::vec3 cuda_shade_v1<AbstractRayTracer::ITERATIONS + 1>(
    const Intersection& intersection, const Ray& ray, const float source_refraction,
    const unsigned int shapec, const Shape* shapev,
    const unsigned int lightc, const Light* lightv
) {
    return mm::vec3(0.0f, 0.0f, 0.0f);
}

__global__ void cuda_raytracer_v1(
    const unsigned int WIDTH, const unsigned int HEIGHT,
    const float ANTIALIASING_SAMPLES, const float ANTIALIASING_STRIDE,
    const mm::vec3 LOCAL_X, const mm::vec3 LOCAL_Y, const mm::vec3 LOCAL_Z,
    const float HFOV, const float VFOV, const mm::vec3 EYE_POS,
    const unsigned int shapec, const Shape* shapev,
    const unsigned int lightc, const Light* lightv,
    unsigned char* bytes
) {
    const unsigned int px = blockIdx.x*blockDim.x + threadIdx.x;
    const unsigned int py = blockIdx.y*blockDim.y + threadIdx.y;

    if(px < WIDTH && py < HEIGHT) {
        const unsigned int idx = (py*WIDTH + px)*AbstractRayTracer::CHANNELS;
        mm::vec3 color(0.0f, 0.0f, 0.0f);

        for(float dx = ANTIALIASING_STRIDE/2.0f; dx < 1.0f; dx += ANTIALIASING_STRIDE) {
            for(float dy = ANTIALIASING_STRIDE/2.0f; dy < 1.0f; dy += ANTIALIASING_STRIDE) {
                const float x = (2.0f*(px + dx) - (float) WIDTH)/(float) WIDTH*mm::tan(HFOV/2);
                const float y = (2.0f*(py + dy) - (float) HEIGHT)/(float) HEIGHT*mm::tan(VFOV/2);
                
                const Ray ray(EYE_POS, mm::vec3::normalize(LOCAL_Z + x*LOCAL_X - y*LOCAL_Y));
                const Intersection intersection = Intersection::of(ray, shapec, shapev);

                color += (1.0f/ANTIALIASING_SAMPLES)*cuda_shade_v1<0>(intersection, ray, 1.0f, shapec, shapev, lightc, lightv);
            }
        }

        bytes[idx + 0] = static_cast<unsigned char>(color.r()*255);
        bytes[idx + 1] = static_cast<unsigned char>(color.g()*255);
        bytes[idx + 2] = static_cast<unsigned char>(color.b()*255);
        bytes[idx + 3] = static_cast<unsigned char>(1.0f*255);
    }
}

class GPURaytracer final : public AbstractRayTracer {
    private:
        cudaStream_t _stream;
        unsigned char* _d_bytes;

        Shape* _d_SHAPEV;
        const unsigned int SHAPEC;

        Light* _d_LIGHTV;
        const unsigned int LIGHTC;

    public:
        const unsigned int BLOCK_WIDTH, BLOCK_HEIGHT;  

        GPURaytracer(
            const unsigned int width, const unsigned int height,
            const unsigned int antialiasing,
            const unsigned int shapec, const Shape* shapev,
            const unsigned int lightc, const Light* lightv,
            const unsigned int block_width = 16, const unsigned int block_height = 16
        ) : 
            AbstractRayTracer(width, height, antialiasing),
            BLOCK_WIDTH(block_width), BLOCK_HEIGHT(block_height),
            SHAPEC(shapec), LIGHTC(lightc)
        {
            cudaStreamCreate(&this->_stream);
            cudaMalloc(&this->_d_bytes, this->len_bytes());

            cudaMalloc(&this->_d_SHAPEV, sizeof(Shape)*this->SHAPEC);
            cudaMalloc(&this->_d_LIGHTV, sizeof(Light)*this->LIGHTC);

            cudaMemcpy(this->_d_SHAPEV, shapev, sizeof(Shape)*this->SHAPEC, cudaMemcpyHostToDevice);
            cudaMemcpy(this->_d_LIGHTV, lightv, sizeof(Light)*this->LIGHTC, cudaMemcpyHostToDevice);
        }

        virtual ~GPURaytracer() override {
            cudaFree(this->_d_LIGHTV);
            cudaFree(this->_d_SHAPEV);
            cudaFree(this->_d_bytes);
            cudaStreamDestroy(this->_stream);
        }

        virtual void run(const Camera& camera, const mm::vec3& localX, const mm::vec3& localY, const mm::vec3& localZ) override {
            // Launch kernel
            cuda_raytracer_v1<<<
                dim3((this->WIDTH + this->BLOCK_WIDTH - 1)/this->BLOCK_WIDTH, (this->HEIGHT + this->BLOCK_HEIGHT - 1)/this->BLOCK_HEIGHT),
                dim3(this->BLOCK_WIDTH, this->BLOCK_HEIGHT)
                // , TODO:
            >>>(
                this->WIDTH, this->HEIGHT,
                this->ANTIALIASING.SAMPLES, this->ANTIALIASING.STRIDE,
                localX, localY, localZ,
                camera.hfov(this->WIDTH, this->HEIGHT), camera.vfov, camera.eye_pos,
                this->SHAPEC, this->_d_SHAPEV,
                this->LIGHTC, this->_d_LIGHTV,
                this->_d_bytes
            );

            // Copy data out
            cudaMemcpy(this->_bytes, this->_d_bytes, this->len_bytes(), cudaMemcpyDeviceToHost);
        }
};