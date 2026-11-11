include_guard(GLOBAL)
set(VOLUMETRICS_INTEGRATION_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(volumetrics_attach_renderer target)
    if(NOT TARGET VolumetricFog)
        message(FATAL_ERROR "Add the volumetrics subdirectory with VOLUMETRICS_BUILD_OPENGL=ON first")
    endif()
    target_sources(${target} PRIVATE
        "${VOLUMETRICS_INTEGRATION_DIR}/RendererAdapter.cpp"
        "${VOLUMETRICS_INTEGRATION_DIR}/VolumetricUI.cpp"
        "${VOLUMETRICS_INTEGRATION_DIR}/VolumetricValidation.cpp")
    target_include_directories(${target} PUBLIC "${VOLUMETRICS_INTEGRATION_DIR}")
    target_link_libraries(${target} PUBLIC VolumetricFog)
    target_compile_definitions(${target} PRIVATE
        VOLUMETRICS_SHADER_DIRECTORY="assets/volumetrics/shaders"
        VOLUMETRICS_SOURCE_SHADERS="${VOLUMETRICS_INTEGRATION_DIR}/../assets/shaders")
endfunction()

function(volumetrics_attach_demo target)
    target_sources(${target} PRIVATE "${VOLUMETRICS_INTEGRATION_DIR}/VolumetricDemoScene.cpp")
    # Run from the executable directory; preserve original renderer assets.
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${VOLUMETRICS_INTEGRATION_DIR}/../assets" "$<TARGET_FILE_DIR:${target}>/assets/volumetrics"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:VolumetricCPU> $<TARGET_FILE_DIR:${target}>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:VolumetricFog> $<TARGET_FILE_DIR:${target}>)
    if(BUILD_TESTING)
        add_test(NAME volumetric_renderer COMMAND $<TARGET_FILE:${target}> --volumetric-validate)
        set_tests_properties(volumetric_renderer PROPERTIES WORKING_DIRECTORY "$<TARGET_FILE_DIR:${target}>")
    endif()
endfunction()
