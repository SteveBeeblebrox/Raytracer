#pragma once

#include "include/NonCopyable.hpp"

#include "Types.hpp"

/// A base class for our CPU and GPU raytracers
class AbstractRayTracer : NonCopyable {
    protected:
        /// Row major pixel data
        unsigned char* _bytes;
    public:
        static const unsigned int CHANNELS = 4; /// RGBA
        static const unsigned int ITERATIONS = 3; // Max number of reflections in reflections before recursion gets cut off

        const unsigned int WIDTH, HEIGHT;

        // AA reduces sharp pixel lines on the edge of objects by averaging out multiple rays per pixel
        const struct {
            const unsigned int LEVEL; // Kept for debug info, otherwise use derived values
            const float SAMPLES; // Samples is (level+1)**2, so 0 -> 1 sample, 1 -> 2 samples, 2 -> 9 samples
            const float STRIDE; // Space between aa rays
        } ANTIALIASING;
        
        AbstractRayTracer(const unsigned int width, const unsigned int height, const unsigned int antialiasing) : 
            WIDTH(width), HEIGHT(height),
            ANTIALIASING { antialiasing, (float) ((antialiasing + 1)*(antialiasing + 1)), 1.0f/((float) antialiasing + 1.0f)}
        {
            this->_bytes = new unsigned char[this->size()];
        }

        virtual ~AbstractRayTracer() {
            delete[] this->_bytes;
        }

        /// Derived classes override this, local vectors form coordinate system for screen relative to view
        virtual void run(const Camera& camera, const mm::vec3& localX, const mm::vec3& localY, const mm::vec3& localZ) = 0;
        
        void run(const Camera& camera) {
            // Compute the camera ray for the given (x,y) image pixel
            // See http://www.unknownroad.com/rtfm/graphics/rt_eyerays.html
            const mm::vec3
                localZ = mm::vec3::normalize(camera.look_pos - camera.eye_pos),
                localX = mm::vec3::normalize(mm::vec3::cross(localZ,  1.0f - mm::abs(mm::vec3::dot(localZ, mm::vec3(0.0f, 1.0f, 0.0f))) < 1e-3 ? mm::vec3(1.0f, 0.0f, 0.0f) : mm::vec3(0.0f, 1.0f, 0.0f))),
                localY = mm::vec3::normalize(mm::vec3::cross(localX, localZ))
            ;

            this->run(camera, localX, localY, localZ);
        }

        /// Gets the raw image bytes, invalid if not `run()` at least once
        [[nodiscard]] virtual const unsigned char* bytes() const final {
            return this->_bytes;
        }

        /// Gets the size of `bytes()`
        [[nodiscard]] virtual unsigned int size() const final {
            return this->WIDTH*this->HEIGHT*AbstractRayTracer::CHANNELS;
        }
};