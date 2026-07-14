cmake_minimum_required(VERSION 3.20)

project(Launcher)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows GUI application
set(CMAKE_WIN32_EXECUTABLE TRUE)


add_executable(
    Launcher
    src/main.cpp
)


target_link_libraries(
    Launcher
    user32
    shell32
    advapi32
    gdi32
)
