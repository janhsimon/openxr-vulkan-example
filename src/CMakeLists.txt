set(TARGET_NAME xrvk)

set(SHADER_SRC
  shaders/Basic.vert
  shaders/Basic.frag
)

set(SRC
  Main.cpp
  
  Buffer.cpp
  Buffer.h

  Headset.cpp
  Headset.h

  MirrorView.cpp
  MirrorView.h

  Renderer.cpp
  Renderer.h

  RenderTarget.cpp
  RenderTarget.h

  ${SHADER_SRC}
)

add_executable(${TARGET_NAME})
target_sources(${TARGET_NAME} PRIVATE ${SRC})
target_include_directories(${TARGET_NAME} PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${TARGET_NAME} PRIVATE glfw glm openxr ${Vulkan_LIBRARIES})

target_compile_definitions(${TARGET_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG>) # Add a clean DEBUG prepocessor define if applicable
set_target_properties(${TARGET_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${TARGET_NAME}>") # For MSVC debugging

# Copy shared libraries on Windows
if(WIN32)
  set (DLLS "${CMAKE_SOURCE_DIR}/external/glfw/lib/win/glfw3.dll")
  foreach (DLL ${DLLS})
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DLL} "$<TARGET_FILE_DIR:${TARGET_NAME}>")
  endforeach()
endif()

# Compile shaders into output directory
foreach(SHADER ${SHADER_SRC})
  set(SHADER_INPUT "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}")
  add_custom_command(
    OUTPUT "$<TARGET_FILE_DIR:${TARGET_NAME}>/${SHADER}.spv"
    COMMAND glslc --target-env=vulkan1.3 ${SHADER_INPUT} -std=450core -O -o "$<TARGET_FILE_DIR:${TARGET_NAME}>/${SHADER}.spv" $<$<NOT:$<CONFIG:DEBUG>>:-O>
    MAIN_DEPENDENCY ${SHADER_INPUT}
    COMMENT ${SHADER}
  )
endforeach()