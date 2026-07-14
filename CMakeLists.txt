cmake_minimum_required(VERSION 3.20)

project(Launcher)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows GUI application (no console window)
set(CMAKE_WIN32_EXECUTABLE TRUE)


# All C++ source files
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


# Header files (optional but helps IDEs)
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
)


# Include src folder so #include "App.h" works
target_include_directories(
    Launcher
    PRIVATE
    src
)


# Windows API libraries
target_link_libraries(
    Launcher

    user32
    shell32
    advapi32
    gdi32
    comctl32
)


# Add application resources
if(EXISTS "${CMAKE_SOURCE_DIR}/resources/resource.rc")

    target_sources(
        Launcher
        PRIVATE
        resources/resource.rc
    )

endif()
