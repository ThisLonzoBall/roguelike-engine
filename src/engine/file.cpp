#include "engine/file.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

std::optional<std::string> readTextFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return std::nullopt;

    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

bool directoryExists(const std::string& path) {
    std::error_code ec;
    return fs::is_directory(path, ec);
}

std::vector<std::string> listFiles(const std::string& directory, std::string_view extension) {
    std::vector<std::string> paths;

    std::error_code ec;
    for (const fs::directory_entry& entry : fs::directory_iterator(directory, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        if (entry.path().extension().string() != extension) continue;
        paths.push_back(entry.path().generic_string());
    }

    std::sort(paths.begin(), paths.end());
    return paths;
}
