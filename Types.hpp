#pragma once

#include "include/mm.hpp"

struct Ray final {
    mm::vec3 direction;
    mm::vec3 origin;
    Ray(const mm::vec3 _origin, const mm::vec3 _direction, float _step = 0.001f) : direction(_direction.normalize()), origin(_origin + direction*_step) {} // Offset slightly to avoid rounding issues
};

struct Camera final {
    mm::vec3 eye_pos;
    mm::vec3 look_pos = {0.0f, 0.0f, 0.0f};
    mm::vec3 up_vector = {0.0f, 0.0f, 0.0f};
    float vfov = 45.0f;
    float near_clip_plane = 0.001f;
};

struct Material final {
    mm::vec3 diffuse = {1.0f, 1.0f, 1.0f};
    mm::vec3 ambient = {0.2f, 0.2f, 0.2f};
    mm::vec3 specular = {0.0f, 0.0f, 0.0f};
    float shininess = 0.0f;
    float reflectivity = 0.0f;
    float indexOfRefraction = 0.0f;
    float alpha = 1.0f;
};

struct Light final {
    mm::vec3 pos = {5.0f, 5.0f, 5.0f};
    mm::vec3 diffuse = {1.0f, 1.0f, 1.0f};
    mm::vec3 ambient = {1.0f, 1.0f, 1.0f};
    mm::vec3 specular = {1.0f, 1.0f, 1.0f};
};

struct Intersection final {
    mm::vec3 pos;
    mm::vec3 normal;
    const Material* material = nullptr;
    [[nodiscard]] inline bool is_valid() const {
        return this->material != nullptr;
    }

    [[nodiscard]] static inline Intersection invalid() {
        return Intersection {};
    }

    [[nodiscard]] static inline Intersection valid(const mm::vec3& pos, const mm::vec3& normal, const Material* material) {
        return Intersection {pos, normal, material};
    }
};

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

    [[nodiscard]] inline Intersection intersect(const Ray& ray) const {
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
};