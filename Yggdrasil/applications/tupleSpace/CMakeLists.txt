cmake_minimum_required(VERSION 2.8)

# Project's name
project("Tuple Space")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -O3")

add_subdirectory("ygg_based")
add_subdirectory("stand_alone")

add_executable(init init.c)
target_link_libraries(init yggcore_wireless)

#add_executable(clientTs tuple_client.c tuple.c)
#add_executable(fftTs fft.c tuple.c)

#target_compile_definitions(fftTs PUBLIC MACOSX)
#target_compile_definitions(clientTs PUBLIC MACOSX)
#target_compile_definitions(Ts PUBLIC MACOSX)

add_custom_target(ts)
add_dependencies(ts TsBenchA TsBenchB YggTsBenchA YggTsBenchB YggTs Ts)