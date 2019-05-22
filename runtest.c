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
#include <sys/mman.h>

#include "structs.h"
#include "helper.h"
// #include "synclist.h"
#include "arr.h"
#include "myfilesystem.h"

#define TEST(x) test(x, #x)

#define LEN_F1 1024
#define LEN_F2 720
#define LEN_F3 112

static char* f1 = "file_data.bin";
static char* f2 = "directory_table.bin";
static char* f3 = "hash_data.bin";

int file;
int dir;
int hash;

/****************************/
/* Helper function */
void test(int (*test_function) (), char * function_name) {
	static long test_count = 1;
	int ret = test_function();
	if (ret == 0) {
		printf("%3ld. Passed %s\n", test_count, function_name);
	} else {
		printf("%3ld. Failed %s returned %d\n", test_count, function_name, ret);
	}
	test_count++;
}

// Write blank data files for use with testing
void gen_blank_files() {
	pwrite_null_byte(file, LEN_F1, 0);
	pwrite_null_byte(dir, LEN_F2, 0);
	pwrite_null_byte(hash, LEN_F3, 0);
}
/****************************/

int success() {
    return 0;
}

int failure() {
    return 1;
}

// Test initialising and freeing array for memory leaks
int test_array_empty() {
	filesys_t* fs = malloc(sizeof(*fs));
	arr_t* o_list = arr_init(LEN_F2/META_LENGTH, OFFSET, fs);
	arr_t* n_list = arr_init(LEN_F2/META_LENGTH, NAME, fs);
	free_arr(o_list);
	free_arr(n_list);
	free(fs);
	return 0;
}

// Test get from array
int test_array_get() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);
	file_t* f[5];
	f[0] = file_init("test3.txt", 5, 10, 0);
	f[1] = file_init("zero1.txt", new_file_offset(0, NULL, fs), 0, 3);
	f[2] = file_init("zero2.txt", new_file_offset(0, NULL, fs), 0, 4);
	f[3] = file_init("test2.txt", 0, 5, 2);
	f[4] = file_init("test1.txt", 15, 10, 1);

	file_t key[4];
	update_file_offset(5, &key[0]);
	update_file_name("zero1.txt", &key[1]);
	update_file_offset(20, &key[2]);
	update_file_name("nothing", &key[3]);

	// Insert elements into arrays
	for (int i = 0; i < 5; ++i) {
		arr_insert_s(f[i], fs->o_list);
		arr_insert_s(f[i], fs->n_list);
	}

	// Compare successful get operations
	if (arr_get_s(&key[0], fs->o_list) != f[0] ||
		arr_get_s(&key[1], fs->n_list) != f[1]) {
		perror("array_get: Incorrect file_t* retrieved");
		return 1;
	}

	// Compare file not found operations
	if (arr_get_s(&key[2], fs->o_list) != NULL ||
		arr_get_s(&key[3], fs->n_list) != NULL) {
		perror("array_get: File should not be found");
		return 2;
	}

	close_fs(fs);
	return 0;
}

// Test sorted index insertion and right element shifting with normal
// and zero size files
int test_array_insert() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);
	file_t* f[4];
	f[0] = file_init("test3.txt", 5, 10, 0);
	f[1] = file_init("zero.txt", new_file_offset(0, NULL, fs), 0, 3);
	f[2] = file_init("test2.txt", 0, 5, 2);
	f[3] = file_init("test1.txt", 15, 10, 1);

	file_t* o_expect[4] = {f[2], f[0], f[3], f[1]};
	file_t* n_expect[4] = {f[3], f[2], f[0], f[1]};

	// Insert elements into arrays
	for (int i = 0; i < 4; ++i) {
		arr_insert_s(f[i], fs->o_list);
		arr_insert_s(f[i], fs->n_list);
	}

	// Try inserting duplicate
	if (arr_insert_s(f[3], fs->o_list) != -1 ||
		arr_insert_s(f[3], fs->n_list) != -1) {
		perror("array_insert: Duplicate insertion should fail");
		return 1;
	}

	// Compare offset and name lists with expected
	for (int i = 0; i < 4; ++i) {
		if (fs->o_list->list[i] != o_expect[i]) {
			perror("array_insert: Incorrect offset insertion order");
			return 2;
		}

		if (fs->n_list->list[i] != n_expect[i]) {
			perror("array_insert: Incorrect name insertion order");
			return 3;
		}
	}

	close_fs(fs);
	return 0;
}

// Test sorted removal and left element shifting with normal
// and zero size files
int test_array_remove() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);
	file_t* f[5];
	f[0] = file_init("test3.txt", 5, 10, 0);
	f[1] = file_init("zero2.txt", new_file_offset(0, NULL, fs), 0, 3);
	f[2] = file_init("zero1.txt", new_file_offset(0, NULL, fs), 0, 4);
	f[3] = file_init("test2.txt", 0, 5, 2);
	f[4] = file_init("test1.txt", 15, 10, 1);

	file_t key[4];
	update_file_offset(5, &key[0]);
	update_file_name("zero1.txt", &key[1]);
	update_file_offset(20, &key[2]);
	update_file_name("nothing", &key[3]);

	file_t* o_expect[3] = {f[3], f[4], f[1]};
	file_t* n_expect[3] = {f[4], f[3], f[1]};

	// Insert elements into arrays
	for (int i = 0; i < 5; ++i) {
		arr_insert_s(f[i], fs->o_list);
		arr_insert_s(f[i], fs->n_list);
	}

	// Remove files with valid keys
	// Zero size files can only be found by name
	file_t* test_f = arr_remove_s(&key[0], fs->o_list);
	file_t* zero_f = arr_remove_s(&key[1], fs->n_list);
	if (test_f != f[0] || zero_f != f[2]) {
		perror("array_remove: Removed incorrect files");
		return 1;
	}

	// Remove corresponding entry in opposing list
	if (arr_remove(test_f->n_index, fs->n_list) != test_f ||
		arr_remove(zero_f->o_index, fs->o_list) != zero_f) {
		perror("array_remove: Failed to remove opposing list entry");
		return 2;
	}

	// Attempt to remove files with invalid keys
	if (arr_remove_s(&key[2], fs->o_list) != NULL ||
		arr_remove_s(&key[3], fs->n_list) != NULL) {
		perror("array_remove: Invalid keys should fail");
		return 3;
	}

	// Compare offset and name lists with expected
	for (int i = 0; i < 3; ++i) {
		if (fs->o_list->list[i] != o_expect[i]) {
			perror("array_remove: Incorrect offset order after removal");
			return 4;
		}

		if (fs->n_list->list[i] != n_expect[i]) {
			perror("array_remove: Incorrect name order after removal");
			return 5;
		}
	}

	free(test_f);
	free(zero_f);
	close_fs(fs);
	return 0;
}

// Test initialising and closing filesystem for memory leaks
int test_no_operation() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);
	close_fs(fs);
	return 0;
}

// Test create_file with a single insertion
int test_create_file_success() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);
	if (create_file("test1.txt", 50, fs) ||
		create_file("test2.txt", 100, fs) ||
		create_file("test3.txt", 150, fs)) {
		perror ("create_file_success: Create failed");
		return 1;
	}

	close_fs(fs);
	return 0;
}

// Create a file, and attempt to create another with the same name (length can change).
// Also test init_fs reading in existing files from filesystem
int test_create_file_exists() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);
	if (create_file("document.txt", 50, fs)) {
		perror ("create_file_exists: Create 1 failed");
		return 1;
	}

	if (create_file("document.txt", 20, fs) != 1) {
		perror ("create_file_exists: Create 2 should fail");
		return 1;
	}
	close_fs(fs);

	fs = init_fs(f1, f2, f3, 1);
	if (create_file("document.txt", 20, fs) != 1) {
		perror ("create_file_exists: Create 3 should fail");
		return 1;
	}

	close_fs(fs);
	return 0;
}

// Test create_file with insufficient filesystem space
int test_create_file_no_space() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);
	if (create_file("test1.txt", 50, fs)) {
		perror ("create_file_no_space: Create 1 failed");
		return 1;
	}

	if (create_file("test2.txt", 975, fs) != 2) {
		perror ("create_file_no_space: Create 2 should fail");
		return 1;
	}

	if (create_file("test3.txt", 974, fs)) {
		perror ("create_file_no_space: Create 3 failed");
		return 1;
	}

	close_fs(fs);
	return 0;
}

int test_delete_file_success() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);
	if (create_file("test1.txt", 50, fs)) {
		perror ("delete_file_success: Create failed");
		return 1;
	}
	
	if (delete_file("test1.txt", fs)) {
		perror ("delete_file_success: Delete failed");
		return 1;
	}

	close_fs(fs);
	return 0;
}

int test_hash_verify_success() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);
	if (create_file("test1.txt", 50, fs) ||
	   	create_file("test2.txt", 27, fs)) {
		perror ("hash_verify_success: Create failed");
		return 1;
	}

	pwrite(hash, "132", 3, 0);
	msync(fs->hash, fs->len[2], MS_SYNC);

	// compute_hash_tree(fs);
	
	char buff[10];
	if (read_file("test1.txt", 0, 1, buff, fs) != 3) {
		perror ("hash_verify_success: Read should fail");
		return 1;
	}

	close_fs(fs);
	return 0;
}

int main(int argc, char * argv[]) {
	// Create blank files for tests to use
	file = open(f1, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	dir = open(f2, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	hash = open(f3, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if (file < 0 || dir < 0 || hash < 0) {
		perror("main: Failed to open files");
		exit(1);
	}
	gen_blank_files();

	// Default tests
    TEST(success);
    TEST(failure);

    // Array data structure tests
	TEST(test_array_empty);
	TEST(test_array_get);
	TEST(test_array_insert);
	TEST(test_array_remove);

	// Basic filesystem test
	TEST(test_no_operation);

	// create_file tests
	TEST(test_create_file_success);
	TEST(test_create_file_exists);
	TEST(test_create_file_no_space);

	// delete_file tests
	TEST(test_delete_file_success);

	// hash_verify tests
	TEST(test_hash_verify_success);

	close(file);
	close(dir);
	close(hash);

    return 0;
}


