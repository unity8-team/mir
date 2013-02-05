add_library(
	ubuntu_mock_platform SHARED

	mock.cpp
	# Pull in the default C API implementation on top of the C++ API
	../default/default_ubuntu_application_ui.cpp
)

install(
  TARGETS ubuntu_mock_platform
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib/static)
