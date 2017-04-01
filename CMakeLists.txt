cmake_minimum_required (VERSION 3.5)
project (Boosmarking)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(src bin)

set(prj_config ${CMAKE_SOURCE_DIR}/config)
set(prj_bin ${CMAKE_CURRENT_BINARY_DIR}/bin)

configure_file(${prj_config}/privatekey.pem ${prj_bin}/privatekey.pem COPYONLY)
configure_file(${prj_config}/cacert.pem ${prj_bin}/cacert.pem COPYONLY)







