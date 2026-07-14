cmake_minimum_required(VERSION 3.20)

project(Launcher)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create Windows GUI application
set(CMAKE_WIN32_EXECUTABLE TRUE)


# Source files
set(SOURCES

    src/main.cpp
    src/App.cpp

    # Add more cpp files here later
    # src/Window.cpp
    # src/KeyboardHook.cpp
    # src/SearchEngine.cpp

)


# Resource file
set(RESOURCES

    resources/resource.rc

)


add_executable(
    Launcher
    ${SOURCES}
    ${RESOURCES}
)


# Header location
target_include_directories(
    Launcher
    PRIVATE
    src
)


# Windows libraries

target_link_libraries(
    Launcher

    user32
    shell32
    advapi32
    gdi32
)
