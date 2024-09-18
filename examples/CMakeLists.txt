add_executable(hello hello.c) 
target_link_libraries(hello reactor-uc)

add_executable(hello_action hello_action.c) 
target_link_libraries(hello_action reactor-uc)

add_executable(hello_timer hello_timer.c) 
target_link_libraries(hello_timer reactor-uc)

add_executable(port_ex port_ex.c) 
target_link_libraries(port_ex reactor-uc)

add_executable(shutdown_ex shutdown_ex.c) 
target_link_libraries(shutdown_ex reactor-uc)