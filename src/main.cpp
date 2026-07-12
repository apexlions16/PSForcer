#include "App.h"

#include <cstdio>
#include <string>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    psforcer::App app;
    std::string error;
    if (!app.initialize(error)) {
        std::printf("PSForcer initialization failed: %s\n", error.c_str());
        return 1;
    }
    return app.run();
}
