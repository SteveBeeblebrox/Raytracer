#pragma once

#include "AbstractRaytracer.hpp"

class CPURaytracer final : public AbstractRayTracer {
    private:
        const unsigned int SHAPEC;
        Shape* SHAPEV;

        const unsigned int LIGHTC;
        const Light* LIGHTV;
    public:
        CPURaytracer(
            const unsigned int width, const unsigned int height,
            const unsigned int antialiasing,
            const unsigned int shapec, Shape* shapev,
            const unsigned int lightc, const Light* lightv
        ) : AbstractRayTracer(width, height, antialiasing), SHAPEC(shapec), SHAPEV(shapev), LIGHTC(lightc), LIGHTV(lightv) {}

        virtual ~CPURaytracer() override {}

        [[nodiscard]] mm::vec3 shade(const Intersection& intersection, const Ray& ray, const float source_refraction, const unsigned int bounces) {
            mm::vec3 color(0.0f, 0.0f, 0.0f);

            if(!intersection.is_valid() || bounces > ITERATIONS) {
                return color;
            }

            const Material& material = *intersection.material;
            const mm::vec3
                I = mm::vec3::normalize(ray.direction),
                V = -I,
                p = intersection.pos
            ;

            color += material.ambient*0.05f;

            for(const Light* light = this->LIGHTV; light < this->LIGHTV + this->LIGHTC; light++) {
                if(float transmission = Light::transmission(p, *light, this->SHAPEC, this->SHAPEV); transmission > 0.0f) {
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
                const mm::vec3 reflection = reflectivity > 0.0f ? this->shade(Intersection::of(reflection_ray, this->SHAPEC, this->SHAPEV), reflection_ray, n1, bounces + 1) : mm::vec3(0.0f);

                // Snell's law for refraction
                // https://en.wikipedia.org/wiki/Snell%27s_law#Vector_form
                // See https://registry.khronos.org/OpenGL-Refpages/gl4/html/refract.xhtml
                const float k = 1.0f - (n1/n2)*(n1/n2)*(1.0f - mm::vec3::dot(N, I)*mm::vec3::dot(N, I));
                const Ray refraction_ray(p, (n1/n2)*I - N*((n1/n2)*mm::vec3::dot(N, I) + mm::sqrt(k)));
                const mm::vec3 refraction = material.alpha < 1.0f && k >= 0.0f ? this->shade(Intersection::of(refraction_ray, this->SHAPEC, this->SHAPEV), refraction_ray, n2, bounces + 1) : mm::vec3(0.0f);

                color = reflection*reflectivity + refraction*refractivity + (1.0f - reflectivity - refractivity)*color; 
            }

            return color.map(mm::clamp, 0.0f, 1.0f);
        }

        virtual void run(const Camera& camera, const mm::vec3& localX, const mm::vec3& localY, const mm::vec3& localZ) override {
            for(unsigned int py = 0; py < this->HEIGHT; py++) {
                for(unsigned int px = 0; px < this->WIDTH; px++) {
                    const unsigned int idx = (py*this->WIDTH + px)*AbstractRayTracer::CHANNELS;

                    mm::vec3 color(0.0f, 0.0f, 0.0f);

                    for(float dx = this->ANTIALIASING.STRIDE/2.0f; dx < 1.0f; dx += this->ANTIALIASING.STRIDE) {
                        for(float dy = this->ANTIALIASING.STRIDE/2.0f; dy < 1.0f; dy += this->ANTIALIASING.STRIDE) {
                            const float x = (2.0f*(px + dx) - (float) this->WIDTH)/(float) this->WIDTH*mm::tan(camera.hfov(this->WIDTH, this->HEIGHT)/2);
                            const float y = (2.0f*(py + dy) - (float) this->HEIGHT)/(float) this->HEIGHT*mm::tan(camera.vfov/2);
                            
                            const Ray ray(camera.eye_pos, mm::vec3::normalize(localZ + x*localX - y*localY));
                            const Intersection intersection = Intersection::of(ray, this->SHAPEC, this->SHAPEV);

                            color += (1.0f/this->ANTIALIASING.SAMPLES)*this->shade(intersection, ray, 1.0f, 0);
                        }
                    }

                    this->_bytes[idx + 0] = static_cast<unsigned char>(color.r()*255);
                    this->_bytes[idx + 1] = static_cast<unsigned char>(color.g()*255);
                    this->_bytes[idx + 2] = static_cast<unsigned char>(color.b()*255);
                    this->_bytes[idx + 3] = static_cast<unsigned char>(1.0f*255);
                }
            }
        }
};