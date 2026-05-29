cmake_minimum_required(VERSION 3.10)
project(HermesMonitorStub VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# Source files
set(SOURCES
    main.cpp
)

# Main Executable
add_executable(HermesMonitorStub ${SOURCES})

# Installation
if(WIN32)
    set(PLATFORM "win64")
else()
    set(PLATFORM "linux")
endif()

# We install to the strategy's dist folder
set(DIST_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/../../dist/hermes_strategy_shell_${PLATFORM})

install(TARGETS HermesMonitorStub DESTINATION ${DIST_OUTPUT_DIR})
