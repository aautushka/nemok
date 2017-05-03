project(nemok CXX)

set(SRC
  server.h
  nemok.h
  server.cpp
  client.cpp
)

add_library(nemok ${SRC})
