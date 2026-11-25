
cmake_minimum_required(VERSION 3.20)

project(OGK-Agent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler options
add_library(ogk_agent_options INTERFACE)
target_compile_features(ogk_agent_options INTERFACE cxx_std_23)

# JSON dependency
find_package(nlohmann_json 3.11.3 CONFIG QUIET)

if(NOT nlohmann_json_FOUND)
    include(FetchContent)

    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(nlohmann_json)
endif()

# HTTP dependency
find_package(CURL REQUIRED)

# Main library
add_library(ogk_agent
    src/llm/llm_errors.cpp
    src/llm/ollama_client.cpp
)

target_include_directories(ogk_agent
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ogk_agent
    PUBLIC
        nlohmann_json::nlohmann_json
    PRIVATE
        CURL::libcurl
        ogk_agent_options
)

# Testing
include(CTest)

if(BUILD_TESTING)

    find_package(Python3 COMPONENTS Interpreter REQUIRED)

    # Unit Tests
    add_executable(ogk_unit_tests
        tests/unit_tests.cpp
    )

    target_link_libraries(ogk_unit_tests
        PRIVATE
            ogk_agent
    )

    add_test(
        NAME ogk_unit_tests
        COMMAND ogk_unit_tests
    )


    # HTTP Integration Tests
    add_executable(ogk_integration_tests
        tests/ollama_client_integration_tests.cpp
    )

    target_link_libraries(ogk_integration_tests
        PRIVATE
            ogk_agent
    )

    add_test(
        NAME ogk_integration_tests
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_integration_tests.py
            $<TARGET_FILE:ogk_integration_tests>
    )

endif()
