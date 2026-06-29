#pragma once
#include "model/Types.h"
#include <vector>
#include <atomic>
#include <mutex>
#include <string>

extern std::vector<McVersion> realMcVersions;
extern std::atomic<bool> isDownloading;
extern std::atomic<int> downloadFilesTotal;
extern std::atomic<int> downloadFilesDone;
extern std::atomic<bool> cancelDownload;
extern std::string downloadingInstanceName;
extern std::string downloadingStageLabel;
extern std::mutex uiMutex;

extern std::atomic<uint64_t> downloadBytesTotal;
extern std::atomic<uint64_t> downloadBytesDone;
extern std::atomic<double> downloadSpeedMBps;
extern std::atomic<int> downloadETASeconds;
extern std::string downloadingCurrentFile;

void FetchMinecraftVersions();
void InstallInstanceThread(std::string name, std::string versionId, std::string versionUrl, std::string loader);
bool DownloadFile(const std::string& url, const std::filesystem::path& destPath);