#pragma once

#include <cstdio>

#if defined(PSFORCER_ORBIS)
#include <orbis/libkernel.h>
#endif

namespace psforcer {

inline void bootLog(const char* message) {
    if (!message) return;
#if defined(PSFORCER_ORBIS)
    sceKernelDebugOutText(0, "[PSForcer] %s\n", message);
    FILE* file = std::fopen("/data/psforcer-startup.log", "a");
    if (file) {
        std::fprintf(file, "%s\n", message);
        std::fclose(file);
    }
#else
    std::fprintf(stderr, "[PSForcer] %s\n", message);
#endif
}

}  // namespace psforcer
