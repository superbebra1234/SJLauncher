#pragma once
#include "model/Types.h"
#include <vector>
#include <atomic>
#include <filesystem>
#include <string>
#include <utility>

extern std::vector<MyInstance> myInstancesList;
extern std::atomic<bool> needReloadInstances;

// Добавляем объявление функции, чтобы Downloader.cpp её увидел
std::filesystem::path GetExeDir();

// Корневая папка инстансов (currentSettings.instancesDir, либо <exeDir>/instances)
std::filesystem::path GetInstancesRootDir();
// Папка конкретного инстанса
std::filesystem::path GetInstanceDir(const std::string& name);

void LoadMyInstances();
void LaunchGame(const MyInstance& inst);

// ─── Управление инстансами (аналог PolyMC: rename/copy/delete + per-instance cfg) ─
bool RenameInstance(const std::string& oldName, const std::string& newName, std::string* errorOut = nullptr);
bool DuplicateInstance(const std::string& srcName, const std::string& newName, std::string* errorOut = nullptr);
bool DeleteInstance(const std::string& name);
void OpenInstanceFolder(const std::string& name);

// Загрузка/сохранение per-instance overrides (instance_settings.json) и заметок
InstanceOverrides LoadInstanceOverrides(const std::string& name);
void SaveInstanceOverrides(const std::string& name, const InstanceOverrides& ov);
std::string LoadInstanceNotes(const std::string& name);
void SaveInstanceNotes(const std::string& name, const std::string& notes);

// ─── Группы инстансов (формат instgroups.json как в PolyMC) ─────────────────────
// Возвращает список имён групп в порядке, в котором они хранятся в файле,
// плюс всегда добавляет "" (Ungrouped) в конец, если есть инстансы без группы.
std::vector<std::string> GetAllGroupNames();
std::string GetInstanceGroup(const std::string& name);
void SetInstanceGroup(const std::string& name, const std::string& group);
void RenameGroup(const std::string& oldGroup, const std::string& newGroup);
void DeleteGroup(const std::string& group); // переносит инстансы в Ungrouped
