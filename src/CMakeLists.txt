if (PLATFORM_API_IMPLEMENTATION STREQUAL "hybris")
  add_subdirectory(android/)
elseif(PLATFORM_API_IMPLEMENTATION STREQUAL "mirclient")
  add_subdirectory(mirclient/)
elseif(PLATFORM_API_IMPLEMENTATION STREQUAL "mirserver")
  add_subdirectory(mirserver/)
endif()

