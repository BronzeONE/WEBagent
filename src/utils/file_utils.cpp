#include "utils/file_utils.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace webagent {

std::string readTextFile(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Unable to open file for reading: " + path);
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void writeTextFile(const std::string& path, const std::string& content) {
    ensureParentDirectory(path);
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("Unable to open file for writing: " + path);
    }
    output << content;
}

void ensureDirectory(const std::string& path) {
    if (!path.empty()) {
        fs::create_directories(fs::path(path));
    }
}

void ensureParentDirectory(const std::string& path) {
    const auto parent = fs::path(path).parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent);
    }
}

std::string joinPath(const std::string& base, const std::string& leaf) {
    return (fs::path(base) / fs::path(leaf)).string();
}

std::vector<std::string> listFilesByMask(const std::string& directory, const std::string& mask) {
    std::vector<std::string> files;
    if (!fs::exists(directory)) {
        return files;
    }

    std::string suffix;
    if (mask.size() >= 2 && mask[0] == '*' && mask[1] == '.') {
        suffix = mask.substr(1);
    }

    for (const auto& entry : fs::directory_iterator(directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        if (suffix.empty() || entry.path().extension() == suffix) {
            files.push_back(entry.path().string());
        }
    }

    return files;
}

bool fileExists(const std::string& path) {
    return fs::exists(fs::path(path));
}

}  // namespace webagent
