cmake_minimum_required(VERSION 3.20.0)
set(PLATFORM "POSIX" CACHE STRING "Platform to target")

add_executable(timer_ex timer_ex.c)
target_link_libraries(timer_ex PRIVATE reactor-uc)

add_executable(tcp_ip_bundle_server testing_posix_tcp_ip_bundle_server.c)
target_link_libraries(tcp_ip_bundle_server PUBLIC reactor-uc nanopb)

add_executable(tcp_ip_bundle_client testing_posix_tcp_ip_bundle_client.c)
target_link_libraries(tcp_ip_bundle_client PUBLIC reactor-uc nanopb)


add_executable(tcp_ip_bundle_server_callback testing_tcp_ip_bundle_server_callback.c)
target_link_libraries(tcp_ip_bundle_server_callback PUBLIC reactor-uc nanopb)

add_executable(nanopb_test testing_nanopb.c)
target_link_libraries(nanopb_test PRIVATE reactor-uc nanopb)
