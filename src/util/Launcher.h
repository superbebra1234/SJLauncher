#pragma once
#include "model/Types.h"
#include <vector>
#include <atomic>
#include <filesystem>

extern std::vector<MyInstance> myInstancesList;
extern std::atomic<bool> needReloadInstances;

// Добавляем объявление функции, чтобы Downloader.cpp её увидел
std::filesystem::path GetExeDir(); 

void LoadMyInstances();
void LaunchGame(const MyInstance& inst);