#include "update.hpp"
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <sstream>
#include <filesystem>
#include <prompts/prompts.hpp>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <funcs/spinner.hpp>

namespace fs = std::filesystem;

namespace update {

// Paths
inline const std::string CONFIG = std::string(getenv("HOME")) + "/.config/nucleus-shell/config/configuration.json";
inline const std::string QS_DIR = std::string(getenv("HOME")) + "/.config/quickshell/nucleus-shell";
inline const std::string REPO = "xZepyx/nucleus-shell";
inline const std::string API = "https://api.github.com/repos/" + REPO + "/releases";

// CURL helper
inline size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    std::string* str = static_cast<std::string*>(userdata);
    str->append(ptr, size * nmemb);
    return size * nmemb;
}

// Fetch latest release tag from GitHub
inline std::string fetchLatestTag(bool stable_only) {
    prompt::Stage("Fetching release info");

    std::string response;
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Failed to init curl");

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "User-Agent: nucleus-shell-updater");

    curl_easy_setopt(curl, CURLOPT_URL, API.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) throw std::runtime_error("CURL failed: " + std::string(curl_easy_strerror(res)));

    // suppress unused parameter warning
    (void)stable_only;

    try {
        auto releases = nlohmann::json::parse(response);

        nlohmann::json latest_release;
        for (auto& r : releases) {
            if (r.value("draft", true)) continue;
            if (stable_only && r.value("prerelease", true)) continue;
            if (latest_release.is_null() || r.value("published_at", "") > latest_release.value("published_at", "")) {
                latest_release = r;
            }
        }

        if (latest_release.is_null()) throw std::runtime_error("Release resolution failed");

        std::string tag_name = latest_release.value("tag_name", "");
        if (!tag_name.empty() && tag_name[0] == 'v') tag_name.erase(0, 1);

        return tag_name;
    } catch (nlohmann::json::parse_error& e) {
        std::cerr << "Failed to parse GitHub API response:\n" << response << "\n";
        throw;
    }
}

// Convert choice integer to UpdateMode
UpdateMode choiceToMode(int choice) {
    switch (choice) {
        case 1: return UpdateMode::Stable;
        case 2: return UpdateMode::Edge;
        case 3: return UpdateMode::Git;
        default: throw std::runtime_error("Invalid choice");
    }
}

// Main update logic
void perform(UpdateMode mode, const std::string& gitTag) {
    if (!fs::exists(CONFIG)) throw std::runtime_error("configuration.json not found");

    // Read current version
    std::ifstream cfg(CONFIG);
    nlohmann::json cfgJson;
    cfg >> cfgJson;
    cfg.close();

    std::string current = cfgJson.value("shell", nlohmann::json::object()).value("version", "");
    if (current.empty()) throw std::runtime_error("Current version not set");

    std::string latest;
    std::string latest_tag;

    if (mode == UpdateMode::Git) {
        if (gitTag.empty()) {
            std::cerr << "[!] No git tag provided. Aborting update.\n";
            return;
        }
        latest = (gitTag[0] == 'v') ? gitTag.substr(1) : gitTag;
        latest_tag = "v" + latest;
    } else {
        bool stable = (mode == UpdateMode::Stable);
        latest = fetchLatestTag(stable);
        latest_tag = "v" + latest;
    }

    if (latest == current) {
        std::cout << "[*] Already up to date (" << current << ")\n";
        return;
    }

    // Temp workspace
    fs::path tmp = fs::temp_directory_path() / "nucleus-update";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    std::string zipPath = (tmp / "source.zip").string();
    fs::path root_dir = tmp / ("nucleus-shell-" + latest);
    fs::path src_dir = root_dir / "quickshell" / "nucleus-shell";

    // Download
    std::string downloadCmd = "curl -fsSL https://github.com/" + REPO + "/archive/refs/tags/" + latest_tag + ".zip -o " + zipPath;
    int ret = std::system(downloadCmd.c_str());
    if (ret != 0) {
        std::cerr << "[!] No git tag found: '" << latest_tag << "'. Aborting update.\n";
        return;
    }
    console::spinner("Downloading...", downloadCmd);

    // Extract
    std::string unzipCmd = "unzip -q " + zipPath + " -d " + tmp.string();
    ret = std::system(unzipCmd.c_str());
    if (ret != 0) {
        std::cerr << "[!] Failed to extract the archive. Aborting.\n";
        return;
    }
    console::spinner("Extracting...", unzipCmd);

    if (!fs::exists(src_dir)) {
        std::cerr << "[!] 'nucleus-shell' directory missing in archive. Aborting.\n";
        return;
    }

    // Install
    std::string installCmd = "rm -rf '" + QS_DIR + "' && mkdir -p '" + QS_DIR + "' && cp -r '" + src_dir.string() + "'/* '" + QS_DIR + "'";
    ret = std::system(installCmd.c_str());
    if (ret != 0) {
        std::cerr << "[!] Failed to install files. Aborting.\n";
        return;
    }
    console::spinner("Installing...", installCmd);

    // Update configuration
    cfgJson["shell"]["version"] = latest;
    std::ofstream cfgOut(CONFIG);
    cfgOut << cfgJson.dump(4);
    cfgOut.close();

    // Reload shell
    prompt::Stage("Reloading Shell..");
    std::system("killall qs &>/dev/null || true");
    std::system("nohup qs -c nucleus-shell &>/dev/null & disown");

    std::cout << "[✓] Updated nucleus-shell: " << current << " -> " << latest << "\n";
}


} // namespace update
