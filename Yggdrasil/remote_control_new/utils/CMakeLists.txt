# Specify the minimum version for CMake

cmake_minimum_required(VERSION 2.8)

# Project's name
project(remote_control_utils_new)

include_directories(../../)

add_library(commands_new commands.c)
target_link_libraries(commands_new topology_manager aggregation_interface)


add_library(cmdio_new cmdio.c control_protocol_utils.c)
target_link_libraries(cmdio_new uuid)
