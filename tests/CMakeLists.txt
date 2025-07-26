set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)

add_executable(
	test_crc
	test_crc.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../crc.cpp
)

add_executable(
	test_e2e
	test_e2e.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../crc.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../e2e.cpp
)

target_include_directories(
	test_crc
	PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_include_directories(
	test_e2e
	PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(test_crc gtest_main gtest)
target_link_libraries(test_e2e gtest_main gtest)

add_test(NAME test_crc COMMAND test_crc)
add_test(NAME test_e2e COMMAND test_e2e)

