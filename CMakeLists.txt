cmake_minimum_required(VERSION 3.16)

project(web_agent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(WEB_AGENT_CORE_SOURCES
    src/core/agent.cpp
    src/core/config.cpp
    src/core/logger.cpp
    src/network/api_types.cpp
    src/network/http_client.cpp
    src/network/registration.cpp
    src/executor/task_executor.cpp
    src/executor/task_factory.cpp
    src/executor/file_manager.cpp
    src/utils/file_utils.cpp
    src/utils/json.cpp
    src/utils/platform_utils.cpp
    src/utils/string_utils.cpp
)

add_library(web-agent-lib ${WEB_AGENT_CORE_SOURCES})
target_include_directories(web-agent-lib PUBLIC src)

add_executable(web-agent src/main.cpp)
target_link_libraries(web-agent PRIVATE web-agent-lib)

if(APPLE)
    target_compile_options(web-agent-lib PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    include(FetchContent)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(web-agent-tests
        tests/test_api_types.cpp
        tests/test_config.cpp
        tests/test_task_executor.cpp
        tests/test_agent_integration.cpp
        tests/test_http_client_negative.cpp
    )
    target_link_libraries(web-agent-tests PRIVATE web-agent-lib GTest::gtest_main)
    target_include_directories(web-agent-tests PRIVATE tests src)

    include(GoogleTest)
    gtest_discover_tests(web-agent-tests)
endif()
