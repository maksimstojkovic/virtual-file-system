#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "structs.h"
#include "helper.h"
#include "arr.h"
#include "myfilesystem.h"

#define TEST(x) test(x, #x)

#define LEN_F1 1024
#define LEN_F2 720
#define LEN_F3 112

// Static filesystem filenames and file descriptors
static char* f1 = "file_data.bin";
static char* f2 = "directory_table.bin";
static char* f3 = "hash_data.bin";
static int file;
static int dir;
static int hash;

static int error_count;

/*
 * Helper Functions
 */

/*
 * Function for running filesystem tests and printing return values
 */
void test(int (*test_function) (), char * function_name) {
	static long test_count = 1;
	int ret = test_function();
	if (ret == 0) {
		printf("%3ld. Passed %s\n", test_count, function_name);
	} else {
		printf("%3ld. Failed %s returned %d\n", test_count, function_name, ret);
		++error_count;
	}
	++test_count;
}

/*
 * Function for writing null bytes to filesystem files
 * Used at the beginning of tests to ensure files are blank
 */
void gen_blank_files() {
	pwrite_null_byte(file, 0, LEN_F1);
	pwrite_null_byte(dir, 0, LEN_F2);
	pwrite_null_byte(hash, 0, LEN_F3);
}

/*
 * Filesystem Test Functions
 */

// Helper function success test
int success() {
    return 0;
}

// Helper function failure test
int failure() {
	--error_count;
    return 1;
}

// Test initialising and freeing array for memory leaks
int test_array_empty() {
	filesys_t* fs = malloc(sizeof(*fs));
	arr_t* o_list = arr_init(LEN_F2 / META_LEN, OFFSET, fs);
	arr_t* n_list = arr_init(LEN_F2 / META_LEN, NAME, fs);
	free_arr(o_list);
	free_arr(n_list);
	free(fs);
	return 0;
}

// Test sorted index insertion and right index shifting
int test_array_insert() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);

	// Normal and zero size files
	file_t* f[4];
	f[0] = file_init("zero.txt", new_file_offset(0, NULL, fs), 0, 3);
	f[1] = file_init("test3.txt", 5, 10, 0);
	f[2] = file_init("test2.txt", 0, 5, 2);
	f[3] = file_init("test1.txt", 15, 10, 1);

	// Expected order in offset and name arrays
	file_t* o_expect[4] = {f[2], f[1], f[3], f[0]};
	file_t* n_expect[4] = {f[3], f[2], f[1], f[0]};

	// Insert elements into arrays
	for (int i = 0; i < 4; ++i) {
		arr_insert_s(f[i], fs->o_list);
		arr_insert_s(f[i], fs->n_list);
	}

	// Try inserting duplicate file
	if (arr_insert_s(f[3], fs->o_list) != -1 ||
		arr_insert_s(f[3], fs->n_list) != -1) {
		perror("array_insert: Duplicate insertion should fail");
		return 1;
	}

	// Compare offset and name lists with expected
	for (int i = 0; i < 4; ++i) {
		if (fs->o_list->list[i] != o_expect[i]) {
			perror("array_insert: Incorrect offset insertion order");
			return 1;
		}

		if (fs->n_list->list[i] != n_expect[i]) {
			perror("array_insert: Incorrect name insertion order");
			return 1;
		}
	}

	close_fs(fs);
	return 0;
}

// Test get and sorted get from array
int test_array_get() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);

	// Test zero size files and normal files
	// Arbitrary ordering used for dir_table indices
	// Majority of zero size files used to test binary search handling
	file_t* f[7];
	f[0] = file_init("zero4.txt", new_file_offset(0, NULL, fs), 0, 3);
	f[1] = file_init("zero3.txt", new_file_offset(0, NULL, fs), 0, 4);
	f[2] = file_init("zero2.txt", new_file_offset(0, NULL, fs), 0, 6);
	f[3] = file_init("zero1.txt", new_file_offset(0, NULL, fs), 0, 5);
	f[4] = file_init("f3.txt", 5, 10, 0);
	f[5] = file_init("f2.txt", 0, 5, 2);
	f[6] = file_init("f1.txt", 15, 10, 1);

	file_t key[5];
	update_file_offset(5, &key[0]);
	update_file_offset(20, &key[1]);
	update_file_offset(MAX_FILE_DATA_LEN, &key[2]);
	update_file_name("zero1.txt", &key[3]);
	update_file_name("nothing", &key[4]);

	// Insert elements into arrays
	for (int i = 0; i < 7; ++i) {
		arr_insert_s(f[i], fs->o_list);
		arr_insert_s(f[i], fs->n_list);
	}

	// Successful get operations
	if (arr_get_s(&key[0], fs->o_list) != f[4] ||
		arr_get_s(&key[3], fs->n_list) != f[3]) {
		perror("array_get: Incorrect file_t* retrieved");
		return 1;
	}

	// File not found operations
	if (arr_get_s(&key[1], fs->o_list) != NULL ||
		arr_get_s(&key[2], fs->o_list) != NULL ||
		arr_get_s(&key[4], fs->n_list) != NULL) {
		perror("array_get: File should not be found");
		return 1;
	}

	// Test offset key value comparison for zero size file
	file_t file_a;
	file_t* file_b;
	update_file_offset(MAX_FILE_DATA_LEN, &file_a);
	file_b = file_init("offset.txt", new_file_offset(0, NULL, fs), 0, 7);

	if (cmp_key(&file_a, file_b, fs->o_list) != -1) {
		perror("array_get: Zero size file should redirect to lower indices");
		return 1;
	}
	free(file_b);

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

	file_t key[5];
	update_file_offset(5, &key[0]);
	update_file_offset(20, &key[1]);
	update_file_offset(MAX_FILE_DATA_LEN, &key[2]);
	update_file_name("zero1.txt", &key[3]);
	update_file_name("nothing", &key[4]);

	file_t* o_expect[3] = {f[3], f[4], f[1]};
	file_t* n_expect[3] = {f[4], f[3], f[1]};

	// Insert elements into arrays
	for (int i = 0; i < 5; ++i) {
		arr_insert_s(f[i], fs->o_list);
		arr_insert_s(f[i], fs->n_list);
	}

	// Remove files using key file_t structs
	file_t* norm_f = arr_remove_s(&key[0], fs->o_list);
	file_t* zero_f = arr_remove_s(&key[3], fs->n_list);
	if (norm_f != f[0] || zero_f != f[2]) {
		perror("array_remove: Removed incorrect files");
		return 1;
	}

	// Remove corresponding entry in opposing list
	if (arr_remove(norm_f->n_index, fs->n_list) != norm_f ||
		arr_remove(zero_f->o_index, fs->o_list) != zero_f) {
		perror("array_remove: Failed to remove opposing list entry");
		return 1;
	}

	// Attempt to remove files with invalid keys
	if (arr_remove_s(&key[1], fs->o_list) != NULL ||
		arr_remove_s(&key[2], fs->o_list) != NULL ||
		arr_remove_s(&key[4], fs->n_list) != NULL) {
		perror("array_remove: Invalid keys should fail");
		return 1;
	}

	// Compare offset and name lists with expected
	for (int i = 0; i < 3; ++i) {
		if (fs->o_list->list[i] != o_expect[i]) {
			perror("array_remove: Incorrect offset order after removal");
			return 1;
		}

		if (fs->n_list->list[i] != n_expect[i]) {
			perror("array_remove: Incorrect name order after removal");
			return 1;
		}
	}

	free(norm_f);
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

	// Valid create operations
	if (create_file("test1.txt", 50, fs) ||
		create_file("test2.txt", 100, fs) ||
		create_file("test3.txt", 800, fs) ||
		create_file("zero1.txt", 0, fs) ||
		create_file("zero2.txt", 0, fs) ||
		create_file("zero3.txt", 0, fs)) {
		perror ("create_file_success: Create failed");
		return 1;
	}

	// Create file with repack
	// delete_file tests can be found below
	if (delete_file("test2.txt", fs)) {
		perror ("create_file_success: Delete operation failed");
		return 1;
	}

	if (create_file("test4.txt", 150, fs)) {
		perror ("create_file_success: Create with repack failed");
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
		perror ("create_file_no_space: Create failed");
		return 1;
	}

	if (create_file("test2.txt", 975, fs) != 2) {
		perror ("create_file_no_space: File too large, create should fail");
		return 1;
	}

	if (create_file("test3.txt", 974, fs)) {
		perror ("create_file_no_space: Max size create failed");
		return 1;
	}

	close_fs(fs);

	// Check handling of full dir_table listing
	gen_blank_files();
	fs = init_fs(f1, f2, f3, 1);

	char* names = "abcdefghij";

	for (int i = 0; i < 10; ++i) {
		create_file(names++, 0, fs);
	}

	if (create_file("dir_table_overload.txt", 0, fs) != 2) {
		perror ("create_file_no_space: Directory_table should be full");
		return 1;
	}

	close_fs(fs);
	return 0;
}

int test_resize_file_success() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);

	if (create_file("test1.txt", 50, fs) ||
		create_file("test2.txt", 10, fs) ||
		create_file("zero1.txt", 0, fs)) {
		perror ("resize_file_success: Create failed");
		return 1;
	}

	// Valid resize operations
	if (resize_file("test2.txt", 50, fs) ||

		// Resize with repacking
		resize_file("test1.txt", 100, fs) ||
		resize_file("test2.txt", 100, fs) ||

		// Creates file with offset != 0 and length == 0
		resize_file("test2.txt", 0, fs) ||

		resize_file("test1.txt", 1024, fs) ||
		resize_file("test1.txt", 0, fs) ||
		resize_file("zero1.txt", 1024, fs)) {
		perror ("resize_file_success: Resize failed");
		return 1;
	}

	// Check offset and length of files
	file_t f[3]; // file_t structs for test1.txt, text2.txt and zero1.txt
	msync(fs->dir, fs->dir_table_len, MS_SYNC); // Ensure dir_table is synced
	fsync(dir);
	pread(dir, &f[0].offset, sizeof(uint32_t), NAME_LEN);
	pread(dir, &f[0].length, sizeof(uint32_t), NAME_LEN + OFFSET_LEN);
	pread(dir, &f[1].offset, sizeof(uint32_t), META_LEN + NAME_LEN);
	pread(dir, &f[1].length, sizeof(uint32_t), META_LEN + NAME_LEN + OFFSET_LEN);
	pread(dir, &f[2].offset, sizeof(uint32_t), 2 * META_LEN + NAME_LEN);
	pread(dir, &f[2].length, sizeof(uint32_t), 2 * META_LEN + NAME_LEN + OFFSET_LEN);

	// Compare dir_table values with expected
	if ((uint32_t)f[0].offset != 0 || f[0].length != 0 ||
		(uint32_t)f[1].offset != 100 || f[1].length != 0 ||
		(uint32_t)f[2].offset != 0 || f[2].length != 1024) {
		perror ("resize_file_success: Incorrect dir_table values");
		return 1;
	}

	close_fs(fs);
	return 0;
}

int test_resize_file_no_space() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);
	if (create_file("test1.txt", 50, fs)) {
		perror ("resize_file_no_space: Create failed");
		return 1;
	}

	// Resize to length larger than filesystem
	if (resize_file("test1.txt", 1025, fs) != 2) {
		perror ("resize_file_no_space: Resize should fail");
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

int test_hash_verify_invalid() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);
	if (create_file("test1.txt", 50, fs) ||
	   	create_file("test2.txt", 27, fs)) {
		perror ("hash_verify_invalid: Create failed");
		return 1;
	}

	// Write to file
	char buff[3] = {'a', 'b', 'c'};
	if (write_file("test1.txt", 0, 3, buff, fs) != 0) {
		perror ("hash_verify_invalid: Write failed");
		return 1;
	}

	// Modify hash_data.bin and sync
	pwrite(hash, "132", 3, 0);
	fsync(hash);
	msync(fs->hash, fs->hash_data_len, MS_SYNC);

	if (read_file("test1.txt", 0, 3, buff, fs) != 3) {
		perror ("hash_verify_invalid: Read should fail");
		return 1;
	}

	close_fs(fs);
	return 0;
}

int main(int argc, char * argv[]) {
	error_count = 0;

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
	TEST(test_array_insert);
	TEST(test_array_get);
	TEST(test_array_remove);

	// Basic filesystem test
	TEST(test_no_operation);

	// create_file tests
	TEST(test_create_file_success);  //TODO: Create repack cases
	TEST(test_create_file_exists);
	TEST(test_create_file_no_space);

	// resize_file tests
	TEST(test_resize_file_success);
	TEST(test_resize_file_no_space);
	// TODO: TEST RESIZING NORMAL FILE TO 0 BYTES AND

	// delete_file tests
	TEST(test_delete_file_success);

	// hash_verify tests
	TEST(test_hash_verify_invalid);

	printf("Total Errors: %d\n", error_count);

	close(file);
	close(dir);
	close(hash);

    return 0;
}


