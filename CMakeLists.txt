cmake_minimum_required(VERSION 3.10)
project(vfs-local C)

#set(CMAKE_C_STANDARD 11)
#set(GCC_ADDITIONAL_COMPILE_FLAGS "-O0 -std=gnu11 -fsanitize=address -Wall -Werror -g --coverage")
set(GCC_ADDITIONAL_COMPILE_FLAGS "-O0 -std=gnu11 -Wall -Werror -g --coverage")
set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} ${GCC_ADDITIONAL_COMPILE_FLAGS}")

add_executable(runtest runtest.c myfilesystem.c helper.c arr.c)
add_executable(myfuse myfuse.c myfilesystem.c helper.c arr.c)

target_link_libraries(runtest "-lfuse -lm -lpthread")
target_link_libraries(myfuse "-lfuse -lm -lpthread")


