#include "Downloader.h"
#include "Launcher.h"
#include "VersionUtils.h"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <set>
#include <algorithm>

using json = nlohmann::json;
namespace fs = std::filesystem;

std::vector<McVersion> realMcVersions;
std::atomic<bool> isDownloading{false};
std::atomic<int> downloadFilesTotal{0};
std::atomic<int> downloadFilesDone{0};
std::atomic<bool> cancelDownload{false};

std::string downloadingInstanceName = "";
std::string downloadingStageLabel = "";
std::mutex uiMutex;

std::atomic<uint64_t> downloadBytesTotal{0};
std::atomic<uint64_t> downloadBytesDone{0};
std::atomic<double> downloadSpeedMBps{0.0};
std::atomic<int> downloadETASeconds{0};
std::string downloadingCurrentFile = "";

std::atomic<bool> runStatsThread{false};

bool DownloadFile(const std::string& url, const fs::path& destPath, const std::string& expectedSha1, uint64_t expectedSize) {
    fs::create_directories(destPath.parent_path());

    auto isValidExisting = [&]() -> bool {
        if (!fs::exists(destPath)) return false;
        std::error_code ec;
        uint64_t sz = fs::file_size(destPath, ec);
        if (ec || sz == 0) return false;
        if (expectedSize > 0 && sz != expectedSize) return false;
        if (!expectedSha1.empty() && VersionUtils::Sha1File(destPath) != expectedSha1) return false;
        return true;
    };

    if (isValidExisting()) {
        downloadBytesDone += fs::file_size(destPath);
        return true; 
    }

    const int maxAttempts = 3;
    for (int attempt = 1; attempt <= maxAttempts; attempt++) {
        if (cancelDownload) return false;

        {
            std::lock_guard<std::mutex> lock(uiMutex);
            downloadingCurrentFile = destPath.filename().string();
        }

        fs::path tmpPath = destPath;
        tmpPath += ".part";
        
        std::ofstream file(tmpPath, std::ios::binary);
        cpr::Response r = cpr::Download(file, cpr::Url{url}, cpr::Header{{"User-Agent", "SJLauncher/2.0"}});
        file.close(); 

        if (r.status_code == 200) {
            bool ok = true;
            std::error_code ec;
            uint64_t downloadedSize = fs::file_size(tmpPath, ec);

            if (expectedSize > 0 && downloadedSize != expectedSize) ok = false;
            if (ok && !expectedSha1.empty() && VersionUtils::Sha1File(tmpPath) != expectedSha1) ok = false;

            if (ok) {
                fs::rename(tmpPath, destPath, ec);
                if (ec) {
                    fs::copy_file(tmpPath, destPath, fs::copy_options::overwrite_existing, ec);
                    fs::remove(tmpPath, ec);
                }
                downloadBytesDone += downloadedSize;
                return true;
            }
        }
        
        std::error_code rm_ec;
        fs::remove(tmpPath, rm_ec);
        std::this_thread::sleep_for(std::chrono::milliseconds(300 * attempt));
    }

    if (fs::exists(destPath)) fs::remove(destPath);
    return false;
}

void UpdateDownloadStats() {
    auto lastTime = std::chrono::steady_clock::now();
    uint64_t lastBytes = 0;

    while (runStatsThread && !cancelDownload) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - lastTime).count();

        uint64_t currentBytes = downloadBytesDone.load();
        uint64_t bytesDelta = currentBytes - lastBytes;

        double speedMBps = (bytesDelta / (1024.0 * 1024.0)) / elapsed;
        downloadSpeedMBps.store(speedMBps);

        uint64_t totalB = downloadBytesTotal.load();
        uint64_t bytesRemaining = (totalB > currentBytes) ? (totalB - currentBytes) : 0;
        if (speedMBps > 0.1) {
            int eta = static_cast<int>((bytesRemaining / (1024.0 * 1024.0)) / speedMBps);
            downloadETASeconds.store(eta);
        }

        lastTime = now;
        lastBytes = currentBytes;
    }
}

void FetchMinecraftVersions() {
    try {
        cpr::Response r = cpr::Get(cpr::Url{"https://piston-meta.mojang.com/mc/game/version_manifest_v2.json"});
        if (r.status_code == 200) {
            json manifest = json::parse(r.text);
            realMcVersions.clear();
            for (const auto& v : manifest["versions"]) {
                std::string type = v.value("type", "");
                realMcVersions.push_back({v["id"], v["url"], type});
            }
        }
    } catch (...) {}
}

static json FetchVersionJsonById(const std::string& versionId) {
    for (const auto& v : realMcVersions) {
        if (v.id == versionId) {
            try {
                cpr::Response r = cpr::Get(cpr::Url{v.url});
                if (r.status_code == 200) return json::parse(r.text);
            } catch (...) {}
        }
    }
    return json();
}

void InstallInstanceThread(std::string name, std::string versionId, std::string versionUrl, std::string loader, std::string group) {
    try {
        cancelDownload = false;
        isDownloading = true;
        runStatsThread = true;
        downloadFilesTotal = 0; downloadFilesDone = 0;
        downloadBytesTotal = 0; downloadBytesDone = 0;
        downloadSpeedMBps = 0.0; downloadETASeconds = 0;

        {
            std::lock_guard<std::mutex> lk(uiMutex);
            downloadingInstanceName = name;
            downloadingStageLabel = "Подготовка структуры...";
            downloadingCurrentFile = "";
        }

        fs::path basePath = GetInstancesRootDir() / name;
        fs::path mcDir = basePath / ".minecraft";
        fs::path verDir = mcDir / "versions" / versionId;
        fs::path libsDir = mcDir / "libraries";
        fs::path assetsDir = mcDir / "assets";
        fs::path nativesDir = verDir / "natives";
        fs::path runtimesDir = GetExeDir() / "runtimes";

        fs::create_directories(verDir);
        fs::create_directories(libsDir);
        fs::create_directories(nativesDir);
        fs::create_directories(assetsDir / "indexes");
        fs::create_directories(assetsDir / "objects");
        fs::create_directories(mcDir / "mods");
        fs::create_directories(mcDir / "config");
        fs::create_directories(mcDir / "resourcepacks");
        fs::create_directories(mcDir / "saves");

        json info;
        info["name"] = name;
        info["mc_version"] = versionId;
        info["mod_loader"] = loader;
        info["status"] = "downloading";
        std::ofstream(basePath / "instance_info.json") << info.dump(4);

        json verData;

        // === ПОДДЕРЖКА ЗАГРУЗЧИКОВ ===
        if (loader == "Fabric" || loader == "Quilt") {
            std::string metaUrl = (loader == "Fabric") ? "https://meta.fabricmc.net/v2/versions/loader/" 
                                                       : "https://meta.quiltmc.org/v3/versions/loader/";
            cpr::Response fRes = cpr::Get(cpr::Url{metaUrl + versionId});
            if (fRes.status_code == 200) {
                json fLoaders = json::parse(fRes.text);
                if (!fLoaders.empty()) {
                    std::string loaderVer = fLoaders[0]["loader"]["version"];
                    cpr::Response pRes = cpr::Get(cpr::Url{metaUrl + versionId + "/" + loaderVer + "/profile/json"});
                    verData = json::parse(pRes.text);
                } else {
                    throw std::runtime_error(loader + " не поддерживает версию " + versionId);
                }
            } else { throw std::runtime_error("Ошибка API " + loader + "!"); }
        } 
        else if (loader == "Forge" || loader == "NeoForge") {
            std::string metaLoader = (loader == "Forge") ? "net.minecraftforge" : "net.neoforged";
            cpr::Response fRes = cpr::Get(cpr::Url{"https://meta.prismlauncher.org/v1/" + metaLoader + "/"});
            
            if (fRes.status_code == 200) {
                json fLoaders = json::parse(fRes.text);
                std::string targetVersion = "";
                if (fLoaders.contains("versions")) {
                    for (const auto& v : fLoaders["versions"]) {
                        if (v.contains("requires") && v["requires"].contains("net.minecraft")) {
                            if (v["requires"]["net.minecraft"][0] == "^" + versionId || v["requires"]["net.minecraft"][0] == versionId) {
                                targetVersion = v["version"];
                                break;
                            }
                        }
                    }
                }
                if (!targetVersion.empty()) {
                    cpr::Response pRes = cpr::Get(cpr::Url{"https://meta.prismlauncher.org/v1/" + metaLoader + "/" + targetVersion + ".json"});
                    verData = json::parse(pRes.text);
                } else {
                    throw std::runtime_error(loader + " не поддерживает версию " + versionId);
                }
            } else { throw std::runtime_error("Ошибка API " + loader + "!"); }
        }
        else {
            cpr::Response verRes = cpr::Get(cpr::Url{versionUrl});
            if (verRes.status_code != 200) throw std::runtime_error("Ошибка сети Mojang!");
            verData = json::parse(verRes.text);
        }

        verData = VersionUtils::MergeInherited(verData, FetchVersionJsonById);
        std::ofstream(verDir / (versionId + ".json")) << verData.dump(4);

        std::vector<DownloadTask> tasks;
        std::vector<std::pair<fs::path, std::vector<std::string>>> nativesToExtract;

        // === ПРОВЕРКА JAVA (AUTO-DOWNLOAD) ===
        int javaMajor = 8;
        if (verData.contains("javaVersion") && verData["javaVersion"].contains("majorVersion")) {
            javaMajor = verData["javaVersion"]["majorVersion"].get<int>();
        } else {
            // Для очень старых версий (до 1.13), где Mojang не писали версию Java
            if (versionId.find("1.17") != std::string::npos) javaMajor = 16;
            else if (versionId.find("1.18") != std::string::npos || versionId.find("1.19") != std::string::npos || versionId.find("1.20") != std::string::npos) javaMajor = 17;
            else if (versionId.find("1.21") != std::string::npos) javaMajor = 21;
        }

        fs::path javaDir = runtimesDir / ("java-" + std::to_string(javaMajor));
        fs::path javaZip = runtimesDir / ("java-" + std::to_string(javaMajor) + ".zip");
        
        bool needJava = !fs::exists(javaDir);
        if (needJava) {
            // API Adoptium (Eclipse Temurin) выдает JRE нужной версии
            std::string javaUrl = "https://api.adoptium.net/v3/binary/latest/" + std::to_string(javaMajor) + "/ga/windows/x64/jre/hotspot/normal/eclipse";
            // Внимание: добавляем скачивание Java в общую очередь (ее размер ~40мб)
            tasks.push_back({javaUrl, javaZip, "", 0ULL});
        }

        // Клиентский JAR 
        if (verData.contains("downloads") && verData["downloads"].contains("client")) {
            std::string url = verData["downloads"]["client"]["url"];
            std::string sha1 = verData["downloads"]["client"].value("sha1", "");
            uint64_t size = verData["downloads"]["client"].value("size", 0ULL);
            tasks.push_back({url, verDir / (versionId + ".jar"), sha1, size});
            downloadBytesTotal += size;
        }

        // Библиотеки
        if (verData.contains("libraries")) {
            std::set<std::string> seenPaths;
            for (const auto& lib : verData["libraries"]) {
                if (lib.contains("rules") && !VersionUtils::RulesAllow(lib["rules"])) continue;

                if (lib.contains("downloads") && lib["downloads"].contains("artifact")) {
                    std::string url = lib["downloads"]["artifact"].value("url", "");
                    std::string path = lib["downloads"]["artifact"].value("path", "");
                    std::string sha1 = lib["downloads"]["artifact"].value("sha1", "");
                    uint64_t size = lib["downloads"]["artifact"].value("size", 0ULL);
                    if (!url.empty() && !path.empty() && seenPaths.insert(path).second) {
                        tasks.push_back({url, libsDir / path, sha1, size});
                        downloadBytesTotal += size;
                    }
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
                        std::string path = domain + "/" + artifact + "/" + version + "/" + artifact + "-" + version + ".jar";
                        
                        std::string baseUrl = lib.value("url", "https://libraries.minecraft.net/");
                        if (baseUrl.back() != '/') baseUrl += "/";
                        
                        if (seenPaths.insert(path).second) {
                            tasks.push_back({baseUrl + path, libsDir / path, "", 0ULL});
                        }
                    }
                }

                std::string nativeKey = VersionUtils::ResolveNativeClassifier(lib);
                if (!nativeKey.empty() && lib.contains("downloads") &&
                    lib["downloads"].contains("classifiers") &&
                    lib["downloads"]["classifiers"].contains(nativeKey)) {
                    const auto& cls = lib["downloads"]["classifiers"][nativeKey];
                    std::string url = cls.value("url", "");
                    std::string path = cls.value("path", "");
                    if (!url.empty() && !path.empty() && seenPaths.insert(path).second) {
                        fs::path dest = libsDir / path;
                        uint64_t clsSize = cls.value("size", 0ULL);
                        tasks.push_back({url, dest, cls.value("sha1", ""), clsSize});

                        std::vector<std::string> excludes;
                        if (lib.contains("extract") && lib["extract"].contains("exclude")) {
                            for (const auto& e : lib["extract"]["exclude"]) excludes.push_back(e.get<std::string>());
                        }
                        nativesToExtract.push_back({dest, excludes});
                    }
                }
            }
        }

        // Ассеты 
        bool isLegacyLayout = false;
        bool mapToResources = false;
        std::string assetsIndexId = verData.value("assets", "legacy");
        json assetIndexJson;

        if (verData.contains("assetIndex")) {
            std::string indexUrl = verData["assetIndex"].value("url", "");
            std::string indexSha1 = verData["assetIndex"].value("sha1", "");
            assetsIndexId = verData["assetIndex"].value("id", assetsIndexId);
            fs::path indexPath = assetsDir / "indexes" / (assetsIndexId + ".json");

            uint64_t idxSize = verData["assetIndex"].value("size", 0ULL);
            DownloadFile(indexUrl, indexPath, indexSha1, idxSize);

            std::ifstream idxFile(indexPath);
            if (idxFile.is_open()) {
                assetIndexJson = json::parse(idxFile);
                isLegacyLayout = assetIndexJson.value("virtual", false);
                mapToResources = assetIndexJson.value("map_to_resources", false);

                if (assetIndexJson.contains("objects")) {
                    for (auto& [key, obj] : assetIndexJson["objects"].items()) {
                        std::string hash = obj.value("hash", "");
                        if (hash.empty()) continue;
                        std::string subDir = hash.substr(0, 2);
                        uint64_t size = obj.value("size", 0ULL);
                        tasks.push_back({
                            "https://resources.download.minecraft.net/" + subDir + "/" + hash,
                            assetsDir / "objects" / subDir / hash,
                            hash, size
                        });
                        downloadBytesTotal += size;
                    }
                }
            }
        }

        // --- ЗАПУСК ПОТОКОВ СКАЧИВАНИЯ ---
        downloadFilesTotal = static_cast<int>(tasks.size());
        {
            std::lock_guard<std::mutex> lock(uiMutex);
            downloadingStageLabel = "Скачивание файлов...";
        }

        std::thread statsThread(UpdateDownloadStats);

        std::atomic<size_t> nextTaskIdx{0};
        std::atomic<bool> anyFailed{false};
        
        auto worker = [&]() {
            while (!cancelDownload) {
                size_t idx = nextTaskIdx.fetch_add(1);
                if (idx >= tasks.size()) break;

                if (!DownloadFile(tasks[idx].url, tasks[idx].destPath, tasks[idx].sha1, tasks[idx].size)) {
                    anyFailed = true; 
                }
                downloadFilesDone++;
            }
        };

        std::vector<std::thread> pool;
        for (int i = 0; i < 16; i++) pool.emplace_back(worker);
        for (auto& t : pool) t.join();

        runStatsThread = false; 
        statsThread.join();

        if (cancelDownload) throw std::runtime_error("Отменено пользователем!");

        // --- РАСПАКОВКА JAVA ---
        if (needJava && fs::exists(javaZip)) {
            {
                std::lock_guard<std::mutex> lock(uiMutex);
                downloadingStageLabel = "Распаковка Java " + std::to_string(javaMajor) + "...";
            }
            VersionUtils::ExtractZip(javaZip, javaDir);
            std::error_code ec;
            fs::remove(javaZip, ec);
        }

        // --- ПОСТОБРАБОТКА (Legacy ассеты) ---
        if ((isLegacyLayout || mapToResources) && assetIndexJson.contains("objects")) {
            std::lock_guard<std::mutex> lock(uiMutex);
            downloadingStageLabel = "Раскладка ассетов (legacy)...";

            fs::path legacyRoot = mapToResources ? mcDir / "resources" : assetsDir / "virtual" / "legacy";
            for (auto& [key, obj] : assetIndexJson["objects"].items()) {
                std::string hash = obj.value("hash", "");
                if (hash.empty()) continue;
                fs::path src = assetsDir / "objects" / hash.substr(0, 2) / hash;
                fs::path dst = legacyRoot / key; 
                if (!fs::exists(src)) continue;
                fs::create_directories(dst.parent_path());
                if (!fs::exists(dst)) {
                    std::error_code ec;
                    fs::create_hard_link(src, dst, ec);
                    if (ec) fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
                }
            }
        }

        // --- РАСПАКОВКА NATIVES ---
        {
            std::lock_guard<std::mutex> lock(uiMutex);
            downloadingStageLabel = "Распаковка нативных библиотек...";
        }
        for (const auto& [jarPath, excludes] : nativesToExtract) {
            if (fs::exists(jarPath)) VersionUtils::ExtractNativesJar(jarPath, nativesDir, excludes);
        }

        {
            std::lock_guard<std::mutex> lock(uiMutex);
            downloadingStageLabel = anyFailed ? "Готово (с ошибками, часть файлов не скачалась)" : "Готово!";
        }

        info["status"] = "ready";
        std::ofstream(basePath / "instance_info.json") << info.dump(4);
        if (!group.empty()) SetInstanceGroup(name, group);

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        isDownloading = false;
        needReloadInstances = true;

    } catch (const std::exception& e) {
        {
            std::lock_guard<std::mutex> lock(uiMutex);
            downloadingStageLabel = std::string("Ошибка: ") + e.what();
        }
        runStatsThread = false;
        isDownloading = false;
        needReloadInstances = true;
    }
}
