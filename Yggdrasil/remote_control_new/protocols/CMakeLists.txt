# Specify the minimum version for CMake

cmake_minimum_required(VERSION 2.8)

# Project's name
project(remote_control_protocols_new)

include_directories(../../)

link_libraries(yggcore_wireless_new)

add_library(discovery discovery.c)
add_library(dissemination dissemination.c)
add_library(reliable_dissemination reliable_dissemination.c)

target_link_libraries(reliable_dissemination PUBLIC discovery PRIVATE hashfunctions)

target_link_libraries(discovery PUBLIC discovery_interface)
target_link_libraries(dissemination PRIVATE hashfunctions)



