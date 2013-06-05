include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
)

if(ENABLE_HYBRIS_IMPLEMENTATION)
  add_subdirectory(hybris/)
endif()

if(ENABLE_MIRSERVER_IMPLEMENTATION OR ENABLE_MIRCLIENT_IMPLEMENTATION)
add_subdirectory(mircommon)
endif()

if(ENABLE_MIRSERVER_IMPLEMENTATION)
add_subdirectory(mirserver/)
endif()

if(ENABLE_MIRCLIENT_IMPLEMENTATION)
add_subdirectory(mirclient/)
endif()
