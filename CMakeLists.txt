cmake_minimum_required(VERSION 3.20)

project(Launcher
    VERSION 1.0
    LANGUAGES CXX
)

# Use C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows GUI application (no console window)
set(CMAKE_WIN32_EXECUTABLE TRUE)


# Source files
set(SOURCES

    src/main.cpp

    src/App.cpp
    src/Window.cpp
    src/KeyboardHook.cpp
    src/SearchEngine.cpp
    src/ProgramIndexer.cpp
    src/TrayIcon.cpp
    src/Settings.cpp
    src/Utils.cpp

)


# Header files
set(HEADERS

    include/App.h
    include/Window.h
    include/KeyboardHook.h
    include/SearchEngine.h
    include/ProgramIndexer.h
    include/TrayIcon.h
    include/Settings.h
    include/Utils.h

)


# Create executable
add_executable(
    Launcher
    ${SOURCES}
    ${HEADERS}
)


# Include header directory
target_include_directories(
    Launcher
    PRIVATE
    include
)


# Windows libraries required for Win32 API

target_link_libraries(
    Launcher

    user32
    shell32
    advapi32
    gdi32
    comctl32

)


# Resource file
if(EXISTS "${CMAKE_SOURCE_DIR}/resources/resource.rc")

    target_sources(
        Launcher
        PRIVATE
        resources/resource.rc
    )

endif()
