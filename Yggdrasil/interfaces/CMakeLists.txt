# Specify the minimum version for CMake

cmake_minimum_required(VERSION 2.8)

# Project's name
project(Interfaces)


include_directories(../)
include_directories(../core)
include_directories(../../Yggdrasil-LowLevelLib)

#ONLY WIRELESS
link_libraries(yggcore_wireless_new)

add_library(aggregation_interface
        aggregation/aggregation_functions.c
        aggregation/aggregation_operations.c
        )


add_library(discovery_interface
        discovery/discovery_events.c
        )


add_library(routing_interface
        routing/routing_interface.c
        )


