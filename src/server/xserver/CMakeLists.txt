set(
  MIR_XSERVER_SRCS

  global_socket_listening_server_spawner.cpp
  null_server_spawner.cpp
)

ADD_LIBRARY(
  mirxserverintegration STATIC

  ${MIR_XSERVER_SRCS}
)
