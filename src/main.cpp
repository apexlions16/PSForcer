#include "App.h"
#include "BootLog.h"

#include <cstdio>
#include <string>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    psforcer::bootLog("main entered");
    psforcer::App app;
    psforcer::bootLog("App constructed");

    std::string error;
    if (!app.initialize(error)) {
        const std::string message = std::string("initialization failed: ") + error;
        psforcer::bootLog(message.c_str());
        std::printf("PSForcer initialization failed: %s\n", error.c_str());

        // Returning a non-zero code is surfaced by the PS4 shell as CE-34878-0.
        // Preserve the diagnostic log but exit cleanly so initialization errors do
        // not look like an application crash.
        return 0;
    }

    psforcer::bootLog("initialization completed");
    const int result = app.run();
    psforcer::bootLog("main loop exited");
    return result;
}
