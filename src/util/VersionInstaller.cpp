#include "VersionInstaller.h"
#include "Downloader.h"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

// VersionInstaller::installVersion не используется основным UI (там InstallInstanceThread).
// Этот файл оставлен для совместимости — просто заглушка.
bool VersionInstaller::installVersion(const MCVersionDetails& details,
                                      const std::string& gameDir,
                                      ProgressCallback callback)
{
    fs::path baseDir(gameDir);
    fs::path versionsDir  = baseDir / "versions" / details.id;
    fs::path librariesDir = baseDir / "libraries";
    fs::path assetsDir    = baseDir / "assets";

    fs::create_directories(versionsDir);
    fs::create_directories(librariesDir);
    fs::create_directories(assetsDir);

    int totalFiles  = 1 + (int)details.libraries.size();
    int currentFile = 0;

    // 1. client.jar
    std::string jarPath = (versionsDir / (details.id + ".jar")).string();
    // Используем глобальную DownloadFile() из Downloader.h
    if (!DownloadFile(details.jarUrl, fs::path(jarPath))) {
        std::cerr << "Failed to download client jar: " << details.jarUrl << std::endl;
        return false;
    }
    currentFile++;
    if (callback) callback(currentFile, totalFiles, 100.0f);

    // 2. Библиотеки
    for (const auto& lib : details.libraries) {
        fs::path dest = librariesDir / lib.path;
        DownloadFile(lib.url, dest);
        currentFile++;
        if (callback) callback(currentFile, totalFiles, 100.0f);
    }

    return true;
}
