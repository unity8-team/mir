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

add_executable(
  unity-system-compositor
  unity-system-compositor.cpp
)

include_directories(/home/bob/bzr/mir/include)

target_link_libraries(unity-system-compositor
  mirserver
)

install(TARGETS unity-system-compositor
  RUNTIME DESTINATION bin
)
