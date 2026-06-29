#pragma once
#include <string>
#include <vector>

// Структура для списка версий (для UI)
struct MCVersion {
    std::string id;         // "1.20.4", "1.7.10", "b1.8.1"
    std::string type;       // "release", "snapshot", "old_beta"
    std::string url;        // Ссылка на JSON этой версии
};

// Структура для библиотеки (нужно для скачивания и формирования Classpath)
struct MCLibrary {
    std::string name;       // Например: "org.lwjgl:lwjgl:3.3.1"
    std::string url;        // Прямая ссылка на .jar файл
    std::string path;       // Путь для сохранения (например, org/lwjgl/lwjgl/3.3.1/lwjgl-3.3.1.jar)
    bool isNative;          // true, если это нативная библиотека (.dll/.so/.dylib)
    std::string nativeClassifier; // Например: "natives-windows"
};

// Полная информация о версии для установки
struct MCVersionDetails {
    std::string id;
    std::string mainClass;      // Главный класс Java
    std::string jarUrl;         // Ссылка на .jar клиента
    std::string assetsId;       // ID индекса ассетов (например, "1.20")
    std::string assetsIndexUrl; // Ссылка на JSON индекс ассетов
    
    std::vector<MCLibrary> libraries; // Список всех библиотек
};

class MinecraftAPI {
public:
    static std::vector<MCVersion> fetchVersionManifest();
    static MCVersionDetails fetchVersionDetails(const std::string& versionUrl);
};