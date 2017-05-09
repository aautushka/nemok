project(nemok CXX)

set(SRC
  server.h
  nemok.h
  server.cpp
  client.cpp
  http.h
  http.cpp
)

add_library(nemok ${SRC})
