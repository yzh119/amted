cmake_minimum_required(VERSION 3.18)

project(550amtedserver C CXX)

set (CMAKE_CXX_STANDARD 17)
include_directories(include)

file(GLOB AMTED_SERVER_SRC
  src/server.cc
)

file(GLOB CLIENT_SRC
  src/client.cc
)

add_executable(550amtedserver ${AMTED_SERVER_SRC})
add_executable(550client ${CLIENT_SRC})
