#include "network/http_client.h"

#include "core/logger.h"
#include "utils/file_utils.h"
#include "utils/platform_utils.h"
#include "utils/string_utils.h"

#include <stdexcept>

namespace webagent {

HttpClient::HttpClient(const Config& config) : config_(config) {}

std::string HttpClient::postJson(const std::string& url,
                                 const std::string& body,
                                 const std::string& action) const {
    if (config_.isMockMode()) {
        return postJsonFromMock(action, body);
    }
    return postJsonOverCurl(url, body);
}

std::string HttpClient::postMultipartResult(const std::string& url,
                                            const int resultCode,
                                            const std::string& resultJson,
                                            const std::vector<std::string>& files) const {
    if (config_.isMockMode()) {
        return postJsonFromMock("result", resultJson);
    }
    return postMultipartOverCurl(url, resultCode, resultJson, files);
}

std::string HttpClient::postJsonOverCurl(const std::string& url, const std::string& body) const {
    std::string command = "curl --silent --show-error --fail "
                          "--connect-timeout " +
                          std::to_string(config_.connectTimeoutSec) + " --max-time " +
                          std::to_string(config_.requestTimeoutSec) +
                          " -X POST -H 'Content-Type: application/json' --data " +
                          shellEscape(body) + " " + shellEscape(url);
    if (!config_.verifyTls) {
        command += " -k";
    } else if (!config_.caCertPath.empty()) {
        command += " --cacert " + shellEscape(config_.caCertPath);
    }
    if (!config_.apiToken.empty()) {
        command += " -H " + shellEscape("Authorization: Bearer " + config_.apiToken);
    }

    const auto result = runCommandCapture(command);
    if (result.exitCode != 0) {
        throw std::runtime_error("HTTP request failed: " + result.stdoutOutput);
    }
    return result.stdoutOutput;
}

std::string HttpClient::postMultipartOverCurl(const std::string& url,
                                              const int resultCode,
                                              const std::string& resultJson,
                                              const std::vector<std::string>& files) const {
    std::string command = "curl --silent --show-error --fail "
                          "--connect-timeout " +
                          std::to_string(config_.connectTimeoutSec) + " --max-time " +
                          std::to_string(config_.requestTimeoutSec) + " -X POST " + shellEscape(url);
    if (!config_.verifyTls) {
        command += " -k";
    } else if (!config_.caCertPath.empty()) {
        command += " --cacert " + shellEscape(config_.caCertPath);
    }
    if (!config_.apiToken.empty()) {
        command += " -H " + shellEscape("Authorization: Bearer " + config_.apiToken);
    }

    command += " -F " + shellEscape("result_code=" + std::to_string(resultCode));
    command += " -F " + shellEscape("result=" + resultJson);

    std::size_t fieldIndex = 1;
    for (const auto& file : files) {
        if (!fileExists(file)) {
            continue;
        }
        command += " -F " + shellEscape("file" + std::to_string(fieldIndex) + "=@" + file);
        ++fieldIndex;
    }

    const auto result = runCommandCapture(command);
    if (result.exitCode != 0) {
        throw std::runtime_error("Multipart request failed: " + result.stdoutOutput);
    }
    return result.stdoutOutput;
}

std::string HttpClient::postJsonFromMock(const std::string& action, const std::string&) const {
    const std::string responseFile = joinPath(config_.mockApiDirectory, action + "_response.json");
    Logger::instance().debug("Using mock response " + responseFile);
    return readTextFile(responseFile);
}

}  // namespace webagent
