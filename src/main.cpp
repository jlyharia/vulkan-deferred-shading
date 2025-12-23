#include <cstdio>
#include <cstdlib>
#include <exception>

#include "app/App.hpp"

int main() {
    App app(800, 600, "Vulkan Deferred Renderer");

    try {
        app.run();
    } catch (const std::exception &e) {
        std::fprintf(stderr, "Error: %s\n", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
