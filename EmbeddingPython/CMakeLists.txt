#
#  Central Nordic Uart Service - CNUS
#
#  Copyright (C) 2017  S3ler
#
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#

cmake_minimum_required(VERSION 2.6)

find_package(PkgConfig REQUIRED)

pkg_search_module(GATTLIB REQUIRED gattlib)
pkg_search_module(GLIB REQUIRED glib-2.0)

include_directories(${GLIB_INCLUDE_DIRS})

#
# Add Bluetooth support: Workaround as the current 'gatttool' example
# is still based on 'bluez' code
#
pkg_search_module(BLUEZ REQUIRED bluez)

# Extract Bluez version
string(REPLACE "." ";" BLUEZ_VERSIONS "${BLUEZ_VERSION}")
list(GET BLUEZ_VERSIONS 0 BLUEZ_VERSION_MAJOR)
list(GET BLUEZ_VERSIONS 1 BLUEZ_VERSION_MINOR)
add_definitions(-DBLUEZ_VERSION_MAJOR=${BLUEZ_VERSION_MAJOR} -DBLUEZ_VERSION_MINOR=${BLUEZ_VERSION_MINOR})

if(BLUEZ_VERSION_MAJOR STREQUAL "4")
  set(bluez4_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../bluez/bluez4)
  include_directories(${bluez4_DIR}/attrib ${bluez4_DIR}/btio ${bluez4_DIR}/src ${bluez4_DIR}/lib)
else()
  set(bluez5_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../bluez/bluez5)
  include_directories(${bluez5_DIR} ${bluez5_DIR}/attrib ${bluez5_DIR}/btio ${bluez5_DIR}/lib)
endif()

include_directories(/usr/include/python2.7)

add_executable(EmbeddingPython_CallFunction call_function.c)
target_link_libraries(EmbeddingPython_CallFunction ${GATTLIB_LIBRARIES} ${GLIB_LIBRARIES} python2.7 )

add_executable(EmbeddingPython_CallClass call_class.c)
target_link_libraries(EmbeddingPython_CallClass ${GATTLIB_LIBRARIES} ${GLIB_LIBRARIES} python2.7 )

add_executable(EmbeddingPython_CallByteArrayClass_Test call_bytearrayclass.c)
target_link_libraries(EmbeddingPython_CallByteArrayClass_Test ${GATTLIB_LIBRARIES} ${GLIB_LIBRARIES} python2.7 )

add_executable(EmbeddingPython_CallClass_Test call_class_test.c)
target_link_libraries(EmbeddingPython_CallClass_Test ${GATTLIB_LIBRARIES} ${GLIB_LIBRARIES} python2.7 )

add_executable(EmbeddingPython_CallThread call_thread.c)
target_link_libraries(EmbeddingPython_CallThread ${GATTLIB_LIBRARIES} ${GLIB_LIBRARIES} python2.7 pthread)

add_executable(EmbeddingPython_CallThread_2 call_thread_2.c)
target_link_libraries(EmbeddingPython_CallThread_2 ${GATTLIB_LIBRARIES} ${GLIB_LIBRARIES} python2.7 pthread)