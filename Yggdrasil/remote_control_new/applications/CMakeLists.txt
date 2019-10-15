# Specify the minimum version for CMake

cmake_minimum_required(VERSION 2.8)

# Project's name
project(remote_control_applications_new)

include_directories(../../)
include_directories(../../../Yggdrasil-LowLevelLib)

add_library(control_app_cmp control_app_cmp.c)
target_link_libraries(control_app_cmp yggcore_wireless_new topology_manager aggregation_interface)

add_executable(YggdrasilControlClient ControlClient.c)
target_link_libraries(YggdrasilControlClient cmdio_new)

add_executable(YggdrasilControlProcess ControlStructure.c cmd_server.c)
target_link_libraries(YggdrasilControlProcess yggcore_wireless_new cmdio_new commands_new reliable_dissemination)




add_executable(cmdsetup tools/buildtree.c)
target_link_libraries(cmdsetup cmdio_new)
add_executable(cmdcheck tools/checktree.c)
target_link_libraries(cmdcheck cmdio_new)
add_executable(cmdgetneighbors tools/getneighbors.c)
target_link_libraries(cmdgetneighbors cmdio_new)
add_executable(cmdexecuteexperience tools/beginexperience.c)
target_link_libraries(cmdexecuteexperience cmdio_new)
add_executable(cmdterminateexperience tools/terminateexperience.c)
target_link_libraries(cmdterminateexperience cmdio_new)
add_executable(cmdchangevalue tools/changevalue.c)
target_link_libraries(cmdchangevalue cmdio_new)
add_executable(cmdchangelink tools/changelink.c)
target_link_libraries(cmdchangelink cmdio_new)
add_executable(cmdreboot tools/reboot.c)
target_link_libraries(cmdreboot cmdio_new)
add_executable(cmdshutdown tools/shutdown.c)
target_link_libraries(cmdshutdown cmdio_new)
add_executable(cmdenablediscovery tools/enableannounces.c)
target_link_libraries(cmdenablediscovery cmdio_new)
add_executable(cmddisablediscovery tools/disableannounces.c)
target_link_libraries(cmddisablediscovery cmdio_new)
add_executable(cmddebug tools/debugneighbors.c)
target_link_libraries(cmddebug cmdio_new)


add_custom_target(YggControlBundle)
add_dependencies(YggControlBundle YggdrasilControlProcess YggdrasilControlClient
        cmdsetup cmdcheck cmdgetneighbors cmdexecuteexperience cmdterminateexperience cmdchangevalue cmdchangelink
        cmdreboot cmdshutdown cmdenablediscovery cmddisablediscovery cmddebug)

