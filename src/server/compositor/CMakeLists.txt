set(
  MIR_COMPOSITOR_SRCS

  default_display_buffer_compositor.cpp
  default_display_buffer_compositor_factory.cpp
  buffer_queue_factory.cpp
  gl_renderer.cpp
  gl_renderer_factory.cpp
  gl_program_family.cpp
  multi_threaded_compositor.cpp
  occlusion.cpp
  default_configuration.cpp
  screencast_display_buffer.cpp
  compositing_screencast.cpp
  timeout_frame_dropping_policy_factory.cpp
  buffer_queue.cpp
  buffer_handle.cpp
  recently_used_cache.cpp
)

ADD_LIBRARY(
  mircompositor OBJECT

  ${MIR_COMPOSITOR_SRCS}
)

