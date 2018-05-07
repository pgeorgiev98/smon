cmake_minimum_required(VERSION 2.8)
project(smon)
add_executable(smon main.c system.c util.c)
set_property(TARGET smon PROPERTY C_STANDARD 99)

# Install
include(GNUInstallDirs)
install(TARGETS smon
	DESTINATION "${CMAKE_INSTALL_BINDIR}")

# Show Warnings
if (CMAKE_COMPILER_IS_GNUCC)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
endif()
