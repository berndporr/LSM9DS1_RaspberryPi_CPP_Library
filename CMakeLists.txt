cmake_minimum_required(VERSION 3.1.0)

set(CMAKE_CXX_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif()

project(LSM9DS1_RaspberryPi_Library VERSION 2.0.0 LANGUAGES CXX)
include(GNUInstallDirs)

add_subdirectory(example)

# add_compile_options(-Wall -Wconversion -Wextra -pedantic)

set(LIBSRC LSM9DS1.cpp)
set(LIBINCLUDE LSM9DS1.h LSM9DS1_Registers.h LSM9DS1_Types.h)

add_library(lsm9ds1
  SHARED
  ${LIBSRC}
  )

set_target_properties(lsm9ds1 PROPERTIES
  SOVERSION 2
  PUBLIC_HEADER "${LIBINCLUDE}")

target_link_libraries(lsm9ds1 pigpio)

install(TARGETS lsm9ds1 EXPORT lsm9ds1-targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})



add_library(lsm9ds1_static
  STATIC
  ${LIBSRC}
  )

set_target_properties(lsm9ds1_static PROPERTIES
  PUBLIC_HEADER "${LIBINCLUDE}")

target_link_libraries(lsm9ds1_static pigpio)

install(TARGETS lsm9ds1_static EXPORT lsm9ds1_static-targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
