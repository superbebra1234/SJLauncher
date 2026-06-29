#include "Downloader.h"
#include "Launcher.h"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <zip.h>
#include <fstream>
#include <filesystem>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>

using json = nlohmann::json;
namespace fs = std::filesystem;

// ─── Глобальные переменные ────────────────────────────────────────────────────
std::vector<McVersion> realMcVersions;
std::atomic<bool>     isDownloading{false};
std::atomic<int>      downloadFilesTotal{0};
std::atomic<int>      downloadFilesDone{0};
std::atomic<bool>     cancelDownload{false};
std::string           downloadingInstanceName;
std::string           downloadingStageLabel;
std::mutex            uiMutex;
std::atomic<uint64_t> downloadBytesTotal{0};
std::atomic<uint64_t> downloadBytesDone{0};
std::atomic<double>   downloadSpeedMBps{0.0};
std::atomic<int>      downloadETASeconds{0};
std::string           downloadingCurrentFile;

// ─── Prism: Library::isActive() — проверка rules для Windows ─────────────────
static bool IsAllowedByRules(const json& rulesArr) {
    if (!rulesArr.is_array() || rulesArr.empty()) return true;
    bool allowed = false;
    for (const auto& rule : rulesArr) {
        std::string action = rule.value("action", "allow");
        bool matches = true;
        if (rule.contains("os")) {
            std::string osName = rule["os"].value("name", "");
            if (!osName.empty() && osName != "windows") matches = false;
        }
        if (matches) allowed = (action == "allow");
    }
    return allowed;
}

// ─── Скачать один файл (пропускает уже существующие) ─────────────────────────
bool DownloadFile(const std::string& url, const fs::path& destPath) {
    if (cancelDownload) return false;
    if (url.empty()) return false;

    fs::create_directories(destPath.parent_path());

    // Уже скачан — считаем байты и пропускаем
    if (fs::exists(destPath) && fs::file_size(destPath) > 0) {
        downloadBytesDone += fs::file_size(destPath);
        return true;
    }

    { std::lock_guard<std::mutex> lk(uiMutex);
      downloadingCurrentFile = destPath.filename().string(); }

    cpr::Response r = cpr::Get(cpr::Url{url}, cpr::Timeout{30000});
    if (r.status_code == 200) {
        std::ofstream f(destPath, std::ios::binary);
        if (f) {
            f.write(r.text.data(), (std::streamsize)r.text.size());
            downloadBytesDone += (uint64_t)r.text.size();
            return true;
        }
    }
    if (fs::exists(destPath)) fs::remove(destPath); // битый файл
    return false;
}

// ─── Распаковка natives .jar/.zip через libzip (как Prism ExtractNatives) ────
// .jar — это обычный zip. Prism извлекает только .dll/.so/.dylib, игнорируя META-INF
static void ExtractNativesZip(const fs::path& zipPath, const fs::path& destDir) {
    if (!fs::exists(zipPath) || fs::file_size(zipPath) == 0) return;

    int err = 0;
    zip_t* z = zip_open(zipPath.string().c_str(), ZIP_RDONLY, &err);
    if (!z) return;

    zip_int64_t n = zip_get_num_entries(z, 0);
    for (zip_int64_t i = 0; i < n; i++) {
        const char* name = zip_get_name(z, i, 0);
        if (!name) continue;
        std::string entry(name);

        // Папки и META-INF пропускаем (как Prism)
        if (entry.empty() || entry.back() == '/') continue;
        if (entry.find("META-INF") != std::string::npos) continue;

        // Только нативные библиотеки
        bool isNative = entry.find(".dll")   != std::string::npos
                     || entry.find(".so")    != std::string::npos
                     || entry.find(".dylib") != std::string::npos;
        if (!isNative) continue;

        fs::path out = destDir / fs::path(entry).filename();
        if (fs::exists(out)) continue; // не перезаписываем

        zip_file_t* zf = zip_fopen_index(z, i, 0);
        if (!zf) continue;

        std::ofstream of(out, std::ios::binary);
        char buf[65536];
        zip_int64_t read;
        while ((read = zip_fread(zf, buf, sizeof(buf))) > 0)
            of.write(buf, read);
        zip_fclose(zf);
    }
    zip_close(z);
}

// ─── Фоновый расчёт скорости и ETA ───────────────────────────────────────────
void UpdateDownloadStats() {
    auto  last      = std::chrono::steady_clock::now();
    uint64_t lastB  = 0;
    while (isDownloading && !cancelDownload) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        auto now    = std::chrono::steady_clock::now();
        double dt   = std::chrono::duration<double>(now - last).count();
        uint64_t cur = downloadBytesDone.load();
        double speed = (double)(cur - lastB) / (1024.0 * 1024.0) / dt;
        downloadSpeedMBps.store(speed);
        uint64_t rem = downloadBytesTotal.load() - cur;
        if (speed > 0.05)
            downloadETASeconds.store((int)((rem / (1024.0 * 1024.0)) / speed));
        last  = now;
        lastB = cur;
    }
}

// ─── Загрузка списка версий с Mojang ─────────────────────────────────────────
void FetchMinecraftVersions() {
    try {
        auto r = cpr::Get(
            cpr::Url{"https://piston-meta.mojang.com/mc/game/version_manifest_v2.json"},
            cpr::Timeout{10000});
        if (r.status_code != 200) return;
        json j = json::parse(r.text);
        realMcVersions.clear();
        for (auto& v : j["versions"])
            realMcVersions.push_back({v["id"], v["url"], v.value("type","")});
    } catch (...) {}
}

// ─────────────────────────────────────────────────────────────────────────────
// InstallInstanceThread — полный аналог Prism:
//   LibrariesTask + AssetUpdateTask + ExtractNatives
// ─────────────────────────────────────────────────────────────────────────────
void InstallInstanceThread(std::string name, std::string versionId,
                           std::string versionUrl, std::string loader)
{
    try {
        // Сброс состояния
        cancelDownload     = false;
        isDownloading      = true;
        downloadFilesTotal  = 0;
        downloadFilesDone   = 0;
        downloadBytesTotal  = 0;
        downloadBytesDone   = 0;
        downloadSpeedMBps   = 0.0;
        downloadETASeconds  = 0;

        auto setStage = [](const std::string& s) {
            std::lock_guard<std::mutex> lk(uiMutex);
            downloadingStageLabel  = s;
            downloadingCurrentFile = "";
        };

        { std::lock_guard<std::mutex> lk(uiMutex);
          downloadingInstanceName = name; }
        setStage("Сбор метаданных...");

        // ── Пути (как Prism: instances/<name>/.minecraft/) ───────────────────
        fs::path basePath   = GetExeDir() / "instances" / name;
        fs::path mcDir      = basePath / ".minecraft";
        fs::path verDir     = mcDir / "versions"  / versionId;
        fs::path libsDir    = mcDir / "libraries";
        fs::path assetsDir  = mcDir / "assets";
        fs::path nativesDir = verDir / "natives";

        fs::create_directories(verDir);
        fs::create_directories(libsDir);
        fs::create_directories(nativesDir);
        fs::create_directories(assetsDir / "indexes");
        fs::create_directories(assetsDir / "objects");

        // instance_info.json — наш аналог instance.cfg Prism
        {
            json info;
            info["name"]       = name;
            info["mc_version"] = versionId;
            info["mod_loader"] = loader;
            info["status"]     = "downloading";
            std::ofstream(basePath / "instance_info.json") << info.dump(4);
        }

        // ── Шаг 1: version.json (Prism: MinecraftInstance::getVersionFile) ───
        setStage("Загрузка version.json...");
        fs::path vJsonPath = verDir / (versionId + ".json");
        json verData;
        {
            std::string vJsonText;
            if (fs::exists(vJsonPath) && fs::file_size(vJsonPath) > 0) {
                std::ifstream f(vJsonPath);
                vJsonText = {std::istreambuf_iterator<char>(f), {}};
            } else {
                auto r = cpr::Get(cpr::Url{versionUrl}, cpr::Timeout{15000});
                if (r.status_code != 200)
                    throw std::runtime_error("Не удалось скачать version.json: HTTP "
                                             + std::to_string(r.status_code));
                vJsonText = r.text;
                std::ofstream(vJsonPath) << vJsonText;
            }
            verData = json::parse(vJsonText);
        }

        // ── Список задач для скачивания ───────────────────────────────────────
        std::vector<DownloadTask> tasks;
        std::vector<fs::path>     nativesToExtract;

        // ── Шаг 2: client.jar (Prism: MinecraftDownloadTask) ─────────────────
        if (verData.contains("downloads") && verData["downloads"].contains("client")) {
            auto& cl = verData["downloads"]["client"];
            tasks.push_back({ cl["url"], verDir / (versionId + ".jar") });
            downloadBytesTotal += cl.value("size", (uint64_t)0);
        }

        // ── Шаг 3: Библиотеки (Prism: LibrariesTask) ─────────────────────────
        // Prism проверяет: rules, затем скачивает artifact и classifiers natives
        if (verData.contains("libraries")) {
            for (auto& lib : verData["libraries"]) {
                // Проверка rules (OS-фильтр)
                if (lib.contains("rules") && !IsAllowedByRules(lib["rules"]))
                    continue;

                // Обычная библиотека (.jar)
                if (lib.contains("downloads") && lib["downloads"].contains("artifact")) {
                    auto& art = lib["downloads"]["artifact"];
                    std::string url  = art.value("url", "");
                    std::string path = art.value("path", "");
                    if (!url.empty() && !path.empty()) {
                        tasks.push_back({url, libsDir / path});
                        downloadBytesTotal += art.value("size", (uint64_t)0);
                    }
                }

                // Natives — СТАРЫЙ формат (1.7-1.12): поле "natives" -> classifiers
                // Пример: "natives": { "windows": "natives-windows-${arch}" }
                if (lib.contains("natives") && lib["natives"].contains("windows")
                    && lib.contains("downloads") && lib["downloads"].contains("classifiers"))
                {
                    std::string key = lib["natives"]["windows"].get<std::string>();
                    // Заменяем ${arch} на 64 (64-битная Windows)
                    auto pos = key.find("${arch}");
                    if (pos != std::string::npos) key.replace(pos, 7, "64");

                    auto& clsf = lib["downloads"]["classifiers"];
                    if (clsf.contains(key)) {
                        auto& nat  = clsf[key];
                        std::string url  = nat.value("url", "");
                        std::string path = nat.value("path", "");
                        if (!url.empty() && !path.empty()) {
                            fs::path dest = libsDir / path;
                            tasks.push_back({url, dest});
                            nativesToExtract.push_back(dest);
                            downloadBytesTotal += nat.value("size", (uint64_t)0);
                        }
                    }
                }

                // Natives — НОВЫЙ формат (1.13+): classifiers с ключами
                // "natives-windows", "natives-windows-64", "natives-windows-x86_64"
                // без поля "natives" в корне lib
                if (!lib.contains("natives")
                    && lib.contains("downloads")
                    && lib["downloads"].contains("classifiers"))
                {
                    auto& clsf = lib["downloads"]["classifiers"];
                    for (auto& key : {"natives-windows-64", "natives-windows-x86_64",
                                      "natives-windows"})
                    {
                        if (!clsf.contains(key)) continue;
                        auto& nat  = clsf[key];
                        std::string url  = nat.value("url", "");
                        std::string path = nat.value("path", "");
                        if (!url.empty() && !path.empty()) {
                            fs::path dest = libsDir / path;
                            tasks.push_back({url, dest});
                            nativesToExtract.push_back(dest);
                            downloadBytesTotal += nat.value("size", (uint64_t)0);
                        }
                        break; // берём только первый подходящий ключ
                    }
                }
            }
        }

        // ── Шаг 4: Asset index (Prism: AssetUpdateTask) ──────────────────────
        std::string assetId;
        if (verData.contains("assetIndex")) {
            assetId = verData["assetIndex"].value("id", "legacy");
            std::string indexUrl = verData["assetIndex"].value("url", "");
            fs::path indexPath   = assetsDir / "indexes" / (assetId + ".json");

            // Скачиваем индекс сразу (он нужен для следующего шага)
            if (!fs::exists(indexPath) || fs::file_size(indexPath) == 0) {
                auto r = cpr::Get(cpr::Url{indexUrl}, cpr::Timeout{10000});
                if (r.status_code == 200) {
                    fs::create_directories(indexPath.parent_path());
                    std::ofstream(indexPath) << r.text;
                }
            }

            // ── Шаг 5: Assets objects ────────────────────────────────────────
            if (fs::exists(indexPath)) {
                std::ifstream f(indexPath);
                json assetIndex = json::parse(f);
                if (assetIndex.contains("objects")) {
                    for (auto& [key, obj] : assetIndex["objects"].items()) {
                        std::string hash   = obj["hash"];
                        std::string prefix = hash.substr(0, 2);
                        fs::path dest = assetsDir / "objects" / prefix / hash;
                        tasks.push_back({
                            "https://resources.download.minecraft.net/" + prefix + "/" + hash,
                            dest
                        });
                        downloadBytesTotal += obj.value("size", (uint64_t)0);
                    }
                }
            }
        }

        // ── Скачиваем всё параллельно (16 потоков, как Prism NetJob) ─────────
        downloadFilesTotal = (int)tasks.size();
        downloadFilesDone  = 0;
        setStage("Скачивание файлов...");

        std::thread statsThread(UpdateDownloadStats);
        std::atomic<size_t> idx{0};

        auto worker = [&]() {
            while (!cancelDownload) {
                size_t i = idx.fetch_add(1);
                if (i >= tasks.size()) break;
                DownloadFile(tasks[i].url, tasks[i].destPath);
                downloadFilesDone++;
            }
        };

        std::vector<std::thread> pool;
        for (int i = 0; i < 16; i++) pool.emplace_back(worker);
        for (auto& t : pool) t.join();
        statsThread.join();

        if (cancelDownload) throw std::runtime_error("Установка отменена.");

        // ── Шаг 6: Распаковка natives (Prism: ExtractNatives) ────────────────
        setStage("Распаковка нативных библиотек...");
        for (auto& p : nativesToExtract)
            ExtractNativesZip(p, nativesDir);

        // ── Готово ────────────────────────────────────────────────────────────
        setStage("Готово! " + versionId + " установлен.");
        {
            json info;
            info["name"]       = name;
            info["mc_version"] = versionId;
            info["mod_loader"] = loader;
            info["status"]     = "ready";
            std::ofstream(basePath / "instance_info.json") << info.dump(4);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        isDownloading       = false;
        needReloadInstances = true;

    } catch (const std::exception& e) {
        { std::lock_guard<std::mutex> lk(uiMutex);
          downloadingStageLabel = std::string("Ошибка: ") + e.what(); }
        isDownloading       = false;
        needReloadInstances = true;
    }
}
