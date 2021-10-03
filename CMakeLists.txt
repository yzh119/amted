cmake_minimum_required(VERSION 3.18)

project(550amtedserver)

file(GLOB AMTED_SERVER_SRC
  src/*.cc
)

add_executable(550amtedserver ${AMTED_SERVER_SRC})