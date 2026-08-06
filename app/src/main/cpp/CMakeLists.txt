cmake_minimum_required(VERSION 3.22.1)
project("uvpainter" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(uvpainter SHARED
    JniBridge.cpp
    engine/Engine.cpp
    gl/GLObjects.cpp
    gl/RenderContext.cpp
    io/Json.cpp
    io/ModelLoader.cpp
    paint/Document.cpp
    paint/PaintEngine.cpp
    paint/UndoStack.cpp
    render/ViewportRenderer.cpp
    scene/Camera.cpp
)

target_include_directories(uvpainter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(uvpainter PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Release>:-ffast-math>
)

target_link_libraries(uvpainter
    android
    EGL
    GLESv3
    log
    # Las capas del proyecto se guardan comprimidas: una capa de dibujo es casi
    # toda transparente y baja de 16 MB a unos cientos de KB.
    z
)
