cmake_minimum_required(VERSION 3.18)

project(550amtedserver C)

include_directories(include)

file(GLOB AMTED_SERVER_SRC
  src/server.c
)

file(GLOB CLIENT_SRC
  src/client.c
)

add_executable(550amtedserver ${AMTED_SERVER_SRC})
add_executable(550client ${CLIENT_SRC})
