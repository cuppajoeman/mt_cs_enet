cmake_minimum_required(VERSION 3.10)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(multithreaded_enet_client)

add_executable(multithreaded_enet_client
	main.cpp 
)

# ENET: reliable UDP
add_subdirectory(external_libraries/enet)
include_directories(external_libraries/enet/include)

target_link_libraries(multithreaded_enet_client enet)
