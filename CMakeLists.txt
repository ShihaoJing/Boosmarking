cmake_minimum_required (VERSION 3.5)
project (BoostExercise)

set(CMAKE_CXX_FLAGS "-g -Wall")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

FIND_PACKAGE( Boost 1.58 COMPONENTS thread system REQUIRED )
INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} )

add_executable(server server.cpp)
add_executable(client client.cpp)
add_executable(benchmark benchmark.cpp)

TARGET_LINK_LIBRARIES(server ${Boost_LIBRARIES} ssl crypto)
TARGET_LINK_LIBRARIES(client ${Boost_LIBRARIES} pthread ssl crypto)
TARGET_LINK_LIBRARIES(benchmark ${Boost_LIBRARIES} ssl crypto)