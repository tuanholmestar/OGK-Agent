cmake_minimum_required(VERSION 3.20)

project(OGK-Agent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(ogk_agent_options INTERFACE)
target_compile_features(ogk_agent_options INTERFACE cxx_std_23)

include(CTest)
