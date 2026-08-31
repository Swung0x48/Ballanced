cmake_minimum_required(VERSION 3.21)

project(Ballanced
        VERSION 0.1.0
        DESCRIPTION "Ballance Decompilation"
        HOMEPAGE_URL https://github.com/doyaGu/Ballanced
        LANGUAGES C CXX
)

list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(GNUInstallDirs)
include(BallanceCache)
include(BallanceProject)
include(BallanceOptions)
include(BallanceCompiler)
include(BallanceVersion)
include(BallanceBuildManifest)
include(CTest)

if (BALLANCE_HEADLESS_ONLY)
    message(STATUS "[Ballance] Configuring headless-only targets")
    add_subdirectory(Source/Headless)
else ()
    add_subdirectory(Source)
    include(BallanceStage)
endif ()
