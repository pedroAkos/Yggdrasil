

link_libraries(pthread m)


add_executable(Ts tuple_server.c tuple.c)

add_executable(TsClient my_client.c tuple.c)
add_executable(TsResolver my_resolver.c tuple.c)