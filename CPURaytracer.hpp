#pragma once

#include "AbstractRaytracer.hpp"
#include "Types.hpp"

class CPURaytracer final : public AbstractRayTracer {
    private:
        unsigned char* _bytes;

        const Shape* S_SHAPEV;
        const unsigned int S_SHAPEC;
    public:
        CPURaytracer(const unsigned int width, const unsigned int height, const unsigned int antialiasing, const unsigned int s_shapec, const Shape* s_shapev) : AbstractRayTracer(width, height, antialiasing), S_SHAPEC(s_shapec), S_SHAPEV(s_shapev) {
            this->_bytes = new unsigned char[this->WIDTH*this->HEIGHT*AbstractRayTracer::CHANNELS];
        }

        virtual ~CPURaytracer() override {
            delete[] this->_bytes;
        }

        Intersection intersectRay(const Ray& ray) const {
            Intersection closestIntersection = Intersection::invalid();
            for(const Shape* shape = this->S_SHAPEV; shape < this->S_SHAPEV + this->S_SHAPEC; shape++) {
                Intersection intersection = shape->intersect(ray);
                if(intersection.is_valid() && (!closestIntersection.is_valid() || mm::vec3::distance(intersection.pos, ray.origin) < mm::vec3::distance(closestIntersection.pos, ray.origin))) {
                    closestIntersection = intersection;
                }
            }
            return closestIntersection;
        }

        float intersectShadowRay(const mm::vec3& pos, const Light& light) const {
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
                            
                            const Intersection intersection = this->intersectRay(Ray(camera.eye_pos, mm::vec3::normalize(localZ + x*localX - y*localY)));

                            // TODO: replace this if statement with shade() once the basics are working
                            if(intersection.is_valid()) {
                                color = (camera.eye_pos - intersection.pos).length()/10.0f*intersection.material->diffuse;
                            }
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