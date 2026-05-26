#pragma once

#include <string>
#include <vector>

namespace webagent {

std::string readTextFile(const std::string& path);
void writeTextFile(const std::string& path, const std::string& content);
void ensureDirectory(const std::string& path);
void ensureParentDirectory(const std::string& path);
std::string joinPath(const std::string& base, const std::string& leaf);
std::vector<std::string> listFilesByMask(const std::string& directory, const std::string& mask);
bool fileExists(const std::string& path);

}  // namespace webagent
