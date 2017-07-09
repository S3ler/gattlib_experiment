cmake_minimum_required(VERSION 3.6)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES Connection.cpp Connection.h)
set(MAIN_FILE main.cpp)

add_executable(BLEConnection ${SOURCE_FILES} ${MAIN_FILE})
target_link_libraries(BLEConnection  libBLEScanner pthread bluetooth)