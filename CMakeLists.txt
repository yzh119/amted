cmake_minimum_required(VERSION 3.10)

project(550amtedserver C CXX)

set (CMAKE_CXX_STANDARD 17)
include_directories(include)

file(GLOB AMTED_SERVER_SRC
  src/server.cc
)

file(GLOB CLIENT_SRC
  src/client.cc
)


message(STATUS "Check if the system supports epoll")
if(UNIX)
  include(CheckSymbolExists)
  check_symbol_exists(epoll_create "sys/epoll.h" EPOLL_PROTOTYPE_EXISTS)
endif()

if(EPOLL_PROTOTYPE_EXISTS)
  message(STATUS "Check if the system supports epoll - yes")
  message(STATUS "Compile both client and server.")
  add_definitions(-DUSE_EPOLL)
  add_executable(550amtedserver ${AMTED_SERVER_SRC})
  target_link_libraries(550amtedserver pthread)
  install(TARGETS 550amtedserver DESTINATION /usr/bin)
else()
  message(STATUS "Check if the system supports epoll - no")
  message(STATUS "Only compile client.")
endif()
add_executable(550client ${CLIENT_SRC})

install(TARGETS 550client DESTINATION /usr/bin)