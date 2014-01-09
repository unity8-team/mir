include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(application)
add_subdirectory(testbackend)

set(
  UBUNTU_APPLICATION_API_LINK_LIBRARIES
  
  ubuntu_application_location
  ubuntu_application_url_dispatcher
)

if(ENABLE_HYBRIS_IMPLEMENTATION)
  add_subdirectory(hybris/)
endif()

if(ENABLE_MIRSERVER_IMPLEMENTATION OR ENABLE_MIRCLIENT_IMPLEMENTATION)
  add_subdirectory(mircommon/)
endif()

if(ENABLE_MIRSERVER_IMPLEMENTATION)
  add_subdirectory(mirserver/)
endif()

if(ENABLE_MIRCLIENT_IMPLEMENTATION)
  add_subdirectory(mirclient/)
endif()
