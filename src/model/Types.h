#pragma once
#include <string>
#include <filesystem>

struct McVersion { std::string id; std::string url; std::string type; };
struct MyInstance { std::string name; std::string mc_version; std::string mod_loader; std::string status; };
struct DownloadTask { std::string url; std::filesystem::path destPath; };