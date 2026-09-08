cmake_minimum_required(VERSION 3.16)
project(Solution VERSION 1.0 LANGUAGES C CXX)

SET (CONFIG_VERSION_MAJOR 2)
SET (CONFIG_VERSION_MINOR 3)
SET (CONFIG_VERSION_BUG 1)
# SET (CONFIG_VERSION_TAG "Dev")

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Add custom cmake modules path
LIST (APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/CMakeModules)

include(ExternalProject)
include(CasparCG_Util)

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)

FIND_PACKAGE (Git)
SET (GIT_TIMESTAMP "")
SET (GIT_VERSION "N/A")
IF (DEFINED ENV{GIT_TIMESTAMP} AND NOT $ENV{GIT_TIMESTAMP} STREQUAL "")
    SET (GIT_TIMESTAMP "$ENV{GIT_TIMESTAMP}")
    SET (GIT_VERSION "$ENV{GIT_VERSION}")
ELSEIF (GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/../.git")
    EXEC_PROGRAM ("${GIT_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/../" ARGS log -n 1 --format=format:"%ai" OUTPUT_VARIABLE GIT_TIMESTAMP)
    EXEC_PROGRAM ("${GIT_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/../" ARGS rev-parse --verify --short=10 HEAD OUTPUT_VARIABLE GIT_VERSION)
ENDIF ()

set(CASPARCG_DOWNLOAD_MIRROR https://github.com/CasparCG/dependencies/releases/download/ CACHE STRING "Source/mirror to use for external dependencies")
set(CASPARCG_DOWNLOAD_CACHE ${CMAKE_CURRENT_BINARY_DIR}/external CACHE STRING "Download cache directory for cmake ExternalProjects")

cmake_host_system_information(RESULT CONFIG_CPU_COUNT QUERY NUMBER_OF_PHYSICAL_CORES)

if(UNIX AND NOT APPLE)
    include(Bootstrap_Linux)
elseif (UNIX AND APPLE)
    include(Bootstrap_Macos)
else()
    include(Bootstrap_Windows)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Multimedia Network Sql WebSockets Widgets Core5Compat)

# Qt WebEngine renders HTML templates in the Preview panel. It is deliberately
# optional: it is a large component that minimal Qt installs and the CI images do
# not carry, and everything else in the client works without it. When it is
# missing the Preview panel says so in place of the template and carries on.
find_package(Qt6 QUIET COMPONENTS WebEngineWidgets)
if (Qt6WebEngineWidgets_FOUND)
    message(STATUS "Qt WebEngine found - template preview enabled")
else()
    message(STATUS "Qt WebEngine not found - template preview disabled (everything else builds)")
endif()
if (MSVC OR APPLE)
    qt_standard_project_setup()
endif()


add_subdirectory(Common)
add_subdirectory(Caspar)
add_subdirectory(Web)
add_subdirectory(Gpi)
add_subdirectory(Osc)
add_subdirectory(Repository)
add_subdirectory(Core)
add_subdirectory(Ndi)
add_subdirectory(Widgets)
add_subdirectory(Tests)
add_subdirectory(Shell)
add_subdirectory(Push)
