set(TARGET_NAME glfw)

add_library(${TARGET_NAME} SHARED IMPORTED GLOBAL)
target_include_directories(${TARGET_NAME} INTERFACE include)

if(WIN32)
  set_target_properties(${TARGET_NAME} PROPERTIES IMPORTED_IMPLIB ${CMAKE_CURRENT_LIST_DIR}/lib/win/glfw3dll.lib)
endif()