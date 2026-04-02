cmake_minimum_required(VERSION 3.20)
project(AutoClicker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(AutoClicker WIN32
    src/main.cpp
)

target_compile_definitions(AutoClicker PRIVATE UNICODE _UNICODE)
target_link_libraries(AutoClicker PRIVATE user32 gdi32 comctl32 comdlg32)

