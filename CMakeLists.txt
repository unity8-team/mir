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

# Use C++11 and warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")

project(UnitySystemCompisitor)

cmake_minimum_required(VERSION 2.8)

# Set location of outputs
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)

# Check for Mir server library
find_package(PkgConfig)
pkg_check_modules(MIRSERVER REQUIRED mirserver)
include_directories(${MIRSERVER_INCLUDE_DIRS})

# Check for boost
find_package(Boost 1.48.0 COMPONENTS chrono date_time filesystem system thread program_options regex REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

# Check for Google Protocol Buffers
find_package(Protobuf REQUIRED)

add_subdirectory(src/)
