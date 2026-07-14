cmake_minimum_required(VERSION 3.20)

project(Launcher)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_WIN32_EXECUTABLE TRUE)


set(SOURCES

    src/main.cpp

    src/App.cpp
    src/Window.cpp
    src/KeyboardHook.cpp
    src/ProgramIndexer.cpp
    src/SearchEngine.cpp
    src/Settings.cpp
    src/TrayIcon.cpp
    src/Utils.cpp

)


set(HEADERS

    src/App.h
    src/Window.h
    src/KeyboardHook.h
    src/ProgramIndexer.h
    src/SearchEngine.h
    src/Settings.h
    src/TrayIcon.h
    src/Utils.h

)


add_executable(
    Launcher
    ${SOURCES}
    ${HEADERS}

    resources/resource.rc
)


target_include_directories(
    Launcher
    PRIVATE
    src
)


# Fix Windows min/max macro conflict
target_compile_definitions(
    Launcher
    PRIVATE
    NOMINMAX
)


target_link_libraries(
    Launcher

    user32
    shell32
    advapi32
    gdi32
    comctl32
    dwmapi
)
