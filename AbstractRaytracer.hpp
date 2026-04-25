#pragma once

#include "include/NonCopyable.hpp"

class AbstractRayTracer : NonCopyable {
    public:
        const unsigned int WIDTH, HEIGHT;
        static const unsigned int CHANNELS = 4; /// RGBA
        static const unsigned int ITERATIONS = 10; /// RGBA
        const unsigned int ANTIALIASING_LEVEL;
        AbstractRayTracer(const unsigned int width, const unsigned int height, const unsigned int antialiasing) : WIDTH(width), HEIGHT(height), ANTIALIASING_LEVEL(antialiasing) {}
        virtual ~AbstractRayTracer() = default;
        [[nodiscard]] virtual const unsigned char* run(const Camera& camera) = 0;
};