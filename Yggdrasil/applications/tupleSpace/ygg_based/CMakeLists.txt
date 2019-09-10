
include_directories(../../../)

link_libraries(batman reliable_p2p hashfunctions)

add_executable(YggTs tuple_server.c tuple.c ygg_comm.c)

add_executable(YggTsClient my_client.c tuple.c ygg_comm.c)

add_executable(YggTsBenchA benchmarkA.c tuple.c ygg_comm.c)
add_executable(YggTsBenchB benchmarkB.c tuple.c ygg_comm.c)