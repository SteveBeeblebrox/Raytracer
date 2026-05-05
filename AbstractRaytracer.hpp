#pragma once

#include "include/NonCopyable.hpp"

#include "Types.hpp"

class AbstractRayTracer : NonCopyable {
    protected:
        unsigned char* _bytes;
    public:
        static const unsigned int CHANNELS = 4; /// RGBA
        static const unsigned int ITERATIONS = 10;

        const unsigned int WIDTH, HEIGHT;

        const struct {
            const unsigned int LEVEL; // Kept for debug info, otherwise use derived values
            const float SAMPLES;
            const float STRIDE;
        } ANTIALIASING;
        
        AbstractRayTracer(const unsigned int width, const unsigned int height, const unsigned int antialiasing) : 
            WIDTH(width), HEIGHT(height),
            ANTIALIASING { antialiasing, (antialiasing + 1)*(antialiasing + 1), 1.0f/((float) antialiasing + 1.0f)}
        {
            this->_bytes = new unsigned char[this->WIDTH*this->HEIGHT*AbstractRayTracer::CHANNELS];
        }

        virtual ~AbstractRayTracer() {
            delete[] this->_bytes;
        }

        [[nodiscard]] virtual const unsigned char* run(const Camera& camera) = 0;

        [[nodiscard]] virtual const unsigned char* bytes() const {
            return this->_bytes;
        }
};