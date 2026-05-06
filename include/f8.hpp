#ifndef F8_HPP
#define F8_HPP

#include <stdint.h>
#include <ctime>
#include <cstddef>
#include <string>

/// F8 ("Fate")
/// (c) Trin Wasinger 2025
///
/// F8 is a machine independent PRNG library with support for stateful
/// random numbers via xorshift32.
///
/// Partially based on https://thebookofshaders.com/13/
///
/// NOTE: F8's global state is per include!
///
/// NOTE: This is a stripped down version of F8 that does not depend on GLM but lacks some features.
///       (See https://github.com/trinslaptop/Minceraft/blob/main/include/f8.hpp for the original) 
namespace f8 {
    namespace {
        uint32_t _state = 0x80801;
    }

    /// Get a random uint32_t
    inline uint32_t rand(uint32_t& state = _state) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    /// Get a random integer in [min, max)
    inline uint32_t randi(const int min, const int max, uint32_t& state = _state) {
        return rand(state) % (max - min) + min;
    }

    /// Get a random float in [0.0f, 1.0f)
    inline float randf(uint32_t& state = _state) {
        return rand(state) / (float) ((unsigned long) 1 << 32);
    }

    /// Get a random boolean with an optional probability
    inline bool randb(const float probability = 0.5f, uint32_t& state = _state) {
        return rand(state) < probability*((double) ((unsigned long) 1 << 32) + 1.0);
    }

    /// Generates a random version 4 UUID
    inline std::string uuid4(uint32_t& state = _state) {
        const char ALPHABET[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
        std::string t = "xxxxxxxx-xxxx-4xxx-Nxxx-xxxxxxxxxxxx";

        for(size_t i = 0; i < t.length(); i++) {
            if(t[i] == 'x') t[i] = ALPHABET[rand(state) % 16];
            else if(t[i] == 'N') t[i] = ALPHABET[0b1000 | (rand(state) % 4)];
        }

        return t;
    }

    /// A simple hash function, not cryptographically secure
	inline size_t cyrb(const std::string& text, size_t seed = 0) {
		size_t h1 = 0xdeadbeef ^ seed, h2 = 0x41c6ce57 ^ seed;
		for (size_t i = 0; i < text.length(); i++) {
			char ch = text.at(i);
			h1 = (h1 ^ ch)*2654435761;
			h2 = (h2 ^ ch)*1597334677;
		}
		h1 = ((h1 ^ (h1>>16))*2246822507) ^ ((h2 ^ (h2>>13))*3266489909);
		h2 = ((h2 ^ (h2>>16))*2246822507) ^ ((h1 ^ (h1>>13))*3266489909);
		return (h2 << 16) | h1;
	}

    /// Sets the RNG seed to the given value or the current time if argument is absent (or 0)
    /// and returns the new seed, also cycles the generator a few times
    inline unsigned int srand(const unsigned int seed = 0, uint32_t& state = _state) {
        state = seed ? seed : time(NULL);
        
        for(size_t i = 0; i < 16; i++) {
            rand(state);
        }
        
        return seed;
    }
}

#endif