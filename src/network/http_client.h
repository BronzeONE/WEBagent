#pragma once

#include "core/config.h"

#include <string>
#include <vector>

namespace webagent {

class HttpClient {
public:
    explicit HttpClient(const Config& config);

    std::string postJson(const std::string& url, const std::string& body, const std::string& action) const;
    std::string postMultipartResult(const std::string& url,
                                    int resultCode,
                                    const std::string& resultJson,
                                    const std::vector<std::string>& files) const;

private:
    std::string postJsonOverCurl(const std::string& url, const std::string& body) const;
    std::string postMultipartOverCurl(const std::string& url,
                                      int resultCode,
                                      const std::string& resultJson,
                                      const std::vector<std::string>& files) const;
    std::string postJsonFromMock(const std::string& action, const std::string& body) const;

    const Config& config_;
};

}  // namespace webagent
