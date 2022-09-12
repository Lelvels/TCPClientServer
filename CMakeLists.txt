cmake_minimum_required(VERSION 3.10)
project(ClientServerV2)
add_subdirectory(lib)
add_executable(Server Server.cpp)
add_executable(Client Client.cpp)
add_executable(epollServer epollServer.cpp)
add_executable(epollTest epollTest.cpp)
add_executable(epollClient epollClient.cpp)

set(EXTRA_LIBS get_num)
list(APPEND EXTRA_LIBS error_functions)
list(APPEND EXTRA_LIBS become_daemon)
list(APPEND EXTRA_LIBS inet_sockets)

# set the project name
target_link_libraries(Server PUBLIC -pthread)
target_link_libraries(Client PUBLIC)

#target_link_libraries(Server PUBLIC ${EXTRA_LIBS})
target_include_directories(Server PUBLIC
                            "${PROJECT_BINARY_DIR}"
                            )

target_include_directories(Client PUBLIC 
                            "${PROJECT_BINARY_DIR}"
                            )