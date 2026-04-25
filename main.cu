//$(which echo) -n; exec make run -- "$@"
#include <iostream>
#include <sstream>
#include <ios>
#include <string>
#include <vector>
#include <stdexcept>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "include/stb_image_write.h"
#include "include/json.hpp"
#include "include/mm.hpp"

#include "util.hpp"
#include "Types.hpp"
#include "CPURaytracer.hpp"

int main(int argc, const char** argv) {
    if(argc > 1) {
        json::ValueT config = json::parse(util::cat(argv[1]));

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
        
        auto raytracer = CPURaytracer(json::get<json::NumberT>(config, "resolution[0]", 1920.0f), json::get<json::NumberT>(config, "resolution[1]", 1080.0f), json::get<json::NumberT>(config, "antialiasing", 1.0f), shapes.size(), shapes.data(), lights.size(), lights.data());

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

        std::cout << "\033[96;1m>\033[0m ";
        for (std::string line; std::getline(std::cin, line); std::cout << "\033[96;1m>\033[0m ") {
            line = util::string::trim(line);
            if(line.empty()) {
                continue;
            } else if(line == "?" || line == "h" || line == "help") {
                std::cout << util::string::trim(R"(
?/h/help       - Show help message
q/quit/exit    - Exit repl
s/stat         - Show scene debug info
r/run          - Render frame to 'render.png'
tp <x> <y> <z> - Move the arcball camera to x, y, z
                )") << std::endl;
            } else if(line == "q" || line == "quit" || line == "exit") {
                break;
            } else if(line == "s" || line == "stat") {
                std::cout << "Static Shapes:      " << shapes.size() << std::endl;
                std::cout << "Lights:             " << lights.size() << std::endl;
                std::cout << "Resolution:         " << raytracer.WIDTH << "x" << raytracer.HEIGHT << std::endl;
                std::cout << "Antialiasing Level: " << raytracer.ANTIALIASING_LEVEL << " (" << (raytracer.ANTIALIASING_LEVEL + 1)*(raytracer.ANTIALIASING_LEVEL + 1) << " samples)" << std::endl;
                std::cout << "Camera Pos:         " << camera.eye_pos.x() << ", " << camera.eye_pos.y() << ", " << camera.eye_pos.z() << std::endl;
            
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
                std::cout << "Rendering..." << std::endl;
                const unsigned char* const bytes = raytracer.run(camera);

                if(!stbi_write_png("render.png", raytracer.WIDTH, raytracer.HEIGHT, AbstractRayTracer::CHANNELS, bytes, AbstractRayTracer::CHANNELS*raytracer.WIDTH)) {
                    util::error("Unable to write to '%s'", "render.png");
                }
            } else {
                util::error("Unknown command '%s'!", line);
            }
        }
    } else {
        util::error("Expected 2 arguments, got %d!", argc);
        return 1;
    }

    return 0;
}