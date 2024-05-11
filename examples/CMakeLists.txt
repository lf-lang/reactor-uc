add_executable(hello hello.c) 
target_link_libraries(hello reactor-uc)

add_executable(hello_action hello_action.c) 
target_link_libraries(hello_action reactor-uc)