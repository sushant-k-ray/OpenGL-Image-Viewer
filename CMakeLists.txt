cmake_minimum_required(VERSION 3.16)

project(glimviewer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

add_executable(glimviewer main.cpp
    glad/src/glad.c
    glimview.hpp
    glimview.cpp)

target_include_directories(glimviewer PRIVATE
    ${OPENGL_INCLUDE_DIRS}
    ${GLFW3_INCLUDE_DIRS}
    glad/include
    stb
)

target_link_libraries(glimviewer PRIVATE
    ${OPENGL_LIBRARIES}
    ${GLFW3_LIBRARIES}
    glfw
)

include(GNUInstallDirs)
install(TARGETS glimviewer
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
