
include_directories(../../../)

link_libraries(pthread m hashfunctions list)


add_executable(Ts tuple_server.c tuple.c)

add_executable(TsClient my_client.c tuple.c)
add_executable(TsResolver my_resolver.c tuple.c)

add_executable(TsBenchA benchmarkA.c tuple.c)
add_executable(TsBenchB benchmarkB.c tuple.c)