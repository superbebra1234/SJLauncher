#pragma once
#include <string>
#include <cstdint>
#include <filesystem>

struct McVersion { std::string id; std::string url; std::string type; };

// Настройки, которые могут быть переопределены для конкретного инстанса
// (аналог "instance.cfg" в PolyMC/MultiMC) - если override* == false,
// используется соответствующее значение из глобальных currentSettings.
struct InstanceOverrides {
    bool overrideJava   = false;
    std::string javaPath;                 // пусто = авто-поиск
    std::string javaArgs;
    int memStrategy = 0;                  // 0 = Fixed (МБ), 1 = Percent (см. MemoryStrategy)
    int ramMb       = 2048;
    int memPercent  = 50;
};

struct MyInstance {
    std::string name;
    std::string mc_version;
    std::string mod_loader;
    std::string status;
    std::string group = "";               // "" = без группы (Ungrouped)
    std::string notes = "";
    InstanceOverrides overrides;
};

struct DownloadTask {
    std::string url;
    std::filesystem::path destPath;
    std::string sha1;     // ожидаемый sha1, если известен (пусто = не проверять хэш)
    uint64_t size = 0;    // ожидаемый размер в байтах, если известен
};
