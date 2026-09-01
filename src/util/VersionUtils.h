#pragma once
#include <string>
#include <map>
#include <vector>
#include <functional>
#include <nlohmann/json.hpp>
#include <filesystem>

namespace VersionUtils {

    std::string CurrentOsName();   
    std::string CurrentArchBits(); 

    bool RulesAllow(const nlohmann::json& rules, const std::vector<std::string>& activeFeatures = {});
    std::string ResolveNativeClassifier(const nlohmann::json& lib);
    std::string SubstituteVars(std::string str, const std::map<std::string, std::string>& vars);

    std::vector<std::string> ExtractGameArgs(const nlohmann::json& versionJson);
    std::vector<std::string> ExtractJvmArgs(const nlohmann::json& versionJson);

    nlohmann::json MergeInherited(nlohmann::json child, const std::function<nlohmann::json(const std::string&)>& fetchParentJson);

    std::string Sha1File(const std::filesystem::path& path);
    std::string Sha1Hex(const std::string& data);

    bool ExtractNativesJar(const std::filesystem::path& jarPath, const std::filesystem::path& destDir, const std::vector<std::string>& excludes);
    
    // НОВАЯ ФУНКЦИЯ ДЛЯ РАСПАКОВКИ JAVA ZIP
    bool ExtractZip(const std::filesystem::path& zipPath, const std::filesystem::path& destDir);
}