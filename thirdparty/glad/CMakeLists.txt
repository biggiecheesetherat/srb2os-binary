add_library(glad STATIC src/gl.c include/glad/gl.h include/KHR/khrplatform.h)
add_library(glad::glad ALIAS glad)
target_include_directories(glad PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
