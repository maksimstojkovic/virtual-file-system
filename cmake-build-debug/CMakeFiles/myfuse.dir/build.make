# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.10

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = "/mnt/c/Users/Maksim/Google Drive/_Uni Y3S1/COMP2017/assignment/vfs-local"

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = "/mnt/c/Users/Maksim/Google Drive/_Uni Y3S1/COMP2017/assignment/vfs-local/cmake-build-debug"

# Include any dependencies generated for this target.
include CMakeFiles/myfuse.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/myfuse.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/myfuse.dir/flags.make

CMakeFiles/myfuse.dir/myfuse.c.o: CMakeFiles/myfuse.dir/flags.make
CMakeFiles/myfuse.dir/myfuse.c.o: ../myfuse.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir="/mnt/c/Users/Maksim/Google Drive/_Uni Y3S1/COMP2017/assignment/vfs-local/cmake-build-debug/CMakeFiles" --progress-num=$(CMAKE_PROGRESS_1) "Building C object CMakeFiles/myfuse.dir/myfuse.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/myfuse.dir/myfuse.c.o   -c "/mnt/c/Users/Maksim/Google Drive/_Uni Y3S1/COMP2017/assignment/vfs-local/myfuse.c"

CMakeFiles/myfuse.dir/myfuse.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/myfuse.dir/myfuse.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E "/mnt/c/Users/Maksim/Google Drive/_Uni Y3S1/COMP2017/assignment/vfs-local/myfuse.c" > CMakeFiles/myfuse.dir/myfuse.c.i

CMakeFiles/myfuse.dir/myfuse.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/myfuse.dir/myfuse.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S "/mnt/c/Users/Maksim/Google Drive/_Uni Y3S1/COMP2017/assignment/vfs-local/myfuse.c" -o CMakeFiles/myfuse.dir/myfuse.c.s

CMakeFiles/myfuse.dir/myfuse.c.o.requires:

.PHONY : CMakeFiles/myfuse.dir/myfuse.c.o.requires

CMakeFiles/myfuse.dir/myfuse.c.o.provides: CMakeFiles/myfuse.dir/myfuse.c.o.requires
	$(MAKE) -f CMakeFiles/myfuse.dir/build.make CMakeFiles/myfuse.dir/myfuse.c.o.provides.build
.PHONY : CMakeFiles/myfuse.dir/myfuse.c.o.provides

CMakeFiles/myfuse.dir/myfuse.c.o.provides.build: CMakeFiles/myfuse.dir/myfuse.c.o


CMakeFiles/myfuse.dir/myfilesystem.c.o: CMakeFiles/myfuse.dir/flags.make
CMakeFiles/myfuse.dir/myfilesystem.c.o: ../myfilesystem.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir="/mnt/c/Users/Maksim/Google Drive/_Uni Y3S1/COMP2017/assignment/vfs-local/cmake-build-debug/CMakeFiles" --progress-num=$(CMAKE_PROGRESS_2) "Building C object CMakeFiles/myfuse.dir/myfilesystem.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/myfuse.dir/myfilesystem.c.o   -c "/mnt/c/Users/Maksim/Google Drive/_Uni Y3S1/COMP2017/assignment/vfs-local/myfilesystem.c"

CMakeFiles/myfuse.dir/myfilesystem.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/myfuse.dir/myfilesystem.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E "/mnt/c/Users/Maksim/Google Drive/_Uni Y3S1/COMP2017/assignment/vfs-local/myfilesystem.c" > CMakeFiles/myfuse.dir/myfilesystem.c.i

CMakeFiles/myfuse.dir/myfilesystem.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/myfuse.dir/myfilesystem.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S "/mnt/c/Users/Maksim/Google Drive/_Uni Y3S1/COMP2017/assignment/vfs-local/myfilesystem.c" -o CMakeFiles/myfuse.dir/myfilesystem.c.s

CMakeFiles/myfuse.dir/myfilesystem.c.o.requires:

.PHONY : CMakeFiles/myfuse.dir/myfilesystem.c.o.requires

CMakeFiles/myfuse.dir/myfilesystem.c.o.provides: CMakeFiles/myfuse.dir/myfilesystem.c.o.requires
	$(MAKE) -f CMakeFiles/myfuse.dir/build.make CMakeFiles/myfuse.dir/myfilesystem.c.o.provides.build
.PHONY : CMakeFiles/myfuse.dir/myfilesystem.c.o.provides

CMakeFiles/myfuse.dir/myfilesystem.c.o.provides.build: CMakeFiles/myfuse.dir/myfilesystem.c.o


CMakeFiles/myfuse.dir/helper.c.o: CMakeFiles/myfuse.dir/flags.make
CMakeFiles/myfuse.dir/helper.c.o: ../helper.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir="/mnt/c/Users/Maksim/Google Drive/_Uni Y3S1/COMP2017/assignment/vfs-local/cmake-build-debug/CMakeFiles" --progress-num=$(CMAKE_PROGRESS_3) "Building C object CMakeFiles/myfuse.dir/helper.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/myfuse.dir/helper.c.o   -c "/mnt/c/Users/Maksim/Google Drive/_Uni Y3S1/COMP2017/assignment/vfs-local/helper.c"

CMakeFiles/myfuse.dir/helper.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/myfuse.dir/helper.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E "/mnt/c/Users/Maksim/Google Drive/_Uni Y3S1/COMP2017/assignment/vfs-local/helper.c" > CMakeFiles/myfuse.dir/helper.c.i

CMakeFiles/myfuse.dir/helper.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/myfuse.dir/helper.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S "/mnt/c/Users/Maksim/Google Drive/_Uni Y3S1/COMP2017/assignment/vfs-local/helper.c" -o CMakeFiles/myfuse.dir/helper.c.s

CMakeFiles/myfuse.dir/helper.c.o.requires:

.PHONY : CMakeFiles/myfuse.dir/helper.c.o.requires

CMakeFiles/myfuse.dir/helper.c.o.provides: CMakeFiles/myfuse.dir/helper.c.o.requires
	$(MAKE) -f CMakeFiles/myfuse.dir/build.make CMakeFiles/myfuse.dir/helper.c.o.provides.build
.PHONY : CMakeFiles/myfuse.dir/helper.c.o.provides

CMakeFiles/myfuse.dir/helper.c.o.provides.build: CMakeFiles/myfuse.dir/helper.c.o


CMakeFiles/myfuse.dir/arr.c.o: CMakeFiles/myfuse.dir/flags.make
CMakeFiles/myfuse.dir/arr.c.o: ../arr.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir="/mnt/c/Users/Maksim/Google Drive/_Uni Y3S1/COMP2017/assignment/vfs-local/cmake-build-debug/CMakeFiles" --progress-num=$(CMAKE_PROGRESS_4) "Building C object CMakeFiles/myfuse.dir/arr.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/myfuse.dir/arr.c.o   -c "/mnt/c/Users/Maksim/Google Drive/_Uni Y3S1/COMP2017/assignment/vfs-local/arr.c"

CMakeFiles/myfuse.dir/arr.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/myfuse.dir/arr.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E "/mnt/c/Users/Maksim/Google Drive/_Uni Y3S1/COMP2017/assignment/vfs-local/arr.c" > CMakeFiles/myfuse.dir/arr.c.i

CMakeFiles/myfuse.dir/arr.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/myfuse.dir/arr.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S "/mnt/c/Users/Maksim/Google Drive/_Uni Y3S1/COMP2017/assignment/vfs-local/arr.c" -o CMakeFiles/myfuse.dir/arr.c.s

CMakeFiles/myfuse.dir/arr.c.o.requires:

.PHONY : CMakeFiles/myfuse.dir/arr.c.o.requires

CMakeFiles/myfuse.dir/arr.c.o.provides: CMakeFiles/myfuse.dir/arr.c.o.requires
	$(MAKE) -f CMakeFiles/myfuse.dir/build.make CMakeFiles/myfuse.dir/arr.c.o.provides.build
.PHONY : CMakeFiles/myfuse.dir/arr.c.o.provides

CMakeFiles/myfuse.dir/arr.c.o.provides.build: CMakeFiles/myfuse.dir/arr.c.o


# Object files for target myfuse
myfuse_OBJECTS = \
"CMakeFiles/myfuse.dir/myfuse.c.o" \
"CMakeFiles/myfuse.dir/myfilesystem.c.o" \
"CMakeFiles/myfuse.dir/helper.c.o" \
"CMakeFiles/myfuse.dir/arr.c.o"

# External object files for target myfuse
myfuse_EXTERNAL_OBJECTS =

myfuse: CMakeFiles/myfuse.dir/myfuse.c.o
myfuse: CMakeFiles/myfuse.dir/myfilesystem.c.o
myfuse: CMakeFiles/myfuse.dir/helper.c.o
myfuse: CMakeFiles/myfuse.dir/arr.c.o
myfuse: CMakeFiles/myfuse.dir/build.make
myfuse: CMakeFiles/myfuse.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir="/mnt/c/Users/Maksim/Google Drive/_Uni Y3S1/COMP2017/assignment/vfs-local/cmake-build-debug/CMakeFiles" --progress-num=$(CMAKE_PROGRESS_5) "Linking C executable myfuse"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/myfuse.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/myfuse.dir/build: myfuse

.PHONY : CMakeFiles/myfuse.dir/build

CMakeFiles/myfuse.dir/requires: CMakeFiles/myfuse.dir/myfuse.c.o.requires
CMakeFiles/myfuse.dir/requires: CMakeFiles/myfuse.dir/myfilesystem.c.o.requires
CMakeFiles/myfuse.dir/requires: CMakeFiles/myfuse.dir/helper.c.o.requires
CMakeFiles/myfuse.dir/requires: CMakeFiles/myfuse.dir/arr.c.o.requires

.PHONY : CMakeFiles/myfuse.dir/requires

CMakeFiles/myfuse.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/myfuse.dir/cmake_clean.cmake
.PHONY : CMakeFiles/myfuse.dir/clean

CMakeFiles/myfuse.dir/depend:
	cd "/mnt/c/Users/Maksim/Google Drive/_Uni Y3S1/COMP2017/assignment/vfs-local/cmake-build-debug" && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" "/mnt/c/Users/Maksim/Google Drive/_Uni Y3S1/COMP2017/assignment/vfs-local" "/mnt/c/Users/Maksim/Google Drive/_Uni Y3S1/COMP2017/assignment/vfs-local" "/mnt/c/Users/Maksim/Google Drive/_Uni Y3S1/COMP2017/assignment/vfs-local/cmake-build-debug" "/mnt/c/Users/Maksim/Google Drive/_Uni Y3S1/COMP2017/assignment/vfs-local/cmake-build-debug" "/mnt/c/Users/Maksim/Google Drive/_Uni Y3S1/COMP2017/assignment/vfs-local/cmake-build-debug/CMakeFiles/myfuse.dir/DependInfo.cmake" --color=$(COLOR)
.PHONY : CMakeFiles/myfuse.dir/depend
