#define _CRT_SECURE_NO_WARNINGS
#include "Settings.h"
#include "Launcher.h"
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
    if (!fs::exists(path)) { SaveSettings(); return; }
    try {
        std::ifstream f(path);
        json j = json::parse(f);

        // Java
        currentSettings.javaPath       = j.value("javaPath", "");
        currentSettings.ramMb          = j.value("ramMb", 2048);
        currentSettings.autoDetectJava = j.value("autoDetectJava", true);
        currentSettings.javaArgs       = j.value("javaArgs", "-XX:+UseG1GC -XX:+ParallelRefProcEnabled -XX:MaxGCPauseMillis=200");
        currentSettings.memStrategy    = (MemoryStrategy)j.value("memStrategy", 0);
        currentSettings.memPercent     = j.value("memPercent", 50);

        // Launcher
        currentSettings.playerName       = j.value("playerName", "Player");
        currentSettings.keepLauncherOpen = j.value("keepLauncherOpen", true);
        currentSettings.showSnapshots    = j.value("showSnapshots", false);
        currentSettings.showOldBeta      = j.value("showOldBeta", false);
        currentSettings.showOldAlpha     = j.value("showOldAlpha", false);
        currentSettings.sendAnalytics    = j.value("sendAnalytics", false);
        currentSettings.checkUpdates     = j.value("checkUpdates", true);
        currentSettings.instancesDir     = j.value("instancesDir", "");

        // Appearance
        currentSettings.theme              = (AppTheme)j.value("theme", 0);
        currentSettings.compactInstanceCards = j.value("compactInstanceCards", false);
        currentSettings.showInstanceNotes  = j.value("showInstanceNotes", true);

        // Window
        currentSettings.windowWidth      = j.value("windowWidth", 1280);
        currentSettings.windowHeight     = j.value("windowHeight", 720);
        currentSettings.maximized        = j.value("maximized", false);
        currentSettings.customWindowSize = j.value("customWindowSize", false);

        // Proxy
        currentSettings.useProxy   = j.value("useProxy", false);
        currentSettings.proxyHost  = j.value("proxyHost", "");
        currentSettings.proxyPort  = j.value("proxyPort", 8080);
        currentSettings.proxyUser  = j.value("proxyUser", "");
        currentSettings.proxyPass  = j.value("proxyPass", "");
    } catch (...) {}
}

void SaveSettings() {
    try {
        json j;
        // Java
        j["javaPath"]       = currentSettings.javaPath;
        j["ramMb"]          = currentSettings.ramMb;
        j["autoDetectJava"] = currentSettings.autoDetectJava;
        j["javaArgs"]       = currentSettings.javaArgs;
        j["memStrategy"]    = (int)currentSettings.memStrategy;
        j["memPercent"]     = currentSettings.memPercent;
        // Launcher
        j["playerName"]       = currentSettings.playerName;
        j["keepLauncherOpen"] = currentSettings.keepLauncherOpen;
        j["showSnapshots"]    = currentSettings.showSnapshots;
        j["showOldBeta"]      = currentSettings.showOldBeta;
        j["showOldAlpha"]     = currentSettings.showOldAlpha;
        j["sendAnalytics"]    = currentSettings.sendAnalytics;
        j["checkUpdates"]     = currentSettings.checkUpdates;
        j["instancesDir"]     = currentSettings.instancesDir;
        // Appearance
        j["theme"]               = (int)currentSettings.theme;
        j["compactInstanceCards"]= currentSettings.compactInstanceCards;
        j["showInstanceNotes"]   = currentSettings.showInstanceNotes;
        // Window
        j["windowWidth"]      = currentSettings.windowWidth;
        j["windowHeight"]     = currentSettings.windowHeight;
        j["maximized"]        = currentSettings.maximized;
        j["customWindowSize"] = currentSettings.customWindowSize;
        // Proxy
        j["useProxy"]  = currentSettings.useProxy;
        j["proxyHost"] = currentSettings.proxyHost;
        j["proxyPort"] = currentSettings.proxyPort;
        j["proxyUser"] = currentSettings.proxyUser;
        j["proxyPass"] = currentSettings.proxyPass;

        std::ofstream f(GetSettingsPath());
        f << j.dump(4);
    } catch (...) {}
}
