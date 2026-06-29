#define _CRT_SECURE_NO_WARNINGS
#include "Launcher.h"
#include "Settings.h"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

std::vector<MyInstance>  myInstancesList;
std::atomic<bool>        needReloadInstances{false};

// ─── Prism: Library::isActive() для Windows ───────────────────────────────────
static bool IsAllowedByRules(const json& rulesArr) {
    if (!rulesArr.is_array() || rulesArr.empty()) return true;
    bool allowed = false;
    for (auto& rule : rulesArr) {
        std::string action = rule.value("action", "allow");
        bool matches = true;
        if (rule.contains("os")) {
            std::string n = rule["os"].value("name", "");
            if (!n.empty() && n != "windows") matches = false;
        }
        if (matches) allowed = (action == "allow");
    }
    return allowed;
}

fs::path GetExeDir() {
    char buf[MAX_PATH];
    GetModuleFileNameA(NULL, buf, MAX_PATH);
    return fs::path(buf).parent_path();
}

void LoadMyInstances() {
    myInstancesList.clear();
    fs::path dir = GetExeDir() / "instances";
    if (!fs::exists(dir)) return;
    for (auto& e : fs::directory_iterator(dir)) {
        if (!e.is_directory()) continue;
        fs::path jp = e.path() / "instance_info.json";
        if (!fs::exists(jp)) continue;
        try {
            std::ifstream f(jp);
            json j = json::parse(f);
            MyInstance inst;
            inst.name       = j.value("name",       "Unknown");
            inst.mc_version = j.value("mc_version",  "?");
            inst.mod_loader = j.value("mod_loader",  "Vanilla");
            inst.status     = j.value("status",      "ready");
            myInstancesList.push_back(inst);
        } catch (...) {}
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// LaunchGame — полный аналог Prism MinecraftInstance::createLaunchScript()
// + LauncherPartLaunch
// ─────────────────────────────────────────────────────────────────────────────
void LaunchGame(const MyInstance& inst) {
    fs::path basePath   = GetExeDir() / "instances" / inst.name;
    fs::path mcDir      = basePath / ".minecraft";
    fs::path verDir     = mcDir / "versions"  / inst.mc_version;
    fs::path libsDir    = mcDir / "libraries";
    fs::path nativesDir = verDir / "natives";
    fs::path clientJar  = verDir / (inst.mc_version + ".jar");
    fs::path verJson    = verDir / (inst.mc_version + ".json");

    if (!fs::exists(verJson) || !fs::exists(clientJar)) {
        MessageBoxA(NULL,
            ("Файлы игры не найдены!\n\n"
             "Путь: " + verDir.string() + "\n\n"
             "Переустановите сборку.").c_str(),
            "Ошибка", MB_ICONERROR);
        return;
    }

    json verData = json::parse(std::ifstream(verJson));
    std::string mainClass = verData.value("mainClass", "net.minecraft.client.main.Main");

    // ── Classpath (Prism: getClassPath) ──────────────────────────────────────
    // Порядок: все библиотеки по rules, потом client.jar последним
    std::string classpath;
    if (verData.contains("libraries")) {
        for (auto& lib : verData["libraries"]) {
            if (lib.contains("rules") && !IsAllowedByRules(lib["rules"])) continue;
            if (lib.contains("downloads") && lib["downloads"].contains("artifact")) {
                std::string path = lib["downloads"]["artifact"].value("path", "");
                if (!path.empty()) {
                    fs::path p = libsDir / path;
                    if (fs::exists(p))
                        classpath += p.string() + ";";
                }
            }
        }
    }
    classpath += clientJar.string(); // client.jar — всегда последним

    // ── Asset index id ────────────────────────────────────────────────────────
    std::string assetId = "legacy";
    if (verData.contains("assetIndex"))
        assetId = verData["assetIndex"].value("id", "legacy");

    // ── Java executable ───────────────────────────────────────────────────────
    std::string javaExe = "java";
    if (!currentSettings.javaPath.empty() && fs::exists(currentSettings.javaPath)) {
        javaExe = "\"" + currentSettings.javaPath + "\"";
    } else {
        const char* jh = getenv("JAVA_HOME");
        if (jh) {
            fs::path p = fs::path(jh) / "bin" / "java.exe";
            if (fs::exists(p)) javaExe = "\"" + p.string() + "\"";
        }
    }

    // ── Функция замены токенов (Prism: replaceTokensIn) ───────────────────────
    // Все стандартные токены Mojang + наши
    struct Token { std::string key, val; };
    std::vector<Token> tokens = {
        {"auth_player_name",  currentSettings.playerName.empty() ? "Player" : currentSettings.playerName},
        {"version_name",      inst.mc_version},
        {"game_directory",    mcDir.string()},
        {"assets_root",       (mcDir / "assets").string()},
        {"game_assets",       (mcDir / "assets").string()}, // старый токен (до 1.7.10)
        {"assets_index_name", assetId},
        {"auth_uuid",         "00000000-0000-0000-0000-000000000000"},
        {"auth_access_token", "0"},
        {"auth_session",      "token:0:00000000-0000-0000-0000-000000000000"}, // очень старые версии
        {"clientid",          "0"},
        {"auth_xuid",         "0"},
        // user_type: "legacy" для offline (Prism использует "msa" только для аккаунтов)
        {"user_type",         "legacy"},
        {"user_properties",   "{}"},  // для Forge старых версий
        {"version_type",      "release"},
        {"natives_directory", nativesDir.string()},
        {"launcher_name",     "SJLauncher"},
        {"launcher_version",  "1.0"},
        {"classpath",         classpath},
        // Размер окна
        {"resolution_width",  "854"},
        {"resolution_height", "480"},
    };

    auto replaceAll = [&](std::string s) -> std::string {
        for (auto& t : tokens) {
            std::string tok = "${" + t.key + "}";
            size_t pos = 0;
            while ((pos = s.find(tok, pos)) != std::string::npos) {
                s.replace(pos, tok.size(), t.val);
                pos += t.val.size();
            }
        }
        return s;
    };

    // ── JVM аргументы (Prism: javaArguments) ─────────────────────────────────
    // Базовые аргументы — всегда
    std::string jvmPart =
        "-Xms512m "
        "-Xmx" + std::to_string(currentSettings.ramMb) + "M "
        "-XX:+UnlockExperimentalVMOptions -XX:+UseG1GC "
        // Hack для Intel GPU (есть в Prism)
        "-XX:HeapDumpPath=MojangTricksIntelDriversForPerformance_javaw.exe_minecraft.exe.heapdump "
        "-Dminecraft.launcher.brand=SJLauncher "
        "-Dminecraft.launcher.version=1.0 "
        "-Duser.language=en ";

    bool hasNewArgs = verData.contains("arguments") && verData["arguments"].contains("jvm");

    if (hasNewArgs) {
        // Версии 1.13+: аргументы из version.json arguments.jvm
        // Они содержат ${classpath}, ${natives_directory} и т.д.
        for (auto& arg : verData["arguments"]["jvm"]) {
            if (arg.is_string()) {
                jvmPart += replaceAll(arg.get<std::string>()) + " ";
            } else if (arg.is_object() && arg.contains("value")) {
                // feature-gated аргументы (Prism тоже добавляет всё без проверки rules для базовых)
                auto& val = arg["value"];
                if (val.is_string())
                    jvmPart += replaceAll(val.get<std::string>()) + " ";
                else if (val.is_array())
                    for (auto& v : val)
                        if (v.is_string())
                            jvmPart += replaceAll(v.get<std::string>()) + " ";
            }
        }
    } else {
        // Старые версии (до 1.13): classpath и natives вручную
        jvmPart += "-Djava.library.path=\"" + nativesDir.string() + "\" ";
        jvmPart += "-cp " + classpath + " ";
    }

    // ── Игровые аргументы (Prism: processMinecraftArgs) ──────────────────────
    std::string gamePart;

    if (hasNewArgs && verData["arguments"].contains("game")) {
        // Новый формат: arguments.game (массив строк и объектов)
        for (auto& arg : verData["arguments"]["game"]) {
            if (arg.is_string())
                gamePart += replaceAll(arg.get<std::string>()) + " ";
            // object (feature-gated) — пропускаем, как Prism для базового запуска
        }
    } else if (verData.contains("minecraftArguments")) {
        // Старый формат: одна строка с токенами (до 1.13)
        gamePart = replaceAll(verData["minecraftArguments"].get<std::string>());
    }

    // ── Финальная команда ─────────────────────────────────────────────────────
    std::string fullCmd = javaExe + " " + jvmPart + " " + mainClass + " " + gamePart;

    // ── Запуск через CreateProcess (Prism: LauncherPartLaunch) ───────────────
    fs::create_directories(nativesDir); // убедиться что папка есть
    fs::create_directories(mcDir);

    STARTUPINFOA si    = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    si.dwFlags     = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOW; // показываем консоль — удобно видеть ошибки Java

    std::vector<char> cmdBuf(fullCmd.begin(), fullCmd.end());
    cmdBuf.push_back('\0');

    if (!CreateProcessA(nullptr, cmdBuf.data(),
                        nullptr, nullptr, FALSE,
                        CREATE_NEW_CONSOLE,
                        nullptr, mcDir.string().c_str(),
                        &si, &pi))
    {
        DWORD ec = GetLastError();
        std::string err =
            "Не удалось запустить Java!\n\n"
            "Код ошибки: " + std::to_string(ec) + "\n\n"
            "Команда:\n" + fullCmd + "\n\n"
            "Убедитесь что Java установлена и путь к ней указан в настройках.";
        MessageBoxA(NULL, err.c_str(), "Ошибка запуска", MB_ICONERROR);
    } else {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}
