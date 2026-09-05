#define _CRT_SECURE_NO_WARNINGS
#include "Launcher.h"
#include "Settings.h"
#include "VersionUtils.h"
#include <windows.h>
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")
#include <fstream>
#include <iostream>
#include <sstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

// Глобальные переменные
std::vector<MyInstance> myInstancesList;
std::atomic<bool> needReloadInstances{false};

fs::path GetExeDir() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    return fs::path(buffer).parent_path();
}

static int GetSystemRamMb() {
    MEMORYSTATUSEX status = { sizeof(status) };
    if (GlobalMemoryStatusEx(&status)) {
        return (int)(status.ullTotalPhys / (1024 * 1024));
    }
    return 8192; // разумное значение по умолчанию
}

fs::path GetInstancesRootDir() {
    if (!currentSettings.instancesDir.empty()) return fs::path(currentSettings.instancesDir);
    return GetExeDir() / "instances";
}

fs::path GetInstanceDir(const std::string& name) {
    return GetInstancesRootDir() / name;
}

static fs::path GetGroupsFilePath() {
    return GetInstancesRootDir() / "instgroups.json";
}

// ─── Управление группами ───────────────────────────────────────────────────────
static nlohmann::ordered_json LoadGroupsRaw() {
    fs::path p = GetGroupsFilePath();
    if (!fs::exists(p)) return nlohmann::ordered_json{{"formatVersion", "1"}, {"groups", nlohmann::ordered_json::object()}};
    try {
        std::ifstream f(p);
        return nlohmann::ordered_json::parse(f);
    } catch (...) {
        return nlohmann::ordered_json{{"formatVersion", "1"}, {"groups", nlohmann::ordered_json::object()}};
    }
}

static void SaveGroupsRaw(const nlohmann::ordered_json& j) {
    try {
        fs::create_directories(GetInstancesRootDir());
        std::ofstream f(GetGroupsFilePath());
        f << j.dump(4);
    } catch (...) {}
}

std::vector<std::string> GetAllGroupNames() {
    std::vector<std::string> out;
    auto j = LoadGroupsRaw();
    if (j.contains("groups")) {
        for (auto it = j["groups"].begin(); it != j["groups"].end(); ++it) {
            out.push_back(it.key());
        }
    }
    return out;
}

std::string GetInstanceGroup(const std::string& name) {
    auto j = LoadGroupsRaw();
    if (!j.contains("groups")) return "";
    for (auto it = j["groups"].begin(); it != j["groups"].end(); ++it) {
        if (it.value().contains("instances")) {
            for (const auto& inst : it.value()["instances"]) {
                if (inst.get<std::string>() == name) return it.key();
            }
        }
    }
    return "";
}

static void RemoveInstanceFromAllGroups(nlohmann::ordered_json& j, const std::string& name) {
    if (!j.contains("groups")) return;
    for (auto it = j["groups"].begin(); it != j["groups"].end(); ++it) {
        if (!it.value().contains("instances")) continue;
        auto& arr = it.value()["instances"];
        nlohmann::ordered_json filtered = nlohmann::ordered_json::array();
        for (const auto& inst : arr) {
            if (inst.get<std::string>() != name) filtered.push_back(inst);
        }
        arr = filtered;
    }
}

void SetInstanceGroup(const std::string& name, const std::string& group) {
    auto j = LoadGroupsRaw();
    if (!j.contains("groups")) j["groups"] = nlohmann::ordered_json::object();
    RemoveInstanceFromAllGroups(j, name);
    if (!group.empty()) {
        if (!j["groups"].contains(group)) {
            j["groups"][group] = { {"hidden", false}, {"instances", nlohmann::ordered_json::array()} };
        }
        j["groups"][group]["instances"].push_back(name);
    }
    SaveGroupsRaw(j);
    needReloadInstances = true;
}

void RenameGroup(const std::string& oldGroup, const std::string& newGroup) {
    if (oldGroup == newGroup || newGroup.empty()) return;
    auto j = LoadGroupsRaw();
    if (!j.contains("groups") || !j["groups"].contains(oldGroup)) return;

    nlohmann::ordered_json rebuilt = nlohmann::ordered_json::object();
    for (auto it = j["groups"].begin(); it != j["groups"].end(); ++it) {
        std::string key = (it.key() == oldGroup) ? newGroup : it.key();
        rebuilt[key] = it.value();
    }
    j["groups"] = rebuilt;
    SaveGroupsRaw(j);
    needReloadInstances = true;
}

void DeleteGroup(const std::string& group) {
    auto j = LoadGroupsRaw();
    if (!j.contains("groups") || !j["groups"].contains(group)) return;
    j["groups"].erase(group);
    SaveGroupsRaw(j);
    needReloadInstances = true;
}

// ─── Настройки инстансов (Overrides & Notes) ──────────────────────────────────
static fs::path GetInstanceSettingsPath(const std::string& name) {
    return GetInstanceDir(name) / "instance_settings.json";
}

InstanceOverrides LoadInstanceOverrides(const std::string& name) {
    InstanceOverrides ov;
    fs::path p = GetInstanceSettingsPath(name);
    if (!fs::exists(p)) return ov;
    try {
        std::ifstream f(p); json j = json::parse(f);
        ov.overrideJava = j.value("overrideJava", false);
        ov.javaPath     = j.value("javaPath", "");
        ov.javaArgs     = j.value("javaArgs", "");
        ov.memStrategy  = j.value("memStrategy", 0);
        ov.ramMb        = j.value("ramMb", 2048);
        ov.memPercent   = j.value("memPercent", 50);
    } catch (...) {}
    return ov;
}

void SaveInstanceOverrides(const std::string& name, const InstanceOverrides& ov) {
    try {
        fs::path p = GetInstanceSettingsPath(name);
        json j;
        if (fs::exists(p)) { try { std::ifstream f(p); j = json::parse(f); } catch (...) {} }
        j["overrideJava"] = ov.overrideJava;
        j["javaPath"]     = ov.javaPath;
        j["javaArgs"]     = ov.javaArgs;
        j["memStrategy"]  = ov.memStrategy;
        j["ramMb"]        = ov.ramMb;
        j["memPercent"]   = ov.memPercent;
        fs::create_directories(p.parent_path());
        std::ofstream f(p);
        f << j.dump(4);
    } catch (...) {}
}

std::string LoadInstanceNotes(const std::string& name) {
    fs::path p = GetInstanceSettingsPath(name);
    if (!fs::exists(p)) return "";
    try {
        std::ifstream f(p); json j = json::parse(f);
        return j.value("notes", "");
    } catch (...) { return ""; }
}

void SaveInstanceNotes(const std::string& name, const std::string& notes) {
    try {
        fs::path p = GetInstanceSettingsPath(name);
        json j;
        if (fs::exists(p)) { try { std::ifstream f(p); j = json::parse(f); } catch (...) {} }
        j["notes"] = notes;
        fs::create_directories(p.parent_path());
        std::ofstream f(p);
        f << j.dump(4);
    } catch (...) {}
}

// ─── Управление файлами инстанса (Rename, Duplicate, Delete) ──────────────────
bool RenameInstance(const std::string& oldName, const std::string& newName, std::string* errorOut) {
    if (oldName == newName) return true;
    if (newName.empty()) { if (errorOut) *errorOut = "Имя не может быть пустым"; return false; }

    fs::path oldDir = GetInstanceDir(oldName);
    fs::path newDir = GetInstanceDir(newName);
    if (!fs::exists(oldDir)) { if (errorOut) *errorOut = "Инстанс не найден"; return false; }
    if (fs::exists(newDir))  { if (errorOut) *errorOut = "Инстанс с таким именем уже существует"; return false; }

    std::error_code ec;
    fs::rename(oldDir, newDir, ec);
    if (ec) { if (errorOut) *errorOut = ec.message(); return false; }

    fs::path infoPath = newDir / "instance_info.json";
    if (fs::exists(infoPath)) {
        try {
            std::ifstream f(infoPath); json j = json::parse(f); f.close();
            j["name"] = newName;
            std::ofstream out(infoPath); out << j.dump(4);
        } catch (...) {}
    }

    std::string grp = GetInstanceGroup(oldName);
    if (!grp.empty()) {
        auto j = LoadGroupsRaw();
        RemoveInstanceFromAllGroups(j, oldName);
        SaveGroupsRaw(j);
        SetInstanceGroup(newName, grp);
    }

    needReloadInstances = true;
    return true;
}

bool DuplicateInstance(const std::string& srcName, const std::string& newName, std::string* errorOut) {
    if (newName.empty()) { if (errorOut) *errorOut = "Имя не может быть пустым"; return false; }
    fs::path srcDir = GetInstanceDir(srcName);
    fs::path dstDir = GetInstanceDir(newName);
    if (!fs::exists(srcDir)) { if (errorOut) *errorOut = "Исходный инстанс не найден"; return false; }
    if (fs::exists(dstDir))  { if (errorOut) *errorOut = "Инстанс с таким именем уже существует"; return false; }

    std::error_code ec;
    fs::copy(srcDir, dstDir, fs::copy_options::recursive, ec);
    if (ec) { if (errorOut) *errorOut = ec.message(); return false; }

    fs::path infoPath = dstDir / "instance_info.json";
    if (fs::exists(infoPath)) {
        try {
            std::ifstream f(infoPath); json j = json::parse(f); f.close();
            j["name"] = newName;
            std::ofstream out(infoPath); out << j.dump(4);
        } catch (...) {}
    }

    std::string grp = GetInstanceGroup(srcName);
    if (!grp.empty()) SetInstanceGroup(newName, grp);

    needReloadInstances = true;
    return true;
}

bool DeleteInstance(const std::string& name) {
    fs::path dir = GetInstanceDir(name);
    if (!fs::exists(dir)) return false;
    std::error_code ec;
    fs::remove_all(dir, ec);

    auto j = LoadGroupsRaw();
    RemoveInstanceFromAllGroups(j, name);
    SaveGroupsRaw(j);

    needReloadInstances = true;
    return !ec;
}

void OpenInstanceFolder(const std::string& name) {
    fs::path dir = GetInstanceDir(name);
    fs::create_directories(dir);
    ShellExecuteA(NULL, "open", dir.string().c_str(), NULL, NULL, SW_SHOWNORMAL);
}

void LoadMyInstances() {
    myInstancesList.clear();
    fs::path instDir = GetInstancesRootDir();
    if (!fs::exists(instDir)) return;
    for (const auto& entry : fs::directory_iterator(instDir)) {
        if (!entry.is_directory()) continue;
        fs::path jsonPath = entry.path() / "instance_info.json";
        if (!fs::exists(jsonPath)) continue;
        try {
            std::ifstream f(jsonPath); json j = json::parse(f);
            MyInstance inst;
            inst.name       = j.value("name", entry.path().filename().string());
            inst.mc_version = j.value("mc_version", "?");
            inst.mod_loader = j.value("mod_loader", "Vanilla");
            inst.status     = j.value("status", "ready");
            inst.group      = GetInstanceGroup(inst.name);
            inst.notes      = LoadInstanceNotes(inst.name);
            inst.overrides  = LoadInstanceOverrides(inst.name);
            myInstancesList.push_back(inst);
        } catch (...) {}
    }
}

// ─── Подготовка к запуску ─────────────────────────────────────────────────────
static std::string BuildClasspath(const json& verData, const fs::path& libsDir, const fs::path& clientJar) {
    std::string classpath;
    if (verData.contains("libraries")) {
        for (const auto& lib : verData["libraries"]) {
            if (lib.contains("rules") && !VersionUtils::RulesAllow(lib["rules"])) continue;
            
            if (lib.contains("downloads") && lib["downloads"].contains("artifact")) {
                fs::path p = libsDir / lib["downloads"]["artifact"].value("path", "");
                if (fs::exists(p)) classpath += p.string() + ";";
            }
            else if (lib.contains("name")) {
                std::string name = lib["name"];
                size_t colon1 = name.find(':');
                size_t colon2 = name.find(':', colon1 + 1);
                if (colon1 != std::string::npos && colon2 != std::string::npos) {
                    std::string domain = name.substr(0, colon1);
                    std::replace(domain.begin(), domain.end(), '.', '/');
                    std::string artifact = name.substr(colon1 + 1, colon2 - colon1 - 1);
                    std::string version = name.substr(colon2 + 1);
                    std::string pathStr = domain + "/" + artifact + "/" + version + "/" + artifact + "-" + version + ".jar";
                    fs::path p = libsDir / pathStr;
                    if (fs::exists(p)) classpath += p.string() + ";";
                }
            }
        }
    }
    classpath += clientJar.string();
    return classpath;
}

static fs::path ResolveAssetsRoot(const fs::path& assetsDir, const fs::path& mcDir, const std::string& assetsIndexId) {
    fs::path indexPath = assetsDir / "indexes" / (assetsIndexId + ".json");
    if (fs::exists(indexPath)) {
        try {
            std::ifstream f(indexPath);
            json idx = json::parse(f);
            if (idx.value("map_to_resources", false)) return mcDir / "resources";
            if (idx.value("virtual", false)) return assetsDir / "virtual" / "legacy";
        } catch (...) {}
    }
    return assetsDir;
}

// Ищет скачанную Java внутри папки runtimes
static std::string FindJavaInRuntime(int majorVersion) {
    fs::path dir = GetExeDir() / "runtimes" / ("java-" + std::to_string(majorVersion));
    if (!fs::exists(dir)) return "";
    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().filename() == "java.exe") {
            return entry.path().string();
        }
    }
    return "";
}

// ─── Запуск игры ──────────────────────────────────────────────────────────────
void LaunchGame(const MyInstance& inst) {
    fs::path basePath = GetInstanceDir(inst.name);
    fs::path mcDir = basePath / ".minecraft";
    fs::path verDir = mcDir / "versions" / inst.mc_version;
    fs::path libsDir = mcDir / "libraries";
    fs::path assetsDir = mcDir / "assets";
    fs::path nativesDir = verDir / "natives";
    fs::path clientJar = verDir / (inst.mc_version + ".jar");
    fs::path verJson = verDir / (inst.mc_version + ".json");

    if (!fs::exists(verJson) || !fs::exists(clientJar)) {
        MessageBoxA(NULL, "Файлы игры не найдены! Попробуйте переустановить сборку.", "Ошибка", MB_ICONERROR);
        return;
    }

    json verData;
    try {
        std::ifstream f(verJson);
        verData = json::parse(f);
    } catch (...) {
        MessageBoxA(NULL, "Файл version.json поврежден! Переустановите сборку.", "Ошибка", MB_ICONERROR);
        return;
    }

    std::string mainClass = verData.value("mainClass", "net.minecraft.client.main.Main");
    std::string assetsIndexId = verData.contains("assetIndex") ? verData["assetIndex"].value("id", "legacy")
                                                                 : verData.value("assets", "legacy");
    fs::path assetsRoot = ResolveAssetsRoot(assetsDir, mcDir, assetsIndexId);
    std::string classpath = BuildClasspath(verData, libsDir, clientJar);

    std::map<std::string, std::string> vars = {
        {"auth_player_name", currentSettings.playerName},
        {"version_name", inst.mc_version},
        {"game_directory", mcDir.string()},
        {"assets_root", assetsRoot.string()},
        {"game_assets", assetsRoot.string()},
        {"assets_index_name", assetsIndexId},
        {"auth_uuid", "00000000-0000-0000-0000-000000000000"},
        {"auth_access_token", "0"},
        {"user_type", "msa"},
        {"version_type", verData.value("type", "release")},
        {"user_properties", "{}"},
        {"clientid", "-"},
        {"auth_xuid", "0"},
        {"natives_directory", nativesDir.string()},
        {"launcher_name", "SJLauncher"},
        {"launcher_version", "2.0"},
        {"classpath", classpath},
        {"classpath_separator", ";"},
        {"library_directory", libsDir.string()},
    };

    auto safeQuote = [](const std::string& arg) {
        if (arg.empty()) return std::string("\"\"");
        if (arg.find(' ') != std::string::npos && arg.front() != '"') {
            return "\"" + arg + "\"";
        }
        return arg;
    };

    std::string jvmArgs;
    std::vector<std::string> officialJvmArgs = VersionUtils::ExtractJvmArgs(verData);
    if (!officialJvmArgs.empty()) {
        for (auto& a : officialJvmArgs) {
            jvmArgs += safeQuote(VersionUtils::SubstituteVars(a, vars)) + " ";
        }
        if (jvmArgs.find(classpath) == std::string::npos) {
            jvmArgs += "-cp \"" + classpath + "\" ";
        }
    } else {
        jvmArgs = "-Djava.library.path=\"" + nativesDir.string() + "\" -cp \"" + classpath + "\" ";
    }

    const InstanceOverrides& ov = inst.overrides;
    int effectiveRamMb = ov.overrideJava
        ? (ov.memStrategy == 0 ? ov.ramMb : (int)(GetSystemRamMb() * (ov.memPercent / 100.0)))
        : (currentSettings.memStrategy == MemoryStrategy::Fixed
            ? currentSettings.ramMb
            : (int)(GetSystemRamMb() * (currentSettings.memPercent / 100.0)));
    std::string effectiveJavaArgs = ov.overrideJava ? ov.javaArgs : currentSettings.javaArgs;
    
    // ==========================================
    // ЛОГИКА ВЫБОРА JAVA (Auto-Detect)
    // ==========================================
    std::string javaExe = "java"; 
    
    int requiredJava = 8;
    if (verData.contains("javaVersion") && verData["javaVersion"].contains("majorVersion")) {
        requiredJava = verData["javaVersion"]["majorVersion"].get<int>();
    } else {
        if (inst.mc_version.find("1.17") != std::string::npos) requiredJava = 16;
        else if (inst.mc_version.find("1.18") != std::string::npos || inst.mc_version.find("1.19") != std::string::npos || inst.mc_version.find("1.20") != std::string::npos) requiredJava = 17;
        else if (inst.mc_version.find("1.21") != std::string::npos) requiredJava = 21;
    }

    if (currentSettings.autoDetectJava && !ov.overrideJava) {
        std::string downloadedJava = FindJavaInRuntime(requiredJava);
        if (!downloadedJava.empty()) {
            javaExe = "\"" + downloadedJava + "\"";
        } else {
            char* javaHome = getenv("JAVA_HOME");
            if (javaHome) {
                fs::path p = fs::path(javaHome) / "bin" / "java.exe";
                if (fs::exists(p)) javaExe = "\"" + p.string() + "\"";
            }
        }
    } 
    else if (ov.overrideJava && !ov.javaPath.empty()) {
        javaExe = "\"" + ov.javaPath + "\"";
    } 
    else if (!currentSettings.javaPath.empty()) {
        javaExe = "\"" + currentSettings.javaPath + "\"";
    }

    std::string customFlags = "-Xmx" + std::to_string(effectiveRamMb) + "M";
    if (!effectiveJavaArgs.empty()) customFlags += " " + effectiveJavaArgs;

    jvmArgs = customFlags + " " + jvmArgs;

    std::vector<std::string> gameArgList = VersionUtils::ExtractGameArgs(verData);
    std::string gameArgs;
    for (auto& a : gameArgList) {
        gameArgs += safeQuote(VersionUtils::SubstituteVars(a, vars)) + " ";
    }

    fs::create_directories(mcDir); 

    std::string fullCmd = javaExe + " " + jvmArgs + " " + mainClass + " " + gameArgs;

    // Генерация start.bat для безопасного запуска
    fs::path batPath = mcDir / "start.bat";
    std::ofstream batFile(batPath);
    batFile << "@echo off\n";
    batFile << "echo Starting Minecraft...\n";
    batFile << fullCmd << "\n";
    batFile << "if %ERRORLEVEL% neq 0 (\n";
    batFile << "    echo.\n";
    batFile << "    echo [ERROR] Minecraft crashed or Java is missing!\n";
    batFile << "    pause\n";
    batFile << ")\n";
    batFile.close();

    std::string runCmd = "cmd.exe /c \"" + batPath.string() + "\"";
    std::vector<char> cmdBuffer(runCmd.begin(), runCmd.end());
    cmdBuffer.push_back('\0');

    STARTUPINFOA si = { sizeof(si) }; 
    PROCESS_INFORMATION pi = {};

    if (!CreateProcessA(nullptr, cmdBuffer.data(), nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE, nullptr, mcDir.string().c_str(), &si, &pi)) {
        MessageBoxA(NULL, "Критическая ошибка запуска (CreateProcessA)!", "Ошибка", MB_ICONERROR);
    } else {
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }
}
