#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Whole file as a string, or nullopt if it can't be opened.
std::optional<std::string> readTextFile(const std::string& path);

bool directoryExists(const std::string& path);

// Full paths of the regular files directly inside `directory` whose name ends
// in `extension` (e.g. ".room"). Sorted by name so the order is the same on
// every machine. Empty if the directory doesn't exist.
std::vector<std::string> listFiles(const std::string& directory, std::string_view extension);
