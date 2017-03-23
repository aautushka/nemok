project(tests CXX)

enable_testing()
include_directories (..)

set(SRC 
	tests
)

add_executable(tests ${SRC})
target_link_libraries(tests gtest gtest_main pthread)
