cmake_minimum_required(VERSION 3.16)

project(AlertVibe VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Network Sql Widgets)

qt_standard_project_setup()

qt_add_executable(AlertVibe
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/udpmessagehandler.cpp
    src/udpmessagehandler.h
)

target_link_libraries(AlertVibe PRIVATE Qt6::Network Qt6::Sql Qt6::Widgets)

set_target_properties(AlertVibe PROPERTIES
    WIN32_EXECUTABLE TRUE
)
