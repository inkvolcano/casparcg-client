cmake_minimum_required(VERSION 3.16)
project(ndi VERSION 1.0 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)

qt_add_library(ndi
    STATIC
    NdiManager.cpp NdiManager.h
    NdiReceiver.cpp NdiReceiver.h
    Shared.h
)

target_include_directories(ndi PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/ndi/include
)

target_compile_definitions(ndi PUBLIC
    NDI_LIBRARY
)

target_link_libraries(ndi PUBLIC
    Qt::Core
    Qt::Gui
    Qt::Multimedia
)
