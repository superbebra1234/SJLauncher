#include "VersionUtils.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <functional>
#include <zip.h>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace VersionUtils {

std::string CurrentOsName() {
#ifdef _WIN32
    return "windows";
#elif __APPLE__
    return "osx";
#else
    return "linux";
#endif
}

std::string CurrentArchBits() {
    return (sizeof(void*) == 8) ? "64" : "32";
}

bool RulesAllow(const json& rules, const std::vector<std::string>& activeFeatures) {
    if (!rules.is_array() || rules.empty()) return true;

    bool allowed = false;
    std::string osName = CurrentOsName();
    std::string arch = CurrentArchBits();

    for (const auto& rule : rules) {
        std::string action = rule.value("action", "allow");
        bool ruleMatches = true;

        if (rule.contains("os")) {
            const auto& os = rule["os"];
            if (os.contains("name") && os.value("name", "") != osName) ruleMatches = false;
            if (ruleMatches && os.contains("arch")) {
                std::string wantArch = os.value("arch", "");
                bool want64 = (wantArch.find("64") != std::string::npos);
                bool want32 = (wantArch.find("86") != std::string::npos) && !want64;
                if (want64 && arch != "64") ruleMatches = false;
                if (want32 && arch != "32") ruleMatches = false;
            }
        }

        if (ruleMatches && rule.contains("features")) {
            for (auto& [key, val] : rule["features"].items()) {
                bool needed = val.is_boolean() ? val.get<bool>() : true;
                bool have = std::find(activeFeatures.begin(), activeFeatures.end(), key) != activeFeatures.end();
                if (needed != have) { ruleMatches = false; break; }
            }
        }

        if (ruleMatches) allowed = (action == "allow");
    }
    return allowed;
}

std::string ResolveNativeClassifier(const json& lib) {
    if (!lib.contains("natives")) return "";
    std::string osName = CurrentOsName();
    if (!lib["natives"].contains(osName)) return "";

    std::string classifier = lib["natives"].value(osName, "");
    size_t pos = classifier.find("${arch}");
    if (pos != std::string::npos) {
        classifier.replace(pos, 7, CurrentArchBits());
    }
    return classifier;
}

std::string SubstituteVars(std::string str, const std::map<std::string, std::string>& vars) {
    for (const auto& [key, val] : vars) {
        std::string token = "${" + key + "}";
        size_t pos = 0;
        while ((pos = str.find(token, pos)) != std::string::npos) {
            str.replace(pos, token.length(), val);
            pos += val.length();
        }
    }
    return str;
}

std::vector<std::string> ExtractGameArgs(const json& versionJson) {
    std::vector<std::string> out;
    if (versionJson.contains("minecraftArguments") && versionJson["minecraftArguments"].is_string()) {
        std::istringstream iss(versionJson["minecraftArguments"].get<std::string>());
        std::string tok;
        while (iss >> tok) out.push_back(tok);
        return out;
    }
    if (versionJson.contains("arguments") && versionJson["arguments"].contains("game")) {
        for (const auto& entry : versionJson["arguments"]["game"]) {
            if (entry.is_string()) {
                out.push_back(entry.get<std::string>());
            } else if (entry.is_object() && entry.contains("value")) {
                if (entry.contains("rules") && !RulesAllow(entry["rules"])) continue;
                if (entry["value"].is_string()) {
                    out.push_back(entry["value"].get<std::string>());
                } else if (entry["value"].is_array()) {
                    for (const auto& v : entry["value"]) out.push_back(v.get<std::string>());
                }
            }
        }
    }
    return out;
}

std::vector<std::string> ExtractJvmArgs(const json& versionJson) {
    std::vector<std::string> out;
    if (versionJson.contains("arguments") && versionJson["arguments"].contains("jvm")) {
        for (const auto& entry : versionJson["arguments"]["jvm"]) {
            if (entry.is_string()) {
                out.push_back(entry.get<std::string>());
            } else if (entry.is_object() && entry.contains("value")) {
                if (entry.contains("rules") && !RulesAllow(entry["rules"])) continue;
                if (entry["value"].is_string()) {
                    out.push_back(entry["value"].get<std::string>());
                } else if (entry["value"].is_array()) {
                    for (const auto& v : entry["value"]) out.push_back(v.get<std::string>());
                }
            }
        }
    }
    return out;
}

static void mergeLibraries(json& target, const json& toAdd) {
    if (!toAdd.is_array()) return;
    if (!target.is_array()) target = json::array();
    for (const auto& lib : toAdd) {
        std::string name = lib.value("name", "");
        bool exists = false;
        for (const auto& existing : target) {
            if (existing.value("name", "") == name) { exists = true; break; }
        }
        if (!exists) target.push_back(lib);
    }
}

static void mergeArgList(json& target, const json& toAdd) {
    if (!toAdd.is_array()) return;
    if (!target.is_array()) target = json::array();
    for (const auto& a : toAdd) target.push_back(a);
}

json MergeInherited(json child, const std::function<json(const std::string&)>& fetchParentJson) {
    if (!child.contains("inheritsFrom")) return child;

    std::string parentId = child["inheritsFrom"].get<std::string>();
    json parent = fetchParentJson(parentId);
    if (parent.is_null() || parent.empty()) return child; 

    parent = MergeInherited(parent, fetchParentJson);
    json merged = parent; 

    for (const char* key : {"id", "mainClass", "assets", "minimumLauncherVersion", "type", "time", "releaseTime", "javaVersion"}) {
        if (child.contains(key)) merged[key] = child[key];
    }
    if (child.contains("assetIndex")) merged["assetIndex"] = child["assetIndex"];
    if (child.contains("downloads")) merged["downloads"] = child["downloads"];

    json combinedLibs = child.value("libraries", json::array());
    mergeLibraries(combinedLibs, parent.value("libraries", json::array()));
    merged["libraries"] = combinedLibs;

    if (child.contains("minecraftArguments")) merged["minecraftArguments"] = child["minecraftArguments"];

    if (child.contains("arguments") || parent.contains("arguments")) {
        json args = parent.value("arguments", json::object());
        if (child.contains("arguments")) {
            if (child["arguments"].contains("game")) mergeArgList(args["game"], child["arguments"]["game"]);
            if (child["arguments"].contains("jvm")) mergeArgList(args["jvm"], child["arguments"]["jvm"]);
        }
        merged["arguments"] = args;
    }

    return merged;
}

namespace {
    struct SHA1Ctx {
        uint32_t h[5] = {0x67452301,0xEFCDAB89,0x98BADCFE,0x10325476,0xC3D2E1F0};
        uint64_t len = 0;
        uint8_t buf[64];
        size_t bufLen = 0;

        static uint32_t rol(uint32_t v, int b) { return (v << b) | (v >> (32 - b)); }

        void processBlock(const uint8_t* p) {
            uint32_t w[80];
            for (int i = 0; i < 16; i++)
                w[i] = (p[i*4]<<24)|(p[i*4+1]<<16)|(p[i*4+2]<<8)|p[i*4+3];
            for (int i = 16; i < 80; i++)
                w[i] = rol(w[i-3]^w[i-8]^w[i-14]^w[i-16], 1);

            uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4];
            for (int i = 0; i < 80; i++) {
                uint32_t f, k;
                if (i < 20) { f = (b & c) | ((~b) & d); k = 0x5A827999; }
                else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
                else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
                else { f = b ^ c ^ d; k = 0xCA62C1D6; }
                uint32_t temp = rol(a,5) + f + e + k + w[i];
                e = d; d = c; c = rol(b,30); b = a; a = temp;
            }
            h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e;
        }

        void update(const uint8_t* data, size_t size) {
            len += size;
            while (size > 0) {
                size_t take = std::min(size, (size_t)64 - bufLen);
                std::memcpy(buf + bufLen, data, take);
                bufLen += take; data += take; size -= take;
                if (bufLen == 64) { processBlock(buf); bufLen = 0; }
            }
        }

        std::string finalize() {
            uint64_t bitLen = len * 8;
            uint8_t pad = 0x80;
            update(&pad, 1);
            uint8_t zero = 0;
            while (bufLen != 56) update(&zero, 1);
            uint8_t lenBytes[8];
            for (int i = 0; i < 8; i++) lenBytes[i] = (uint8_t)(bitLen >> (56 - i*8));
            update(lenBytes, 8);

            std::ostringstream oss;
            for (int i = 0; i < 5; i++) {
                oss << std::hex << std::setw(8) << std::setfill('0') << h[i];
            }
            return oss.str();
        }
    };
}

std::string Sha1Hex(const std::string& data) {
    SHA1Ctx ctx;
    ctx.update(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    return ctx.finalize();
}

std::string Sha1File(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return "";
    SHA1Ctx ctx;
    std::vector<char> chunk(1 << 16);
    while (f.read(chunk.data(), chunk.size()) || f.gcount() > 0) {
        ctx.update(reinterpret_cast<const uint8_t*>(chunk.data()), (size_t)f.gcount());
    }
    return ctx.finalize();
}

bool ExtractNativesJar(const fs::path& jarPath, const fs::path& destDir, const std::vector<std::string>& excludes) {
    int errCode = 0;
    zip_t* archive = zip_open(jarPath.string().c_str(), ZIP_RDONLY, &errCode);
    if (!archive) return false;

    fs::create_directories(destDir);
    zip_int64_t numEntries = zip_get_num_entries(archive, 0);
    for (zip_int64_t i = 0; i < numEntries; i++) {
        const char* nameC = zip_get_name(archive, i, 0);
        if (!nameC) continue;
        std::string name = nameC;

        if (name.rfind("META-INF/", 0) == 0) continue;
        if (!name.empty() && name.back() == '/') continue;

        bool excluded = false;
        for (const auto& ex : excludes) {
            if (name.rfind(ex, 0) == 0) { excluded = true; break; }
        }
        if (excluded) continue;

        zip_stat_t st;
        if (zip_stat_index(archive, i, 0, &st) != 0) continue;
        zip_file_t* zf = zip_fopen_index(archive, i, 0);
        if (!zf) continue;

        fs::path outPath = destDir / name;
        fs::create_directories(outPath.parent_path());

        std::ofstream out(outPath, std::ios::binary);
        if (out.is_open()) {
            std::vector<char> buf(1 << 16);
            zip_int64_t remaining = (zip_int64_t)st.size;
            while (remaining > 0) {
                zip_int64_t got = zip_fread(zf, buf.data(), std::min((size_t)remaining, buf.size()));
                if (got <= 0) break;
                out.write(buf.data(), got);
                remaining -= got;
            }
        }
        zip_fclose(zf);
    }
    zip_close(archive);
    return true;
}

// НОВАЯ ФУНКЦИЯ ДЛЯ РАСПАКОВКИ JAVA
bool ExtractZip(const fs::path& zipPath, const fs::path& destDir) {
    int errCode = 0;
    zip_t* archive = zip_open(zipPath.string().c_str(), ZIP_RDONLY, &errCode);
    if (!archive) return false;

    fs::create_directories(destDir);
    zip_int64_t numEntries = zip_get_num_entries(archive, 0);
    
    for (zip_int64_t i = 0; i < numEntries; i++) {
        const char* nameC = zip_get_name(archive, i, 0);
        if (!nameC) continue;
        std::string name = nameC;
        
        fs::path outPath = destDir / name;
        
        if (!name.empty() && name.back() == '/') {
            fs::create_directories(outPath);
            continue;
        }
        fs::create_directories(outPath.parent_path());
        
        zip_stat_t st;
        if (zip_stat_index(archive, i, 0, &st) != 0) continue;
        zip_file_t* zf = zip_fopen_index(archive, i, 0);
        if (!zf) continue;
        
        std::ofstream out(outPath, std::ios::binary);
        if (out.is_open()) {
            std::vector<char> buf(1 << 16);
            zip_int64_t remaining = (zip_int64_t)st.size;
            while (remaining > 0) {
                zip_int64_t got = zip_fread(zf, buf.data(), std::min((size_t)remaining, buf.size()));
                if (got <= 0) break;
                out.write(buf.data(), got);
                remaining -= got;
            }
        }
        zip_fclose(zf);
    }
    zip_close(archive);
    return true;
}

} // namespace VersionUtils