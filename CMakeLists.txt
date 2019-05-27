cmake_minimum_required(VERSION 3.10)
project(vfs-local C)

#set(CMAKE_C_STANDARD 11)
set(GCC_ADDITIONAL_COMPILE_FLAGS "-O0 -std=gnu11 -fsanitize=address -Wall -Werror -g --coverage")
set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} ${GCC_ADDITIONAL_COMPILE_FLAGS}")

add_executable(runtest runtest.c myfilesystem.c helper.c arr.c)
target_link_libraries(runtest "-lm -lpthread -lfuse")
#configure_file(${CMAKE_CURRENT_SOURCE_DIR}/data/file_data.bin
#        ${CMAKE_CURRENT_BINARY_DIR}/data/file_data.bin COPYONLY)
#configure_file(${CMAKE_CURRENT_SOURCE_DIR}/data/directory_table.bin
#        ${CMAKE_CURRENT_BINARY_DIR}/data/directory_table.bin COPYONLY)
#configure_file(${CMAKE_CURRENT_SOURCE_DIR}/data/hash_data.bin
#        ${CMAKE_CURRENT_BINARY_DIR}/data/hash_data.bin COPYONLY)

