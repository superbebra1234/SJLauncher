#define _CRT_SECURE_NO_WARNINGS
#include "Settings.h"
#include "Launcher.h" // Нам нужна функция GetExeDir()
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

using json = nlohmann::json;
namespace fs = std::filesystem;

LauncherConfig currentSettings;

fs::path GetSettingsPath() {
    return GetExeDir() / "launcher_settings.json";
}

void LoadSettings() {
    fs::path path = GetSettingsPath();
    if (!fs::exists(path)) {
        SaveSettings(); // Если файла нет, создаем дефолтный
        return;
    }
    
    try {
        std::ifstream f(path);
        json j = json::parse(f);
        
        currentSettings.playerName = j.value("playerName", "Player");
        currentSettings.ramMb = j.value("ramMb", 2048);
        currentSettings.javaPath = j.value("javaPath", "");
    } catch (...) {
        // Если кто-то сломал JSON руками, просто ничего не делаем, останутся дефолтные
    }
}

void SaveSettings() {
    try {
        json j;
        j["playerName"] = currentSettings.playerName;
        j["ramMb"] = currentSettings.ramMb;
        j["javaPath"] = currentSettings.javaPath;

        std::ofstream f(GetSettingsPath());
        f << j.dump(4);
    } catch (...) {}
}