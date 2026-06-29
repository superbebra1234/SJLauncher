#pragma once
#include <string>

struct LauncherConfig {
    std::string playerName = "Player";
    int ramMb = 2048;                  // По умолчанию 2 ГБ
    std::string javaPath = "";         // Если пусто, лаунчер найдет сам
};

// Глобальная переменная, чтобы интерфейс и лаунчер видели текущие настройки
extern LauncherConfig currentSettings;

void LoadSettings();
void SaveSettings();