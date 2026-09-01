#pragma once
#include <string>

// Тема оформления (как в Modrinth)
enum class AppTheme {
    Dark = 0,
    Light = 1,
    System = 2
};

// Управление памятью
enum class MemoryStrategy {
    Fixed = 0,   // фиксированный размер
    Percent = 1  // процент от RAM
};

struct LauncherConfig {
    // === Java ===
    std::string javaPath = "";          // пусто = авто-поиск
    int ramMb = 2048;
    bool autoDetectJava = true;
    std::string javaArgs = "-XX:+UseG1GC -XX:+ParallelRefProcEnabled -XX:MaxGCPauseMillis=200";
    MemoryStrategy memStrategy = MemoryStrategy::Fixed;
    int memPercent = 50;

    // === Launcher ===
    std::string playerName = "Player";
    bool keepLauncherOpen = true;       // не закрывать лаунчер при запуске игры
    bool showSnapshots = false;         // показывать снапшоты в списке версий
    bool showOldBeta = false;
    bool showOldAlpha = false;
    bool sendAnalytics = false;
    bool checkUpdates = true;
    std::string instancesDir = "";      // папка инстансов (пусто = рядом с exe)

    // === Appearance ===
    AppTheme theme = AppTheme::Dark;
    bool compactInstanceCards = false;
    bool showInstanceNotes = true;

    // === Window ===
    int windowWidth = 1280;
    int windowHeight = 720;
    bool maximized = false;
    bool customWindowSize = false;

    // === Proxy ===
    bool useProxy = false;
    std::string proxyHost = "";
    int proxyPort = 8080;
    std::string proxyUser = "";
    std::string proxyPass = "";
};

extern LauncherConfig currentSettings;

void LoadSettings();
void SaveSettings();
