#pragma once

#include <string>
#include <vector>

namespace webagent {

std::string trim(const std::string& value);
std::string toLower(std::string value);
std::string shellEscape(const std::string& value);
std::string join(const std::vector<std::string>& items, const std::string& delimiter);

}  // namespace webagent
