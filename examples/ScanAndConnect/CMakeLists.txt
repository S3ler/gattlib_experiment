cmake_minimum_required(VERSION 3.6)

set(CMAKE_CXX_STANDARD 11)

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

set(SOURCE_FILES Connection.cpp Connection.h)
set(MAIN_FILE main.cpp ConnectionCallbacks.cpp ConnectionCallbacks.h Connector.cpp Connector.h)

add_executable(BLEScanAndConnect ${SOURCE_FILES} ${MAIN_FILE})
target_link_libraries(BLEScanAndConnect ${GATTLIB_LIBRARIES} ${GLIB_LIBRARIES} libBLEScanner pthread bluetooth )