#pragma once

#include "AbstractRaytracer.hpp"
#include "Types.hpp"

class CPURaytracer final : public AbstractRayTracer {
    private:
        unsigned char* _bytes;

        const unsigned int S_SHAPEC;
        const Shape* S_SHAPEV;

        const unsigned int S_LIGHTC;
        const Light* S_LIGHTV;
    public:
        CPURaytracer(
            const unsigned int width, const unsigned int height,
            const unsigned int antialiasing,
            const unsigned int s_shapec, const Shape* s_shapev,
            const unsigned int s_lightc, const Light* s_lightv
        ) : AbstractRayTracer(width, height, antialiasing), S_SHAPEC(s_shapec), S_SHAPEV(s_shapev), S_LIGHTC(s_lightc), S_LIGHTV(s_lightv) {
            this->_bytes = new unsigned char[this->WIDTH*this->HEIGHT*AbstractRayTracer::CHANNELS];
        }

        virtual ~CPURaytracer() override {
            delete[] this->_bytes;
        }

        [[nodiscard]] Intersection intersectRay(const Ray& ray) const {
            Intersection closestIntersection = Intersection::invalid();
            for(const Shape* shape = this->S_SHAPEV; shape < this->S_SHAPEV + this->S_SHAPEC; shape++) {
                Intersection intersection = shape->intersect(ray);
                if(intersection.is_valid() && (!closestIntersection.is_valid() || mm::vec3::distance(intersection.pos, ray.origin) < mm::vec3::distance(closestIntersection.pos, ray.origin))) {
                    closestIntersection = intersection;
                }
            }
            return closestIntersection;
        }

        [[nodiscard]] float intersectShadowRay(const mm::vec3& pos, const Light& light) const {
            // With only solid objects, this could return a boolean if obstructed, but for transparent objects, we need to track how much light was obstructed
            // Returns percent transmission in [0,1]
            float alpha = 0.0f;

            const mm::vec3 L = mm::vec3::normalize(light.pos - pos);
            
            const Ray ray = Ray(pos, L);
            for(const Shape* shape = this->S_SHAPEV; shape < this->S_SHAPEV + this->S_SHAPEC; shape++) {
                Intersection intersection = shape->intersect(ray);
                if(intersection.is_valid() && mm::vec3::distance(intersection.pos, ray.origin) < mm::vec3::distance(light.pos, ray.origin)) {
                    alpha += intersection.material->alpha;
                }

                // Bail early if possible
                if(alpha >= 1.0f) {
                    break;
                }
            }

            return 1.0f - mm::clamp(alpha, 0.0f, 1.0f);
        }

        [[nodiscard]] mm::vec3 shade(const Intersection& intersecton, const Ray& ray, const float source_refraction, const unsigned int bounces) {
            mm::vec3 color(0.0f, 0.0f, 0.0f);

            if(!intersecton.is_valid() || bounces > ITERATIONS) {
                return color;
            }

            const Material& material = *intersecton.material;
            const mm::vec3
                I = mm::vec3::normalize(ray.direction),
                V = -I,
                p = intersecton.pos
            ;

            color += material.ambient*0.05f;

            for(const Light* light = this->S_LIGHTV; light < this->S_LIGHTV + this->S_LIGHTC; light++) {
                if(float transmission = this->intersectShadowRay(p, *light); transmission > 0.0f) {
                    const mm::vec3
                        N = mm::vec3::normalize(intersecton.normal),
                        L = mm::vec3::normalize(light->pos - p),
                        R = -L + 2.0f*N*mm::vec3::dot(N, L)
                    ;

                    color += material.diffuse*transmission*light->diffuse*mm::max(mm::vec3::dot(L, N), 0.0f);
                    color += material.specular*transmission*light->specular*mm::pow(mm::max(mm::vec3::dot(V, R), 0.0f), material.shininess);
                    color += material.ambient*transmission*light->ambient;
                }
            }
            
            if(material.reflectivity > 0.0f || material.alpha < 1.0f) {
                // FLip if back face/inside
                const bool front_facing = mm::vec3::dot(I, intersecton.normal) < 0.0f;
                const mm::vec3 N = (2.0f*front_facing - 1.0f)*mm::vec3::normalize(intersecton.normal);
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
                const mm::vec3 reflection = reflectivity > 0.0f ? this->shade(this->intersectRay(reflection_ray), reflection_ray, n1, bounces + 1) : mm::vec3(0.0f);

                // Snell's law for refraction
                // https://en.wikipedia.org/wiki/Snell%27s_law#Vector_form
                // See https://registry.khronos.org/OpenGL-Refpages/gl4/html/refract.xhtml
                const float k = 1.0f - (n1/n2)*(n1/n2)*(1.0f - mm::vec3::dot(N, I)*mm::vec3::dot(N, I));
                const Ray refraction_ray(p, (n1/n2)*I - N*((n1/n2)*mm::vec3::dot(N, I) + mm::sqrt(k)));
                const mm::vec3 refraction = material.alpha < 1.0f && k >= 0.0f ? this->shade(this->intersectRay(refraction_ray), refraction_ray, n2, bounces + 1) : mm::vec3(0.0f);

                color = reflection*reflectivity + refraction*refractivity + (1.0f - reflectivity - refractivity)*color; 
            }

            return color.map(mm::clamp, 0.0f, 1.0f);
        }

        [[nodiscard]] virtual const unsigned char* run(const Camera& camera) override {
            // Compute the camera ray for the given (x,y) image pixel
            // See http://www.unknownroad.com/rtfm/graphics/rt_eyerays.html
            const float hfov = 2.0f*mm::atan(mm::tan(camera.vfov/2)*(float) this->WIDTH/(float) this->HEIGHT);
            const float samples = (this->ANTIALIASING_LEVEL + 1)*(this->ANTIALIASING_LEVEL + 1), stride = 1.0f/((float) this->ANTIALIASING_LEVEL + 1.0f);
            const mm::vec3
                localZ = mm::vec3::normalize(camera.look_pos - camera.eye_pos),
                localX = mm::vec3::normalize(mm::vec3::cross(localZ,  1.0f - mm::abs(mm::vec3::dot(localZ, mm::vec3(0.0f, 1.0f, 0.0f))) < 1e-3 ? mm::vec3(1.0f, 0.0f, 0.0f) : mm::vec3(0.0f, 1.0f, 0.0f))),
                localY = mm::vec3::normalize(mm::vec3::cross(localX, localZ))
            ;

            for(unsigned int py = 0; py < this->HEIGHT; py++) {
                for(unsigned int px = 0; px < this->WIDTH; px++) {
                    const unsigned int idx = (py*this->WIDTH + px)*AbstractRayTracer::CHANNELS;

                    mm::vec3 color(0.0f, 0.0f, 0.0f);

                    for(float dx = stride/2.0f; dx < 1.0f; dx += stride) {
                        for(float dy = stride/2.0f; dy < 1.0f; dy += stride) {
                            const float x = (2.0f*(px + dx) - (float) this->WIDTH)/(float) this->WIDTH*mm::tan(hfov/2);
                            const float y = (2.0f*(py + dy) - (float) this->HEIGHT)/(float)this->HEIGHT*mm::tan(camera.vfov/2);
                            
                            const Ray ray(camera.eye_pos, mm::vec3::normalize(localZ + x*localX - y*localY));
                            const Intersection intersection = this->intersectRay(ray);

                            color += (1.0f/samples)*this->shade(intersection, ray, 1.0f, 0);
                        }
                    }

                    this->_bytes[idx + 0] = static_cast<unsigned char>(color.r()*255);
                    this->_bytes[idx + 1] = static_cast<unsigned char>(color.g()*255);
                    this->_bytes[idx + 2] = static_cast<unsigned char>(color.b()*255);
                    this->_bytes[idx + 3] = static_cast<unsigned char>(1.0f*255);
                }
            }

            return this->_bytes;
        }
};