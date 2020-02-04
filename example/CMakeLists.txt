cmake_minimum_required(VERSION 3.0)

add_executable (LSM9DS1_demo LSM9DS1_demo.cpp)
target_link_libraries(LSM9DS1_demo lsm9ds1 rt)
target_include_directories(LSM9DS1_demo PRIVATE ..)
