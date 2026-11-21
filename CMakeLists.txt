cmake_minimum_required(VERSION 3.20)

project(OGK-Agent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

include(CTest)
