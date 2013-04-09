# Copyright © 2013 Canonical Ltd.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Authored by: Robert Ancell <robert.ancell@canonical.com>

# Compile protocol
protobuf_generate_cpp(
  GENERATED_PROTOBUF_SRCS GENERATED_PROTOBUF_HDRS
  dm_protocol.proto
)

# Compile system compositor
add_executable(
  unity-system-compositor
  main.cpp
  dm_connection.cpp
  dm_connection.h
  system_compositor.cpp
  system_compositor.h
  ${GENERATED_PROTOBUF_SRCS}
  ${GENERATED_PROTOBUF_HDRS}
)

# Link against libmirserver
target_link_libraries(unity-system-compositor
  mirserver
  pthread
  ${Boost_LIBRARIES}
  ${PROTOBUF_LIBRARIES}
)

# Install into bin directory
install(TARGETS unity-system-compositor
  RUNTIME DESTINATION bin
)
