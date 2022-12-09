set(TARGET_NAME tinyobjloader)

add_library(${TARGET_NAME} STATIC IMPORTED GLOBAL)
target_include_directories(${TARGET_NAME} INTERFACE "include")

if(WIN32)
  set_target_properties(${TARGET_NAME} PROPERTIES IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_LIST_DIR}/lib/win/tinyobjloaderd.lib" IMPORTED_LOCATION "${CMAKE_CURRENT_LIST_DIR}/lib/win/tinyobjloader.lib")
endif()