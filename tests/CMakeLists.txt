project(tests CXX)

enable_testing()
include_directories (..)

set(SRC 
  server_tests
  telnet_tests
  http_tests
  ev2_echo_tests
  http_parser_tests
)

add_executable(tests ${SRC})
target_link_libraries(tests gtest gtest_main pthread nemok event)
