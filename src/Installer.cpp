#include "Installer.h"
#include "FileUtil.h"
#include "HttpClient.h"
#include "PkgHeader.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

#if defined(PSFORCER_ORBIS)
#include <orbis/AppInstUtil.h>
#include <orbis/Bgft.h>
#include <orbis/Sysmodule.h>
#include <orbis/UserService.h>
#endif

namespace psforcer {

namespace {
#if defined(PSFORCER_ORBIS)
std::string orbisError(const char* operation, int32_t result) {
    std::ostringstream message;
    message << operation << " başarısız oldu (0x"
            << std::uppercase << std::hex
            << static_cast<uint32_t>(result) << ')';
    return message.str();
}

void resetBgftDiagnostic() {
    FILE* file = std::fopen("/data/psforcer/bgft_tani.log", "wb");
    if (!file) return;
    std::fprintf(file, "surum=0.2.10 asama=istek_alindi\n");
    std::fflush(file);
    std::fclose(file);
}

void appendBgftDiagnostic(const char* stage, int32_t result = 0) {
    FILE* file = std::fopen("/data/psforcer/bgft_tani.log", "ab");
    if (!file) return;
    std::fprintf(file, "asama=%s sonuc=0x%08X\n",
                 stage ? stage : "bilinmiyor",
                 static_cast<uint32_t>(result));
    std::fflush(file);
    std::fclose(file);
}

int32_t loadInternalModule(OrbisSysModuleInternal module, const char* stage) {
    appendBgftDiagnostic(stage);
    // OpenOrbis bildirimi uint32_t olsa da PS4 hata kodları işaretli 32 bittir.
    const int32_t result = static_cast<int32_t>(
        sceSysmoduleLoadModuleInternal(module));
    const std::string completedStage = std::string(stage) + "_tamam";
    appendBgftDiagnostic(completedStage.c_str(), result);
    return result;
}
#endif
}

InstallOutcome ManualInstaller::requestInstall(const CatalogItem& item,
                                                const PackageInfo& package,
                                                const std::string& packagePath) {
    const std::string markerPath = packagePath + ".hazir";
    std::ofstream marker(markerPath.c_str(), std::ios::out | std::ios::trunc);
    if (!marker) return InstallOutcome(InstallResult::Failed, "Hazır işareti yazılamadı");
    marker << "icerik=" << item.id << "\n";
    marker << "paket=" << package.id << "\n";
    marker << "tur=" << packageKindName(package.kind) << "\n";
    marker << "surum=" << package.version << "\n";
    marker << "yol=" << packagePath << "\n";
    marker.close();
    return InstallOutcome(
        InstallResult::ReadyForManualInstall,
        "Paket doğrulandı ve yetkili kurucu için hazırlandı");
}

InstallOutcome ManualInstaller::requestRemoteInstall(const CatalogItem& item,
                                                      const PackageInfo& package) {
    (void)item;
    (void)package;
    return InstallOutcome(InstallResult::Failed,
                          "Doğrudan PS4 indirmesi yalnızca PS4 derlemesinde kullanılabilir");
}

OrbisInstaller::OrbisInstaller()
    : bgftHeap_(NULL), bgftInitialized_(false), appInstInitialized_(false),
      userServiceInitialized_(false), runtimeModulesLoaded_(false) {}

OrbisInstaller::~OrbisInstaller() {
#if defined(PSFORCER_ORBIS)
    if (bgftInitialized_) {
        sceBgftServiceIntTerm();
        bgftInitialized_ = false;
    }
    if (appInstInitialized_) {
        sceAppInstUtilTerminate();
        appInstInitialized_ = false;
    }
    if (userServiceInitialized_) {
        sceUserServiceTerminate();
        userServiceInitialized_ = false;
    }
#endif
    std::free(bgftHeap_);
    bgftHeap_ = NULL;
}

InstallOutcome OrbisInstaller::requestInstall(const CatalogItem& item,
                                               const PackageInfo& package,
                                               const std::string& packagePath) {
#if !defined(PSFORCER_ORBIS)
    (void)item;
    (void)package;
    (void)packagePath;
    return InstallOutcome(InstallResult::Failed,
                          "Otomatik PKG kurulumu yalnızca PS4 derlemesinde kullanılabilir");
#else
    const int32_t moduleResult = static_cast<int32_t>(
        sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_APP_INST_UTIL));
    if (moduleResult < 0) {
        return InstallOutcome(InstallResult::Failed,
                              orbisError("AppInstUtil sistem modülünü yükleme",
                                         moduleResult));
    }

    if (!fileExists(packagePath)) {
        return InstallOutcome(InstallResult::Failed,
                              "Kurulacak PKG dosyası bulunamadı");
    }

    const uint64_t actualSize = fileSize(packagePath);
    if (package.sizeBytes > 0 && actualSize != package.sizeBytes) {
        std::ostringstream message;
        message << "Kurulum durduruldu: PKG boyutu beklenen "
                << package.sizeBytes << " bayt yerine " << actualSize << " bayt";
        return InstallOutcome(InstallResult::Failed, message.str());
    }

    if (!appInstInitialized_) {
        const int32_t result = sceAppInstUtilInitialize();
        if (result != 0) {
            return InstallOutcome(InstallResult::Failed,
                                  orbisError("AppInstUtil başlatma", result));
        }
        appInstInitialized_ = true;
    }

    char titleId[16];
    std::memset(titleId, 0, sizeof(titleId));
    int32_t isApp = 0;
    int32_t result = sceAppInstUtilGetTitleIdFromPkg(packagePath.c_str(),
                                                     titleId, &isApp);
    if (result != 0) {
        return InstallOutcome(InstallResult::Failed,
                              orbisError("PKG kimliği okuma", result));
    }

    // Dosya artık ağ kaynağı değildir: doğrulanmış ve kapatılmış yerel PKG'yi
    // doğrudan AppInstUtil'a ver. BGFT download görevi kullanmak aynı dosyayı
    // yeniden bir indirme hedefi gibi işleyip boyutunun büyümesine yol açabiliyordu.
    result = sceAppInstUtilAppInstallPkg(packagePath.c_str(), NULL);
    if (result != 0) {
        return InstallOutcome(InstallResult::Failed,
                              orbisError("Yerel PKG kurulumunu başlatma", result));
    }

    std::ostringstream message;
    message << item.title << " tam boyutta indirildi; " << titleId
            << " yerel PKG kurulumu başlatıldı";
    return InstallOutcome(InstallResult::InstallStarted, message.str());
#endif
}

InstallOutcome OrbisInstaller::requestRemoteInstall(const CatalogItem& item,
                                                     const PackageInfo& package) {
#if !defined(PSFORCER_ORBIS)
    (void)item;
    (void)package;
    return InstallOutcome(InstallResult::Failed,
                          "Doğrudan PS4 indirmesi yalnızca PS4 derlemesinde kullanılabilir");
#else
    resetBgftDiagnostic();
    if (package.url.empty() || package.sizeBytes == 0) {
        appendBgftDiagnostic("paket_bilgisi_gecersiz", -1);
        return InstallOutcome(InstallResult::Failed,
                              "Uzak PKG adresi veya kesin boyutu eksik");
    }
    if (package.sizeBytes > std::numeric_limits<uint32_t>::max()) {
        return InstallOutcome(InstallResult::Failed,
                              "PKG boyutu PS4 BGFT sınırını aşıyor");
    }

    // Stub kitaplıklarına bağlanmak çalışma zamanında modülü otomatik yüklemez.
    // BGFT veya UserService dışa aktarımlarına modül yüklenmeden dokunmak bazı
    // PS4 ortamlarında CE-34878-0 ile sürecin kapanmasına neden olur.
    if (!runtimeModulesLoaded_) {
        struct RequiredModule {
            OrbisSysModuleInternal id;
            const char* stage;
            const char* name;
        };
        const RequiredModule modules[] = {
            {ORBIS_SYSMODULE_INTERNAL_SYSCORE, "syscore_modulu", "SysCore"},
            {ORBIS_SYSMODULE_INTERNAL_SYSTEM_SERVICE,
             "system_service_modulu", "SystemService"},
            {ORBIS_SYSMODULE_INTERNAL_USER_SERVICE,
             "user_service_modulu", "UserService"},
            {ORBIS_SYSMODULE_INTERNAL_NETCTL, "netctl_modulu", "NetCtl"},
            {ORBIS_SYSMODULE_INTERNAL_NET, "net_modulu", "Net"},
            {ORBIS_SYSMODULE_INTERNAL_SSL, "ssl_modulu", "SSL"},
            {ORBIS_SYSMODULE_INTERNAL_HTTP, "http_modulu", "HTTP"},
            {ORBIS_SYSMODULE_INTERNAL_APP_INST_UTIL,
             "app_inst_util_modulu", "AppInstUtil"},
            {ORBIS_SYSMODULE_INTERNAL_BGFT, "bgft_modulu", "BGFT"},
            {ORBIS_SYSMODULE_INTERNAL_NP_COMMON, "np_common_modulu", "NpCommon"}
        };
        for (size_t i = 0; i < sizeof(modules) / sizeof(modules[0]); ++i) {
            const int32_t moduleResult = loadInternalModule(
                modules[i].id, modules[i].stage);
            if (moduleResult < 0) {
                return InstallOutcome(
                    InstallResult::Failed,
                    orbisError((std::string(modules[i].name) +
                                " sistem modülünü yükleme").c_str(),
                               moduleResult));
            }
        }
        runtimeModulesLoaded_ = true;
    }

    // BGFT istemci oturumu yalnızca modülleri yüklemekle oluşmaz. Sony'nin
    // başlatma sırasına uygun olarak kullanıcı ve paket kurulum hizmetlerini
    // BGFT'den önce bir kez başlat. GameBaTo, UserService için varsayılan NULL
    // yerine 0x100 öncelik parametresi kullanıyor; aynı çağrı biçimini koru.
    if (!userServiceInitialized_) {
        appendBgftDiagnostic("user_service_init_basliyor");
        int32_t userServicePriority = 0x100;
        const int32_t result = sceUserServiceInitialize(&userServicePriority);
        appendBgftDiagnostic("user_service_init_tamam", result);
        if (result != 0) {
            return InstallOutcome(InstallResult::Failed,
                                  orbisError("UserService başlatma", result));
        }
        userServiceInitialized_ = true;
    }

    if (!appInstInitialized_) {
        appendBgftDiagnostic("app_inst_util_init_basliyor");
        const int32_t result = sceAppInstUtilInitialize();
        appendBgftDiagnostic("app_inst_util_init_tamam", result);
        if (result != 0) {
            return InstallOutcome(InstallResult::Failed,
                                  orbisError("AppInstUtil başlatma", result));
        }
        appInstInitialized_ = true;
    }

    HttpClient resolver;
    std::string resolvedUrl;
    std::vector<uint8_t> headerData;
    std::string error;
    appendBgftDiagnostic("http_basliyor");
    if (!resolver.resolvePackageHeader(package.url, package.sizeBytes,
                                       resolvedUrl, headerData, error)) {
        appendBgftDiagnostic("http_basarisiz", -1);
        return InstallOutcome(InstallResult::Failed, error);
    }
    appendBgftDiagnostic("http_tamam");

    PkgHeaderInfo header;
    if (!parsePkgHeader(headerData, header, error)) {
        appendBgftDiagnostic("pkg_basligi_gecersiz", -1);
        return InstallOutcome(InstallResult::Failed, error);
    }
    appendBgftDiagnostic("pkg_basligi_tamam");
    if (header.packageSize != package.sizeBytes) {
        std::ostringstream message;
        message << "PKG başlığındaki boyut katalogla uyuşmuyor. Beklenen "
                << package.sizeBytes << " bayt, başlıkta " << header.packageSize;
        return InstallOutcome(InstallResult::Failed, message.str());
    }
    if (resolvedUrl.size() >= 0x800) {
        return InstallOutcome(InstallResult::Failed,
                              "İmzalı PKG adresi PS4 BGFT sınırını aşıyor");
    }

    if (!bgftInitialized_) {
        const size_t heapSize = 1024 * 1024;
        bgftHeap_ = std::malloc(heapSize);
        if (!bgftHeap_) {
            return InstallOutcome(InstallResult::Failed,
                                  "PS4 indirme hizmeti için bellek ayrılamadı");
        }
        std::memset(bgftHeap_, 0, heapSize);
        OrbisBgftInitParams initParams;
        std::memset(&initParams, 0, sizeof(initParams));
        initParams.heap = bgftHeap_;
        initParams.heapSize = heapSize;
        appendBgftDiagnostic("bgft_init_basliyor");
        const int32_t initResult = sceBgftServiceIntInit(&initParams);
        appendBgftDiagnostic("bgft_init_tamam", initResult);
        if (initResult != 0) {
            std::free(bgftHeap_);
            bgftHeap_ = NULL;
            return InstallOutcome(InstallResult::Failed,
                                  orbisError("PS4 indirme hizmetini başlatma", initResult));
        }
        bgftInitialized_ = true;
    }

    int32_t userId = -1;
    appendBgftDiagnostic("aktif_kullanici_basliyor");
    int32_t result = sceUserServiceGetForegroundUser(&userId);
    appendBgftDiagnostic("aktif_kullanici_tamam", result);
    if (result != 0) {
        return InstallOutcome(InstallResult::Failed,
                              orbisError("Aktif PS4 kullanıcısını alma", result));
    }
    if (userId < 0) {
        return InstallOutcome(InstallResult::Failed,
                              "Aktif PS4 kullanıcısı bulunamadı");
    }

    const std::string displayName = item.title + " - " + package.label;
    OrbisBgftDownloadParam params;
    std::memset(&params, 0, sizeof(params));
    params.userId = userId;
    params.entitlementType = 5;
    params.id = header.contentId.c_str();
    params.contentUrl = resolvedUrl.c_str();
    params.contentName = displayName.c_str();
    params.iconPath = "";
    params.playgoScenarioId = "0";
    params.option = ORBIS_BGFT_TASK_OPT_DISABLE_CDN_QUERY_PARAM;
    params.packageType = header.packageType.c_str();
    params.packageSubType = "";
    params.packageSize = static_cast<uint32_t>(header.packageSize);

    OrbisBgftTaskId taskId = -1;
    appendBgftDiagnostic("gorev_kaydi_basliyor");
    // GameBaTo'nun PS4 sistem indiricisi yolu, hem ana paket hem yama için
    // DebugDownloadRegisterPkg ile kayıt yapıp görevi IntDownloadStartTask ile
    // başlatıyor. Bu ikili aynı BGFT istemci oturumunda kullanılmalıdır.
    result = sceBgftServiceIntDebugDownloadRegisterPkg(&params, &taskId);
    appendBgftDiagnostic("gorev_kaydi_tamam", result);
    if (result != 0) {
        return InstallOutcome(InstallResult::Failed,
                              orbisError("PS4 indirme görevini oluşturma", result));
    }

    appendBgftDiagnostic("gorev_baslatiliyor");
    result = sceBgftServiceIntDownloadStartTask(taskId);
    appendBgftDiagnostic("gorev_baslatma_tamam", result);
    if (result != 0) {
        sceBgftServiceIntDownloadUnregisterTask(taskId);
        return InstallOutcome(InstallResult::Failed,
                              orbisError("PS4 indirme görevini başlatma", result));
    }

    std::ostringstream message;
    message << displayName
            << " PS4 indirme ve kurulum kuyruğuna eklendi";
    appendBgftDiagnostic("basarili");
    return InstallOutcome(InstallResult::InstallStarted, message.str());
#endif
}

}  // namespace psforcer
