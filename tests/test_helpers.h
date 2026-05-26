#pragma once

#include <cstdlib>
#include <filesystem>
#include <string>

namespace webagent::test {

inline std::string makeTempDir(const std::string& name) {
    const auto base = std::filesystem::temp_directory_path();
    const auto unique = std::filesystem::path(name + "_" + std::to_string(std::rand()));
    const auto dir = base / unique;
    std::filesystem::create_directories(dir);
    return dir.string();
}

}  // namespace webagent::test
