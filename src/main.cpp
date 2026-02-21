#include <cstdio>
#include <cstdlib>
#include <exception>

#include "app/app.hpp"
#include "common/config.hpp"

int main() {
    App app(engineConfig::MAIN_WINDOW_WIDTH, engineConfig::MAIN_WINDOW_HEIGHT, "Vulkan Deferred Renderer");

    try {
        app.run();
    } catch (const std::exception &e) {
        std::fprintf(stderr, "Error: %s\n", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}