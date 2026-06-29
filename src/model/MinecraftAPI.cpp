#include "MinecraftAPI.h"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

// Вспомогательная функция: проверяем, можно ли эту библиотеку использовать на текущей ОС
bool checkRules(const json& rules) {
    if (!rules.is_array()) return true; // Если правил нет, значит подходит всем

    bool allowed = false;
    std::string osName = 
#ifdef _WIN32
        "windows";
#elif __APPLE__
        "osx";
#else
        "linux";
#endif

    for (const auto& rule : rules) {
        std::string action = rule.value("action", "");
        std::string ruleOs = rule.value("os", json::object()).value("name", "");

        if (ruleOs.empty() || ruleOs == osName) {
            allowed = (action == "allow");
        }
    }
    return allowed;
}

std::vector<MCVersion> MinecraftAPI::fetchVersionManifest() {
    std::vector<MCVersion> versions;
    try {
        cpr::Response r = cpr::Get(cpr::Url{"https://piston-meta.mojang.com/mc/game/version_manifest_v2.json"});
        if (r.status_code == 200) {
            json j = json::parse(r.text);
            for (const auto& v : j["versions"]) {
                MCVersion ver;
                ver.id = v["id"];
                ver.type = v["type"];
                ver.url = v["url"];
                versions.push_back(ver);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return versions;
}

MCVersionDetails MinecraftAPI::fetchVersionDetails(const std::string& versionUrl) {
    MCVersionDetails details;
    try {
        cpr::Response r = cpr::Get(cpr::Url{versionUrl});
        if (r.status_code != 200) return details;

        json j = json::parse(r.text);
        details.id = j["id"];
        details.mainClass = j["mainClass"];
        
        // 1. Ссылка на .jar клиента
        if (j.contains("downloads") && j["downloads"].contains("client")) {
            details.jarUrl = j["downloads"]["client"]["url"];
        }

        // 2. Ассеты (звуки, текстуры)
        if (j.contains("assets")) {
            details.assetsId = j["assets"];
        }
        if (j.contains("assetIndex") && j["assetIndex"].contains("url")) {
            details.assetsIndexUrl = j["assetIndex"]["url"];
        }

        // 3. Парсинг библиотек (САМОЕ ВАЖНОЕ)
        if (j.contains("libraries")) {
            for (const auto& lib : j["libraries"]) {
                // Проверяем правила для текущей ОС
                if (lib.contains("rules") && !checkRules(lib["rules"])) {
                    continue; // Пропускаем эту библиотеку
                }

                MCLibrary mcLib;
                mcLib.name = lib.value("name", "");
                mcLib.isNative = false;

                // Обычная .jar библиотека
                if (lib.contains("downloads") && lib["downloads"].contains("artifact")) {
                    mcLib.url = lib["downloads"]["artifact"].value("url", "");
                    mcLib.path = lib["downloads"]["artifact"].value("path", "");
                    if (!mcLib.url.empty()) {
                        details.libraries.push_back(mcLib);
                    }
                }

                // Нативная библиотека (.dll, .so, .dylib)
                if (lib.contains("downloads") && lib["downloads"].contains("classifiers")) {
                    std::string classifier = 
#ifdef _WIN32
                        "natives-windows";
#elif __APPLE__
                        "natives-macos";
#else
                        "natives-linux";
#endif
                    
                    if (lib["downloads"]["classifiers"].contains(classifier)) {
                        MCLibrary nativeLib;
                        nativeLib.name = mcLib.name;
                        nativeLib.isNative = true;
                        nativeLib.nativeClassifier = classifier;
                        nativeLib.url = lib["downloads"]["classifiers"][classifier].value("url", "");
                        nativeLib.path = lib["downloads"]["classifiers"][classifier].value("path", "");
                        if (!nativeLib.url.empty()) {
                            details.libraries.push_back(nativeLib);
                        }
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing details: " << e.what() << std::endl;
    }
    return details;
}