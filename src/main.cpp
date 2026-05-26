#include "core/agent.h"
#include "core/config.h"
#include "core/logger.h"

#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    const std::string configPath = argc > 1 ? argv[1] : "config.json";

    try {
        const auto config = webagent::Config::loadFromFile(configPath);
        webagent::Logger::instance().initialize(config.logFile);
        webagent::Logger::instance().info("Starting WEB-AGENT");

        webagent::Agent agent(config);
        const int exitCode = agent.run();

        webagent::Logger::instance().info("WEB-AGENT stopped");
        return exitCode;
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << '\n';
        return 1;
    }
}
