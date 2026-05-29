cmake_minimum_required(VERSION 3.17)

# ── Single source of truth: read version from VERSION file ────────────────
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" _ver_raw)
string(STRIP "${_ver_raw}" _ver)

project(HermesStrategyShell VERSION ${_ver} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(VCPKG_APPLOCAL_DEPS OFF CACHE BOOL "" FORCE)

# Options
option(ENABLE_MSMQ "Enable MSMQ Support in HermesTrader" ON)
option(ENABLE_LOCAL "Enable LocalExecutor in HermesTrader" ON)
option(BUILD_MONITOR "Build the monitor dashboard" ON)

if(UNIX AND NOT APPLE)
    set(CMAKE_INSTALL_RPATH "$ORIGIN")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

# Configure Version Resources
configure_file(include/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version.h @ONLY)
if(WIN32)
    configure_file(version.rc.in ${CMAKE_CURRENT_BINARY_DIR}/version.rc @ONLY)
endif()

# Source files
file(GLOB SOURCES "src/*.cpp")
if(WIN32)
    list(APPEND SOURCES ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()

# Add Executable
add_executable(HermesStrategyShell ${SOURCES})

# Include directories
target_include_directories(HermesStrategyShell PRIVATE 
    include 
    ${CMAKE_CURRENT_BINARY_DIR}
)

# --- Dependencies Setup (Extern Git Repos) ---
if(NOT TARGET HermesCommon)
    message(STATUS "Adding HermesCommon from extern/hermes_common...")
    add_subdirectory(extern/hermes_common EXCLUDE_FROM_ALL)
endif()

if(NOT TARGET HermesPortal)
    message(STATUS "Adding HermesPortal from extern/... (EXCLUDE_FROM_ALL)")
    add_subdirectory(extern/HermesPortal EXCLUDE_FROM_ALL)
endif()

if(NOT TARGET HermesTrader)
    message(STATUS "Adding HermesTrader from extern/... (EXCLUDE_FROM_ALL)")
    set(ENABLE_LOCAL ${ENABLE_LOCAL} CACHE BOOL "" FORCE)
    add_subdirectory(extern/HermesTrader EXCLUDE_FROM_ALL)
endif()

if(TARGET HermesTrader)
    target_include_directories(HermesTrader PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/HermesPortal/include
        ${CMAKE_CURRENT_BINARY_DIR}/extern/HermesPortal/include
        ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/include
    )
    if(VCPKG_TARGET_TRIPLET)
        target_include_directories(HermesTrader PRIVATE 
            ${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include
        )
    endif()
endif()

find_package(Threads REQUIRED)

target_link_libraries(HermesStrategyShell PRIVATE HermesPortal HermesTrader HermesCommon nlohmann_json::nlohmann_json Threads::Threads)
target_include_directories(HermesStrategyShell PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/extern/HermesPortal/include)

if(UNIX)
    target_link_libraries(HermesStrategyShell PRIVATE stdc++fs pthread)
endif()

target_compile_definitions(HermesStrategyShell PRIVATE HAS_HERMES_TRADER)

find_library(LZO_LIBRARY NAMES lzo2 lzo)
find_package(CURL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# --- Installation Rules (Dist Generation) ---
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    if(WIN32)
        set(PLATFORM "win64")
    else()
        set(PLATFORM "linux")
    endif()
    set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/dist/hermes_strategy_shell_${PLATFORM}" CACHE PATH "Default install prefix" FORCE)
endif()

install(TARGETS HermesStrategyShell RUNTIME DESTINATION .)

install(FILES 
    $<TARGET_FILE:HermesPortal>
    $<TARGET_FILE:HermesTrader>
    DESTINATION .
)

if(WIN32 AND TARGET HermesTraderMSMQ)
    install(FILES $<TARGET_FILE:HermesTraderMSMQ> DESTINATION .)
endif()

if(WIN32)
    set(VCPKG_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/bin")
    file(GLOB VCPKG_DLLS 
        "${VCPKG_BIN_DIR}/libcurl.dll"
        "${VCPKG_BIN_DIR}/lzo2.dll"
        "${VCPKG_BIN_DIR}/zlib1.dll"
        "${VCPKG_BIN_DIR}/uv.dll"
        "${VCPKG_BIN_DIR}/libzmq*.dll"
        "${VCPKG_BIN_DIR}/libcrypto-*.dll"
        "${VCPKG_BIN_DIR}/libssl-*.dll"
    )
    if(VCPKG_DLLS)
        install(FILES ${VCPKG_DLLS} DESTINATION .)
    endif()
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.env")
    install(FILES ".env" DESTINATION .)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/env.example")
    install(FILES "env.example" DESTINATION . RENAME ".env")
endif()

install(FILES "env.example" DESTINATION .)

# Install local data/masters if they exist (for manual overrides)
if(EXISTS "${CMAKE_SOURCE_DIR}/data/masters")
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/data/masters/" DESTINATION data/masters
            FILES_MATCHING PATTERN "*.csv")
endif()
