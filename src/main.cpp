#include "App.h"
#include "BootLog.h"

#include <cstdio>
#include <string>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    psforcer::bootLog("Ana işlem başladı");
    psforcer::App app;
    psforcer::bootLog("Uygulama oluşturuldu");

    std::string error;
    if (!app.initialize(error)) {
        const std::string message = std::string("Başlatma başarısız: ") + error;
        psforcer::bootLog(message.c_str());
        std::printf("PSForcer başlatılamadı: %s\n", error.c_str());
        return 0;
    }

    psforcer::bootLog("Başlatma tamamlandı");
    const int result = app.run();
    psforcer::bootLog("Ana döngü kapandı");
    return result;
}
