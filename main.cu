//$(which echo) -n; exec make run -- "$@"
#include <iostream>
#include <sstream>
#include <ios>
#include <string>
#include <vector>
#include <stdexcept>
#include <memory>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "include/stb_image_write.h"
#include "include/json.hpp"
#include "include/mm.hpp"

#include "util.hpp"
#include "Types.hpp"
#include "CPURaytracer.hpp"
#include "GPURaytracer.hpp"

int main(int argc, const char** argv) {
    const std::string HELP_TEXT = std::string(util::string::trim(R"(
Usage: ./raytracer config.json runtime.json
       ./raytracer --help

REPL commands:
?/h/help       - Show help message
q/quit/exit    - Exit repl
s/stat         - Show scene debug info
r/run          - Render frame to 'render.png'
tp <x> <y> <z> - Move the arcball camera to x, y, z

config.json:
```
{
    "schema": "scene", 
    "resolution": [int, int]          (Default 1920x1080),
    "antialiasing": int               (Default 1, 2**(n+1) samples per pixel),
    "camera": {
        "pos": [float, float, float],
        "fov": float                  (Default 45, in degrees)
    },
    "scene": [
        {
            "type": "sphere",
            "pos": [float, float, float],
            "radius": float,
            "material": {
                "diffuse": [float, float, float],
                "ambient": [float, float, float],
                "specular": [float, float, float],
                "shininess": float,
                "reflectivity": float,
                "refraction": float,
                "alpha": float
            }
        },
        {
            "type": "plane",
            "pos": [float, float, float],
            "normal": [float, float, float],
            "material": {
                ...
            }
        },
        {
            "type": "light",
            "pos": [float, float, float],
            "diffuse": [float, float, float],
            "ambient": [float, float, float],
            "specular": [float, float, float]
        },
        ...
    ]
}
```

runtime.json:
```
{
    "schema": "runtime",
    "target": "gpu" | "cpu" (Default "cpu"),
    "gpu_tweaks": {
        "block_width":       int (Default 16),
        "block_height":      int (Default 16),
        "partition_objects": bool (Default false),
        "buffer_objects":    bool (Default false),
        "layered":           bool (Default false)
    }
}
```
    )"));

    if(argc > 1 && (argv[1] == std::string("?") || argv[1] == std::string("-h") || argv[1] == std::string("--help"))) {
        std::cerr << HELP_TEXT << std::endl;
        return 0;
    }

    if(argc == 3) {
        json::ValueT config = json::parse(util::cat(argv[1]));
        json::ValueT runtime = json::parse(util::cat(argv[2]));

        if(json::get<json::StringT>(config, "schema", "") != "scene") {
            util::error("First json has schema '%s', expected 'scene' (Do you need to swap the order of arguments?)!", json::get<json::StringT>(config, "schema", ""));
            return 2;
        } 

        if(json::get<json::StringT>(runtime, "schema", "") != "runtime") {
            util::error("Second json has schema '%s', expected 'runtime'!", json::get<json::StringT>(runtime, "schema", ""));
            return 2;
        } 

        std::vector<Shape> shapes;
        std::vector<Light> lights;

        for(const json::ValueT& entry : json::get<json::ListT>(config, "scene", std::vector<json::ValueT>())) {
            Shape shape = {};
            const std::string type = json::get<json::StringT>(entry, "type");
            if(util::string::starts_with(type, "_")) {
                continue;
            } else if(type == "light") {
                lights.push_back(Light {
                    .pos = mm::vec3(json::get<json::NumberT>(entry, "pos[0]", 5.0f), json::get<json::NumberT>(entry, "pos[1]", 5.0f), json::get<json::NumberT>(entry, "pos[2]", 5.0f)),
                    .diffuse = mm::vec3(json::get<json::NumberT>(entry, "diffuse[0]", 1.0f), json::get<json::NumberT>(entry, "diffuse[1]", 1.0f), json::get<json::NumberT>(entry, "diffuse[2]", 1.0f)),
                    .ambient = mm::vec3(json::get<json::NumberT>(entry, "ambient[0]", 1.0f), json::get<json::NumberT>(entry, "ambient[1]", 1.0f), json::get<json::NumberT>(entry, "ambient[2]", 1.0f)),
                    .specular = mm::vec3(json::get<json::NumberT>(entry, "specular[0]", 1.0f), json::get<json::NumberT>(entry, "specular[1]", 01.0f), json::get<json::NumberT>(entry, "specular[2]", 1.0f))
                });
                continue;
            } else if(type == "sphere") {
                shape.type = Shape::SPHERE;
                shape.as_sphere.pos = mm::vec3(json::get<json::NumberT>(entry, "pos[0]", 0.0f), json::get<json::NumberT>(entry, "pos[1]", 0.0f), json::get<json::NumberT>(entry, "pos[2]", 0.0f));
                shape.as_sphere.radius = json::get<json::NumberT>(entry, "radius", 1.0f);
            } else if(type == "plane") {
                shape.type = Shape::PLANE;
                shape.as_plane.pos = mm::vec3(json::get<json::NumberT>(entry, "pos[0]", 0.0f), json::get<json::NumberT>(entry, "pos[1]", 0.0f), json::get<json::NumberT>(entry, "pos[2]", 0.0f));
                shape.as_plane.normal = mm::vec3(json::get<json::NumberT>(entry, "normal[0]", 0.0f), json::get<json::NumberT>(entry, "normal[1]", 1.0f), json::get<json::NumberT>(entry, "normal[2]", 0.0f));
            } else {
                util::warn("Shape type must be either 'sphere', 'plane', or 'light', got '%s'", type);
            }

            shape.material.diffuse = mm::vec3(json::get<json::NumberT>(entry, "material.diffuse[0]", 1.0f), json::get<json::NumberT>(entry, "material.diffuse[1]", 1.0f), json::get<json::NumberT>(entry, "material.diffuse[2]", 1.0f));
            shape.material.ambient = mm::vec3(json::get<json::NumberT>(entry, "material.ambient[0]", 0.2f), json::get<json::NumberT>(entry, "material.ambient[1]", 0.2f), json::get<json::NumberT>(entry, "material.ambient[2]", 0.2f));
            shape.material.specular = mm::vec3(json::get<json::NumberT>(entry, "material.specular[0]", 0.0f), json::get<json::NumberT>(entry, "material.specular[1]", 0.0f), json::get<json::NumberT>(entry, "material.specular[2]", 0.0f));
            shape.material.shininess = json::get<json::NumberT>(entry, "material.shininess", 0.0f);
            shape.material.reflectivity = json::get<json::NumberT>(entry, "material.reflectivity", 0.0f);
            shape.material.indexOfRefraction = json::get<json::NumberT>(entry, "material.refraction", 1.0f);
            shape.material.alpha = json::get<json::NumberT>(entry, "material.alpha", 1.0f);

            shapes.push_back(shape);
        }
        
        const float width = json::get<json::NumberT>(config, "resolution[0]", 1920.0f), height = json::get<json::NumberT>(config, "resolution[1]", 1080.0f);
        const unsigned int aa_level = json::get<json::NumberT>(config, "antialiasing", 1.0f);
        const std::string target = json::get<json::StringT>(runtime, "target", "cpu");
        const unsigned int block_width = json::get<json::NumberT>(config, "gpu_tweaks.block_width", 16.0f), block_height = json::get<json::NumberT>(config, "gpu_tweaks.block_height", 16.0f);

        std::unique_ptr<AbstractRayTracer> raytracer = nullptr;
        
        if(target == "gpu") {
            raytracer = std::make_unique<GPURaytracer>(width, height, aa_level, shapes.size(), shapes.data(), lights.size(), lights.data(), block_width, block_height);
        } else {
            if(target != "cpu") {
                util::warn("Unknown runtime target '%s' (Use 'cpu' or 'gpu')", target);
            }
            raytracer = std::make_unique<CPURaytracer>(width, height, aa_level, shapes.size(), shapes.data(), lights.size(), lights.data());
        }

        Camera camera = {
            .eye_pos = mm::vec3(
                json::get<json::NumberT>(config, "camera.pos[0]", 5.0f),
                json::get<json::NumberT>(config, "camera.pos[1]", 1.0f),
                json::get<json::NumberT>(config, "camera.pos[2]", 3.0f)
            ),
            .vfov = (float) json::get<json::NumberT>(config, "camera.fov", 45.0f)*mm::PI/180.0f
        };

        if(lights.size() == 0) {
            util::warn("No lights defined in '%s'!", argv[1]);
        }

        std::cerr << "\033[96;1m>\033[0m ";
        for (std::string line; std::getline(std::cin, line); std::cerr << "\033[96;1m>\033[0m ") {
            line = util::string::trim(line);
            if(line.empty()) {
                continue;
            } else if(line == "?" || line == "h" || line == "help") {
                std::cerr << HELP_TEXT << std::endl;
            } else if(line == "q" || line == "quit" || line == "exit") {
                break;
            } else if(line == "s" || line == "stat") {
                std::cerr << "Target:             " << target << std::endl;
                if(target == "gpu") {
                    std::cerr << "Block Size:         " << block_width << "x" << block_height << std::endl;
                } else {
                    std::cerr << "Block Size:         N/A" << std::endl;
                }
                std::cerr << "Static Shapes:      " << shapes.size() << std::endl;
                std::cerr << "Lights:             " << lights.size() << std::endl;
                std::cerr << "Resolution:         " << raytracer->WIDTH << "x" << raytracer->HEIGHT << std::endl;
                std::cerr << "Antialiasing Level: " << raytracer->ANTIALIASING.LEVEL << " (" << (raytracer->ANTIALIASING.SAMPLES) << " samples)" << std::endl;
                std::cerr << "Camera Pos:         " << camera.eye_pos.x() << ", " << camera.eye_pos.y() << ", " << camera.eye_pos.z() << std::endl;
            
            } else if(util::string::starts_with(line, "tp ")) {
                std::stringstream stream(line.substr(3));
                float x, y, z;
                stream >> x >> y >> z;
                if(stream.fail()) {
                    util::error("Unable to parse position from '%s'", line.substr(3));
                } else {
                    camera.eye_pos = mm::vec3(x, y, z);
                }
            } else if(line == "r" || line == "run") {
                std::cerr << "Rendering..." << std::endl;
                raytracer->run(camera);

                if(!stbi_write_png("render.png", raytracer->WIDTH, raytracer->HEIGHT, AbstractRayTracer::CHANNELS, raytracer->bytes(), AbstractRayTracer::CHANNELS*raytracer->WIDTH)) {
                    util::error("Unable to write to '%s'", "render.png");
                }
            } else {
                util::error("Unknown command '%s'!", line);
            }
        }
    } else {
        util::error("Expected 2 arguments (`./raytracer config.json runtime.json`), got %d!", argc);
        return 1;
    }

    return 0;
}