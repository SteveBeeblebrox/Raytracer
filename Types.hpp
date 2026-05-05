#pragma once

#include "include/mm.hpp"

struct Ray final {
    mm::vec3 direction;
    mm::vec3 origin;
    __host__ __device__ inline Ray(const mm::vec3 _origin, const mm::vec3 _direction, float _step = 0.001f) : direction(_direction.normalize()), origin(_origin + direction*_step) {} // Offset slightly to avoid rounding issues
};

struct Camera final {
    mm::vec3 eye_pos;
    mm::vec3 look_pos = {0.0f, 0.0f, 0.0f};
    mm::vec3 up_vector = {0.0f, 1.0f, 0.0f};
    float vfov = mm::PI/4.0f;
};

struct Material final {
    mm::vec3 diffuse = {1.0f, 1.0f, 1.0f};
    mm::vec3 ambient = {0.2f, 0.2f, 0.2f};
    mm::vec3 specular = {0.0f, 0.0f, 0.0f};
    float shininess = 0.0f;
    float reflectivity = 0.0f;
    // See https://en.wikipedia.org/wiki/List_of_refractive_indices
    float indexOfRefraction = 1.0f;
    float alpha = 1.0f;
};

struct Intersection;

struct Shape final {
    enum {PLANE, SPHERE} type;
    Material material;
    union {
        struct {
            mm::vec3 pos;
            float radius;
        } as_sphere;
        struct {
            mm::vec3 pos;
            mm::vec3 normal;
        } as_plane;
    };

    __host__ __device__ [[nodiscard]] inline Intersection intersect(const Ray& ray) const;
};

struct Intersection final {
    mm::vec3 pos;
    mm::vec3 normal;
    const Material* material = nullptr;

    __host__ __device__ [[nodiscard]] inline bool is_valid() const {
        return this->material != nullptr;
    }

    __host__ __device__ [[nodiscard]] static inline Intersection invalid() {
        return Intersection {};
    }

    __host__ __device__ [[nodiscard]] static inline Intersection valid(const mm::vec3& pos, const mm::vec3& normal, const Material* material) {
        return Intersection {pos, normal, material};
    }

    __host__ __device__ [[nodiscard]] static Intersection closest(const mm::vec3& pos, const Intersection& a, const Intersection& b) {
        return a.is_valid() && (!b.is_valid() || mm::vec3::distance(a.pos, pos) < mm::vec3::distance(b.pos, pos)) ? a : b;
    }

    __host__ __device__ [[nodiscard]] static Intersection of(const Ray& ray, const unsigned int shapec, const Shape* shapev) {
        Intersection intersection = Intersection::invalid();
        for(const Shape* shape = shapev; shape < shapev + shapec; shape++) {
            intersection = Intersection::closest(ray.origin, shape->intersect(ray), intersection);
        }
        return intersection;
    }
};

struct Light final {
    mm::vec3 pos = {5.0f, 5.0f, 5.0f};
    mm::vec3 diffuse = {1.0f, 1.0f, 1.0f};
    mm::vec3 ambient = {1.0f, 1.0f, 1.0f};
    mm::vec3 specular = {1.0f, 1.0f, 1.0f};

    // Note: This function has room for some visual improvements
    // - Occlusion isn't actually just summing the alpha between multiple objects, maybe use multiply & add?
    // - Track and return color value instead of just percentage (e.g. stained for glass shadows)
    // Both of these may run into the alpha ordering problem though
    __host__ __device__ [[nodiscard]] static inline float transmission(const mm::vec3& pos, const Light& light, const unsigned int shapec, const Shape* shapev) {
        // With only solid objects, this could return a boolean if obstructed, but for transparent objects, we need to track how much light was obstructed
        // Returns percent transmission in [0,1]
        float alpha = 0.0f;

        const mm::vec3 L = mm::vec3::normalize(light.pos - pos);
        
        const Ray ray = Ray(pos, L);
        for(const Shape* shape = shapev; shape < shapev + shapec; shape++) {
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

    __host__ __device__ [[nodiscard]] static inline float netTransmission(const float t1, const float t2) {
        // See notes on transmission() about realism of just using addition
        // If t1 has 80% transmission and t2 has 55% transmission, then we want 1.0f - clamp((1.0f - 0.8f) + (1.0f - 0.55f), 0.0f, 1.0f) = 0.35
        // return 1.0f - mm::clamp((1.0f - t1) + (1.0f - t2), 0.0f, 1.0f);
        //                                        1                2 - a - b <= 0
        // 1 - clamp((1 - a) + (1 - b), 0, 1) = { 0           if   2 - a - b >= 1      = max(a + b - 1, 0) since a + b <= 2
        //                                        a + b - 1        0 < 2 - a - b <  1
        return mm::max(t1 + t2 - 1.0f, 0.0f);
    }
};

__host__ __device__ inline Intersection Shape::intersect(const Ray& ray) const {
    switch(this->type) {
        case PLANE: {
            float det, t;
            if((det = mm::vec3::dot(ray.direction, this->as_plane.normal)) == 0) {
                // If origin is on plane
                if(mm::vec3::dot(ray.origin - this->as_plane.pos, this->as_plane.normal) == 0) {
                    return Intersection::valid(ray.origin, this->as_plane.normal, &this->material);
                }
            } else if((t = mm::vec3::dot(this->as_plane.pos - ray.origin, this->as_plane.normal/det)) >= 0.0f) {
                    return Intersection::valid(ray.origin + t*ray.direction, this->as_plane.normal, &this->material);
            }
            return Intersection::invalid();
        }
        case SPHERE: {
            const mm::vec3 x = ray.origin - this->as_sphere.pos;
            const float a = mm::vec3::dot(ray.direction, ray.direction), b = 2.0f*mm::vec3::dot(x, ray.direction), c = mm::vec3::dot(x, x) - this->as_sphere.radius*this->as_sphere.radius;
            
            if(2.0f*a > 0.0f) {
                const float t1 = (-b + mm::sqrt(b*b - 4*a*c))/(2.0f*a), t2 = (-b - mm::sqrt(b*b - 4*a*c))/(2.0f*a);

                if(t1 > 0.0f && t2 > 0.0f) {
                    // If both positive, return smaller
                    const mm::vec3 p = ray.origin + mm::min(t1, t2)*ray.direction;
                    return Intersection::valid(p, mm::vec3::normalize(p - this->as_sphere.pos), &this->material);
                } else if(t1*t2 < 0.0f) {
                    // If one is negative, return the other
                    const mm::vec3 p = ray.origin + mm::max(t1, t2)*ray.direction;
                    return Intersection::valid(p, mm::vec3::normalize(p - this->as_sphere.pos), &this->material);
                }
            }
            return Intersection::invalid();
        }
    }
    return Intersection::invalid();
}
