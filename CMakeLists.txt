cmake_minimum_required(VERSION 3.26)
project(PRSVolumetricFog LANGUAGES C CXX)

option(VOLUMETRICS_BUILD_RENDERER "Build the OpenGL renderer and integration demo" ON)
include(CTest)
if(VOLUMETRICS_BUILD_RENDERER)
    add_subdirectory(opengl-forward-renderer)
else()
    # This path fetches only GLM: no OpenGL, GLFW, ImGui or renderer dependencies.
    add_subdirectory(volumetrics)
endif()
