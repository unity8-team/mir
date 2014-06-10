ADD_LIBRARY(
  mirscheduler STATIC

  asio_main_loop.cpp
  asio_server_action_queue.cpp
  asio_timer_service.cpp
  synchronous_server_action.cpp
)
