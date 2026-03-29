cmake_minimum_required(VERSION 3.16)
project(AIMudCyberworld LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Core Gui Widgets Network REQUIRED)

set(CYBERWORLD_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_executable(cyberworld
    src/main.cpp
    include/MainWindow.h
    src/MainWindow.cpp
    include/Player.h
    src/Player.cpp
    include/WorldState.h
    src/WorldState.cpp
    include/SaveManager.h
    src/SaveManager.cpp
    include/LLMClient.h
    src/LLMClient.cpp
)

target_include_directories(cyberworld PRIVATE ${CYBERWORLD_INCLUDES})

target_link_libraries(cyberworld PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

if(MSVC)
    target_compile_options(cyberworld PRIVATE /utf-8)
endif()
