# By default, build for LLVM

if (MIR_PLATFORM STREQUAL "gbm")
  include_directories(
    ${DRM_INCLUDE_DIRS}
    ${GBM_INCLUDE_DIRS}
    ${EGL_INCLUDE_DIRS}
    ${GLESv2_INCLUDE_DIRS}
  )
endif()

add_subdirectory(display/)
add_subdirectory(graphics_buffer/)

if (MIR_PLATFORM STREQUAL "gbm")
set(
  PLATFORM_SOURCES
  
  ${PLATFORM_SOURCES}
  gbm_platform.cpp
)
endif()

if (MIR_PLATFORM STREQUAL "android")
set(
  PLATFORM_SOURCES
  
  ${PLATFORM_SOURCES}
  android_platform.cpp
)
endif()

if (MIR_PLATFORM STREQUAL "gbm")
  add_subdirectory(egl/)
endif()

add_library(
  mirplatform STATIC

  ${PLATFORM_SOURCES}
)

target_link_libraries(
  mirplatform

  mirdisplay
  mirlogging

  ${MIR_COMMON_PLATFORM_LIBRARIES}
  ${GLESv2_LIBRARIES}
)
