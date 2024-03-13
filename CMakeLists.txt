cmake_minimum_required(VERSION 3.27)
project(Socket_Programming LANGUAGES C CXX)

add_executable(server_tcp server_tcp.cpp)
add_executable(server_udp server_udp.cpp)
add_executable(client_tcp client_tcp.cpp)
add_executable(client_udp client_udp.cpp)

set_property(TARGET server_tcp PROPERTY C_STANDARD 17 C_STANDARD_REQUIRED ON CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
set_property(TARGET server_udp PROPERTY C_STANDARD 17 C_STANDARD_REQUIRED ON CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
set_property(TARGET client_tcp PROPERTY C_STANDARD 17 C_STANDARD_REQUIRED ON CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
set_property(TARGET client_udp PROPERTY C_STANDARD 17 C_STANDARD_REQUIRED ON CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
