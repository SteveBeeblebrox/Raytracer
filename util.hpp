#pragma once

#include <string>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <cstdio>
#include <utility>
#include <type_traits>
#include <string_view>
#include <algorithm>
#include <cctype>
#include <functional>

/// Assorted utility functions that aren't directly related to ray tracing or CUDA 
namespace util {
    /// Read a file into a string (not ideal for large files!)
    [[nodiscard]] inline std::string cat(const char* path) {
        std::ifstream istream(path);
        istream.exceptions(std::ios_base::badbit);
        std::ostringstream ostream;
        ostream << istream.rdbuf();
        return ostream.str();
    }

    namespace string {
        /// Checks is `string` ends with `suffix`
        /// Based on https://stackoverflow.com/a/42844629
        [[nodiscard]] constexpr inline bool ends_with(std::string_view string, std::string_view suffix) {
            return string.size() >= suffix.size() && string.compare(string.size() - suffix.size(), suffix.size(), suffix) == 0;
        }

        /// Checks is `string` starts with `prefix`
        /// Based on https://stackoverflow.com/a/42844629
        [[nodiscard]] constexpr inline bool starts_with(std::string_view string, std::string_view prefix) {
            return string.size() >= prefix.size() && string.compare(0, prefix.size(), prefix) == 0;
        }

        /// C++20 adds a constructor for making std::string_view out of iterators, this is a polyfill
        [[nodiscard]] constexpr inline std::string_view iter_range_to_string_view(std::string_view::iterator first, std::string_view::iterator last) {
            return first != last ? std::string_view(&*first, static_cast<size_t>(last - first)) : std::string_view(nullptr, 0);
        }

        /// Removes chars matching `predicate` from the left side of `string`
        /// Based on https://gist.github.com/dk949/78dd944ea2b9763eb61cc5a652c35900
        template<typename T = bool(*)(char)> [[nodiscard]] constexpr inline std::string_view trim_left(std::string_view string, T predicate = [](char a) { return (bool) std::isspace(a); }) {
            return iter_range_to_string_view(std::find_if(string.begin(), string.end(), std::not_fn(predicate)), string.end());
        }

        /// Removes chars matching `predicate` from the right side of `string`
        /// Based on https://gist.github.com/dk949/78dd944ea2b9763eb61cc5a652c35900
        template<typename T = bool(*)(char)> [[nodiscard]] constexpr inline std::string_view trim_right(std::string_view string, T predicate = [](char a) { return (bool) std::isspace(a); }) {
            return iter_range_to_string_view(string.begin(), std::find_if(string.rbegin(), string.rend(), std::not_fn(predicate)).base());
        }

        /// Removes chars matching `predicate` from the both sides of `string`
        /// Based on https://gist.github.com/dk949/78dd944ea2b9763eb61cc5a652c35900
        template<typename T = bool(*)(char)> [[nodiscard]] constexpr inline std::string_view trim(std::string_view string, T predicate = [](char a) { return (bool) std::isspace(a); }) {
            return trim_right(trim_left(string, predicate), predicate);
        }

        namespace {
            /// With %s, snprintf expects a const char* not a std::string, so remap it
            template<typename T> [[nodiscard]] inline auto format_value(T&& t) {
                if constexpr(std::is_same<std::decay_t<T>, std::string>::value) {
                    return std::forward<T>(t).c_str();
                } else {
                    return std::forward<T>(t);
                }
            }
        }

        /// Format a string using standard printf modifiers like %s and %d
        template<typename... Args> [[nodiscard, gnu::format(printf, 1, 0)]] inline std::string format(const char* format, Args&&... args) {
            const int n = std::snprintf(nullptr, 0, format, format_value(std::forward<Args>(args))...) + 1;
            
            if(n <= 0) {
                throw std::runtime_error("Bad format string");
            }

            std::string buffer(n, '\0');
            std::snprintf(buffer.data(), buffer.size(), format, format_value(std::forward<Args>(args))...);
            buffer.resize(buffer.size() - 1);

            return buffer;
        }
    }

    /// Prints a formatted error message to stderr in red
    template<typename... Args> inline void error(const char* message, Args... args) {
        std::cerr << "\033[91;1m[ERROR]\033[0m: " << util::string::format(message, args...) << std::endl;
    }

    /// Prints a formatted error message to stderr in yellow
    template<typename... Args> inline void warn(const char* message, Args... args) {
        std::cerr << "\033[93;1m[WARNING]\033[0m: " << util::string::format(message, args...) << std::endl;
    }
}