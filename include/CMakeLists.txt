configure_file(config.h.in ${CMAKE_CURRENT_BINARY_DIR}/ubuntu/ui/config.h)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ubuntu/ui/config.h DESTINATION include/ubuntu/ui)

install(DIRECTORY ubuntu DESTINATION include)
