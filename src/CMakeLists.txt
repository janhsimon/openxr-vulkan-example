set(TARGET_NAME openxr-vulkan-example)

set(SHADER_SRC
  shaders/Diffuse.vert
  shaders/Diffuse.frag

  shaders/Grid.vert
  shaders/Grid.frag
)

set(SRC
  Main.cpp
  
  Buffer.cpp
  Buffer.h

  Context.cpp
  Context.h

  Headset.cpp
  Headset.h

  MeshData.cpp
  MeshData.h

  MirrorView.cpp
  MirrorView.h

  Model.h

  Pipeline.cpp
  Pipeline.h

  Renderer.cpp
  Renderer.h

  RenderProcess.cpp
  RenderProcess.h

  RenderTarget.cpp
  RenderTarget.h

  Util.cpp
  Util.h

  ${SHADER_SRC}
)

add_executable(${TARGET_NAME})
target_sources(${TARGET_NAME} PRIVATE ${SRC})
target_include_directories(${TARGET_NAME} PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${TARGET_NAME} PRIVATE boxer glfw glm openxr tinyobjloader ${Vulkan_LIBRARIES})

target_compile_definitions(${TARGET_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG>) # Add a clean DEBUG prepocessor define if applicable
set_target_properties(${TARGET_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${TARGET_NAME}>") # For MSVC debugging

# Copy shared library binaries on Windows
if(WIN32)
  set (DLLS "${CMAKE_SOURCE_DIR}/external/glfw/lib/win/glfw3.dll")
  foreach (DLL ${DLLS})
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${DLL} "$<TARGET_FILE_DIR:${TARGET_NAME}>")
  endforeach()
endif()

# Copy models folder
add_custom_command(TARGET ${TARGET_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} ARGS -E copy_directory "${CMAKE_SOURCE_DIR}/models" "$<TARGET_FILE_DIR:${TARGET_NAME}>/models")

# Create output folder for compiled shaders
# Otherwise shader compilation fails
add_custom_command(TARGET ${TARGET_NAME} PRE_BUILD COMMAND ${CMAKE_COMMAND} ARGS -E make_directory "$<TARGET_FILE_DIR:${TARGET_NAME}>/shaders")

# Compile shaders into output folder
foreach(SHADER ${SHADER_SRC})
  set(SHADER_INPUT "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}")
  add_custom_command(TARGET ${TARGET_NAME} DEPENDS ${SHADER_INPUT} COMMAND glslc ARGS --target-env=vulkan1.3 ${SHADER_INPUT} -std=450core -O -o "$<TARGET_FILE_DIR:${TARGET_NAME}>/${SHADER}.spv" $<$<NOT:$<CONFIG:DEBUG>>:-O> COMMENT ${SHADER})
endforeach()