cmake_minimum_required (VERSION 3.5)
project (BoostExercise)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(server server.cpp)
add_executable(client client.cpp)

FIND_PACKAGE( Boost 1.58 COMPONENTS thread system REQUIRED )
INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} )

if(UNIX AND NOT APPLE)
  set(LINUX TRUE)
else()
  FIND_PACKAGE( OPENSSL COMPONENTS ssl crypto REQUIRED )
  INCLUDE_DIRECTORIES( ${OPENSSL_INCLUDE_DIR} )
  TARGET_LINK_LIBRARIES(server ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})
  TARGET_LINK_LIBRARIES(client ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})
endif()

# if(NOT LINUX) should work, too, if you need that
if(LINUX) 
    message(STATUS ">>> Linux")
    # linux stuff here
    TARGET_LINK_LIBRARIES(server ${Boost_LIBRARIES} ssl crypto)
    TARGET_LINK_LIBRARIES(client ${Boost_LIBRARIES} ssl crypto)
else()
    message(STATUS ">>> Not Linux")
    # stuff that should happen not on Linux 
endif()





