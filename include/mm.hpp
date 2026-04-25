#pragma once

#ifndef __CUDACC__
#define __host__
#define __device__
#endif

#ifdef __CUDA_ARCH__
#define CUDA_IMPL(HOST, DEVICE) DEVICE
#include <cuda_runtime.h>
#else
#define CUDA_IMPL(HOST, DEVICE) HOST
#include <cmath>
#include <algorithm>
#endif

/// A mini math library with support for CUDA
/// (c) Trin Wasinger 2026
namespace mm {
    inline constexpr float PI = 3.14159265358979323846f; 

    __host__ __device__ constexpr inline float sqrt(const float value) { return CUDA_IMPL(std::sqrt, sqrtf)(value); }
    __host__ __device__ constexpr inline float abs(const float value) { return CUDA_IMPL(std::abs, fabsf)(value); }
    __host__ __device__ constexpr inline float exp(const float value) { return CUDA_IMPL(std::exp, expf)(value); }

    __host__ __device__ constexpr inline float floor(const float value) { return CUDA_IMPL(std::floor, floorf)(value); }
    __host__ __device__ constexpr inline float ceil(const float value) { return CUDA_IMPL(std::ceil, ceilf)(value); }
    __host__ __device__ constexpr inline float round(const float value) { return CUDA_IMPL(std::round, roundf)(value); }

    __host__ __device__ constexpr inline float cos(const float value) { return CUDA_IMPL(std::cos, cosf)(value); }
    __host__ __device__ constexpr inline float acos(const float value) { return CUDA_IMPL(std::acos, acosf)(value); }
    
    __host__ __device__ constexpr inline float cosh(const float value) { return CUDA_IMPL(std::cosh, coshf)(value); }
    __host__ __device__ constexpr inline float acosh(const float value) { return CUDA_IMPL(std::acosh, acoshf)(value); }

    __host__ __device__ constexpr inline float sin(const float value) { return CUDA_IMPL(std::sin, sinf)(value); }
    __host__ __device__ constexpr inline float asin(const float value) { return CUDA_IMPL(std::asin, asinf)(value); }

    __host__ __device__ constexpr inline float sinh(const float value) { return CUDA_IMPL(std::sinh, sinhf)(value); }
    __host__ __device__ constexpr inline float asinh(const float value) { return CUDA_IMPL(std::asinh, asinhf)(value); }

    __host__ __device__ constexpr inline float tan(const float value) { return CUDA_IMPL(std::tan, tanf)(value); }
    __host__ __device__ constexpr inline float atan(const float value) { return CUDA_IMPL(std::atan, atanf)(value); }
    __host__ __device__ constexpr inline float atan2(const float y, const float x) { return CUDA_IMPL(std::atan2, atan2f)(y, x); }

    __host__ __device__ constexpr inline float tanh(const float value) { return CUDA_IMPL(std::tanh, tanhf)(value); }
    __host__ __device__ constexpr inline float atanh(const float value) { return CUDA_IMPL(std::atanh, atanhf)(value); }

    __host__ __device__ constexpr inline float log(const float value) { return CUDA_IMPL(std::log, logf)(value); }
    __host__ __device__ constexpr inline float log2(const float value) { return CUDA_IMPL(std::log2, log2f)(value); }
    __host__ __device__ constexpr inline float log10(const float value) { return CUDA_IMPL(std::log10, log10f)(value); }

    __host__ __device__ constexpr inline float min(const float a, const float b) { return CUDA_IMPL(std::min, fminf)(a, b); }
    __host__ __device__ constexpr inline float max(const float a, const float b) { return CUDA_IMPL(std::max, fmaxf)(a, b); }

    __host__ __device__ constexpr inline float clamp(const float value, const float min, const float max) { return CUDA_IMPL(std::clamp(value, min, max), fminf(fmaxf(value, min), max)); }

    struct vec3 final {
        private:
            float _data[3];
        public:
            __host__ __device__ constexpr inline vec3(float x, float y, float z) : _data {x, y, z} {}

            __host__ __device__ constexpr inline vec3(float v) : vec3(v, v, v) {}

            __host__ __device__ constexpr inline vec3() : vec3(0.0f) {}

            __host__ __device__ constexpr inline float x() const { return this->_data[0]; }
            __host__ __device__ constexpr inline float y() const { return this->_data[1]; }
            __host__ __device__ constexpr inline float z() const { return this->_data[2]; }
            
            __host__ __device__ constexpr inline float r() const { return this->_data[0]; }
            __host__ __device__ constexpr inline float g() const { return this->_data[1]; }
            __host__ __device__ constexpr inline float b() const { return this->_data[2]; }

            __host__ __device__ constexpr inline float u() const { return this->_data[0]; }
            __host__ __device__ constexpr inline float v() const { return this->_data[1]; }
            __host__ __device__ constexpr inline float w() const { return this->_data[2]; }

            __host__ __device__ constexpr inline vec3 operator+() const { return vec3(this->_data[0], this->_data[1], this->_data[2]); }
            __host__ __device__ constexpr inline vec3 operator-() const { return vec3(-this->_data[0], -this->_data[1], -this->_data[2]); }

            __host__ __device__ constexpr inline float operator[](int i) const { return this->_data[i]; }
            __host__ __device__ constexpr inline float& operator[](int i) { return this->_data[i]; }

            __host__ __device__ constexpr inline vec3& operator+=(const vec3& value) {
                this->_data[0] += value._data[0];
                this->_data[1] += value._data[1];
                this->_data[2] += value._data[2];
                return *this;
            }

            __host__ __device__ constexpr inline vec3& operator+=(const float value) {
                this->_data[0] += value;
                this->_data[1] += value;
                this->_data[2] += value;
                return *this;
            }

            __host__ __device__ constexpr inline vec3& operator-=(const vec3& value) {
                this->_data[0] -= value._data[0];
                this->_data[1] -= value._data[1];
                this->_data[2] -= value._data[2];
                return *this;
            }

            __host__ __device__ constexpr inline vec3& operator-=(const float value) {
                this->_data[0] -= value;
                this->_data[1] -= value;
                this->_data[2] -= value;
                return *this;
            }

            __host__ __device__ constexpr inline vec3& operator*=(const vec3& value) {
                this->_data[0] *= value._data[0];
                this->_data[1] *= value._data[1];
                this->_data[2] *= value._data[2];
                return *this;
            }

            __host__ __device__ constexpr inline vec3& operator*=(const float value) {
                this->_data[0] *= value;
                this->_data[1] *= value;
                this->_data[2] *= value;
                return *this;
            }

            __host__ __device__ constexpr inline vec3& operator/=(const vec3& value) {
                this->_data[0] /= value._data[0];
                this->_data[1] /= value._data[1];
                this->_data[2] /= value._data[2];
                return *this;
            }

            __host__ __device__ constexpr inline vec3& operator/=(const float value) {
                this->_data[0] /= value;
                this->_data[1] /= value;
                this->_data[2] /= value;
                return *this;
            }

            __host__ __device__ constexpr inline vec3 operator+(const vec3& value) const { return vec3(this->_data[0] + value._data[0], this->_data[1] + value._data[1], this->_data[2] + value._data[2]); }
            __host__ __device__ constexpr inline vec3 operator+(const float value) const { return vec3(this->_data[0] + value, this->_data[1] + value, this->_data[2] + value); }

            __host__ __device__ constexpr inline vec3 operator-(const vec3& value) const { return vec3(this->_data[0] - value._data[0], this->_data[1] - value._data[1], this->_data[2] - value._data[2]); }
            __host__ __device__ constexpr inline vec3 operator-(const float value) const { return vec3(this->_data[0] - value, this->_data[1] - value, this->_data[2] - value); }

            __host__ __device__ constexpr inline vec3 operator*(const vec3& value) const { return vec3(this->_data[0] * value._data[0], this->_data[1] * value._data[1], this->_data[2] * value._data[2]); }
            __host__ __device__ constexpr inline vec3 operator*(const float value) const { return vec3(this->_data[0] * value, this->_data[1] * value, this->_data[2] * value); }

            __host__ __device__ constexpr inline vec3 operator/(const vec3& value) const { return vec3(this->_data[0] / value._data[0], this->_data[1] / value._data[1], this->_data[2] / value._data[2]); }
            __host__ __device__ constexpr inline vec3 operator/(const float value) const { return vec3(this->_data[0] / value, this->_data[1] / value, this->_data[2] / value); }

            __host__ __device__ constexpr inline bool operator==(const vec3& value) const { return this->_data[0] == value._data[0] && this->_data[1] == value._data[1] && this->_data[2] == value._data[2]; }
            __host__ __device__ constexpr inline bool operator!=(const vec3& value) const { return this->_data[0] != value._data[0] || this->_data[1] != value._data[1] || this->_data[2] != value._data[2]; }

            __host__ __device__ constexpr inline float length() const {
                return CUDA_IMPL(std::sqrt, sqrtf)(this->_data[0]*this->_data[0] + this->_data[1]*this->_data[1] + this->_data[2]*this->_data[2]);
            }

            __host__ __device__ static constexpr inline vec3 normalize(const vec3& value) {
                return value.length() > 0.0f ? value/value.length() : value;
            }

            __host__ __device__ constexpr inline vec3 normalize() const {
                return vec3::normalize(*this);
            }
            
            __host__ __device__ static constexpr inline float dot(const vec3& a, const vec3& b) {
                return a._data[0]*b._data[0] + a._data[1]*b._data[1] + a._data[2]*b._data[2];
            }

            __host__ __device__ static constexpr inline vec3 cross(const vec3& a, const vec3& b) {
                return vec3(
                    a._data[1]*b._data[2] - a._data[2]*b._data[1],
                    a._data[2]*b._data[0] - a._data[0]*b._data[2],
                    a._data[0]*b._data[1] - a._data[1]*b._data[0]
                );
            }

            __host__ __device__ static constexpr inline float distance(const vec3& a, const vec3& b) {
                return CUDA_IMPL(std::sqrt, sqrtf)(
                    (a._data[0] - b._data[0])*(a._data[0] - b._data[0]) + (a._data[1] - b._data[1])*(a._data[1] - b._data[1]) + (a._data[2] - b._data[2])*(a._data[2] - b._data[2])
                );
            }
    };
}

#undef CUDA_IMPL