/*
 * You are free to modify any part of this file.
 * The only requirement is that when it is run,
 * all your tests are automatically executed
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "structs.h"
#include "helper.h"
#include "myfilesystem.h"

#define TEST(x) test(x, #x)

#define LEN_F1 1024
#define LEN_F2 720
#define LEN_F3 112

static char* f1 = "data/file_data.bin";
static char* f2 = "data/directory_table.bin";
static char* f3 = "data/hash_data.bin";

int file;
int dir;
int hash;

/****************************/
/* Helper function */
void test(int (*test_function) (), char * function_name) {
    int ret = test_function();
    if (ret == 0) {
        printf("Passed %s\n", function_name);
    } else {
        printf("Failed %s returned %d\n", function_name, ret);
    }
}

// Write blank data files for use with testing
void gen_blank_files() {
	write_null_byte(file, LEN_F1, 0);
	write_null_byte(dir, LEN_F2, 0);
	write_null_byte(hash, LEN_F3, 0);
}
/****************************/

int success() {
    return 0;
}

int failure() {
    return 1;
}

// TODO: Write test
int array_empty() {

	return 0;
}

// Test initialising and closing filesystem for memory leaks
int test_no_operation() {
	gen_blank_files();
	filesys_t* helper = init_fs("data/file_data.bin", "data/directory_table.bin", "data/hash_data.bin", 1);
	close_fs(helper);
	return 0;
}

// Test create_file with a single insertion
int test_create_file_success() {
	gen_blank_files();
	filesys_t* helper = init_fs("data/file_data.bin", "data/directory_table.bin", "data/hash_data.bin", 1);
	if (create_file("test1.txt", 50, helper) ||
		create_file("test2.txt", 100, helper) ||
		create_file("test3.txt", 150, helper)) {
		perror ("create_file_success: Create failed");
		return 1;
	}

	close_fs(helper);
	return 0;
}

// Create a file, and attempt to create another with the same name (length can change).
// Also test init_fs reading in existing files from filesystem
int test_create_file_exists() {
	gen_blank_files();
	filesys_t* helper = init_fs("data/file_data.bin", "data/directory_table.bin", "data/hash_data.bin", 1);
	if (create_file("document.txt", 50, helper)) {
		perror ("create_file_exists: Create 1 failed");
		return 1;
	}

	if (create_file("document.txt", 20, helper) != 1) {
		perror ("create_file_exists: Create 2 should fail");
		return 1;
	}
	close_fs(helper);

	helper = init_fs("data/file_data.bin", "data/directory_table.bin", "data/hash_data.bin", 1);
	if (create_file("document.txt", 20, helper) != 1) {
		perror ("create_file_exists: Create 3 should fail");
		return 1;
	}

	close_fs(helper);
	return 0;
}

// Test create_file with insufficient filesystem space
int test_create_file_no_space() {
	gen_blank_files();
	filesys_t* helper = init_fs("data/file_data.bin", "data/directory_table.bin", "data/hash_data.bin", 1);
	if (create_file("test1.txt", 50, helper)) {
		perror ("create_file_no_space: Create 1 failed");
		return 1;
	}

	if (create_file("test2.txt", 975, helper) != 2) {
		perror ("create_file_no_space: Create 2 should fail");
		return 1;
	}

	if (create_file("test3.txt", 974, helper)) {
		perror ("create_file_no_space: Create 3 failed");
		return 1;
	}

	close_fs(helper);
	return 0;
}

int main(int argc, char * argv[]) {
	// Create data folder and blank files for tests to use
	mkdir("data", 0755);
	file = open(f1, O_RDWR | O_CREAT);
	dir = open(f2, O_RDWR | O_CREAT);
	hash = open(f3, O_RDWR | O_CREAT);
	if (file < 0 || dir < 0 || hash < 0) {
		perror("main: Failed to open files");
		exit(1);
	}
	gen_blank_files();

	// Default tests
    TEST(success);
    TEST(failure);

    // Array data structure tests
	TEST(array_empty);

	// Basic filesystem test
	TEST(test_no_operation);

	//
	TEST(test_create_file_success);
	TEST(test_create_file_exists);
	TEST(test_create_file_no_space);



	close(file);
	close(dir);
	close(hash);

    return 0;
}
