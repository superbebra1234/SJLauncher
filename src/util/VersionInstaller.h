#pragma once
#include "../model/MinecraftAPI.h"
#include <functional>

class VersionInstaller {
public:
    // Коллбэк для прогресса: (текущий файл, всего файлов, прогресс 0-100)
    using ProgressCallback = std::function<void(int, int, float)>;

    // Устанавливает версию: скачивает .jar, библиотеки и ассеты
    static bool installVersion(const MCVersionDetails& details, const std::string& gameDir, ProgressCallback callback = nullptr);
};