#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <assert.h>

#include "structs.h"
#include "helper.h"
#include "arr.h"
#include "myfilesystem.h"

// Macro for running test functions
#define TEST(x) test(x, #x)

// Defined file length values
#define F1_LEN (1024) // file_data length
#define F2_LEN (720) // dir_table length
#define F3_LEN (112) // hash_data length

// Static filesystem filenames and file descriptors
static char* f1 = "file_data.bin";
static char* f2 = "directory_table.bin";
static char* f3 = "hash_data.bin";
static int file_fd;
static int dir_fd;
static int hash_fd;

static int error_count;

/*
 * TODO: Insert brief description about how tests are interleaved
 * 		 to test specific scenarios and maximise coverage
 */

/*
 * Helper Functions
 */

/*
 * Runs filesystem tests, prints return values and updates
 * the global error_count
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
 * Writes null bytes to file_data, dir_table and hash_data
 * Used at the beginning of tests to ensure filesystem is reset
 */
void gen_blank_files() {
	pwrite_null_byte(file_fd, 0, F1_LEN);
	pwrite_null_byte(dir_fd, 0, F2_LEN);
	pwrite_null_byte(hash_fd, 0, F3_LEN);
}

/*
 * Filesystem Test Functions
 */

// Tests success
int success() {
    return 0;
}

// Tests failure
int failure() {
	--error_count; // Decrement as failure is expected
    return 1;
}

// Tests myfilesystem helpers for invalid arguments
int test_helper_error_handling() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);

	// Pass empty filesystem to repack_helper
	assert(repack_helper(fs) == -1 && "repack_helper failed");

	// Pass count == 0 to null byte writing helpers
	assert(write_null_byte(NULL, 0, 0) == 0 &&
	       pwrite_null_byte(file_fd, 0, 0) == 0 &&
	       "null byte helpers should return 0 bytes written");

	// Pass length == 0 to compute_hash_block_range
	// Helper should return without assertion failure
	compute_hash_block_range(0, 0, NULL);

	close_fs(fs);
	return 0;
}

// Tests initialising and freeing arrays for memory leaks
int test_array_empty() {
	filesys_t* fs = salloc(sizeof(*fs));
	arr_t* o_list = arr_init(F2_LEN / META_LEN, OFFSET, fs);
	arr_t* n_list = arr_init(F2_LEN / META_LEN, NAME, fs);
	free_arr(o_list);
	free_arr(n_list);
	free(fs);
	return 0;
}

// Tests sorted array insertion
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
		arr_sorted_insert(f[i], fs->o_list);
		arr_sorted_insert(f[i], fs->n_list);
	}

	// Try inserting duplicate file
	assert(arr_sorted_insert(f[3], fs->o_list) == -1 &&
	       arr_sorted_insert(f[3], fs->n_list) == -1 &&
	       "duplicate insertion should fail");

	// Compare offset and name lists with expected
	for (int i = 0; i < 4; ++i) {
		assert(fs->o_list->list[i] == o_expect[i] &&
		       "offset list insertion order incorrect");
		assert(fs->n_list->list[i] == n_expect[i] &&
			   "name list insertion order incorrect");
	}

	close_fs(fs);
	return 0;
}

// Tests retrieval of array elements by key
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
		arr_sorted_insert(f[i], fs->o_list);
		arr_sorted_insert(f[i], fs->n_list);
	}

	// Successful get operations
	assert(arr_get_by_key(&key[0], fs->o_list) == f[4] &&
		   arr_get_by_key(&key[3], fs->n_list) == f[3] &&
		   "incorrect file_t* retrieved");

	// File not found operations
	assert(arr_get_by_key(&key[1], fs->o_list) == NULL &&
		   arr_get_by_key(&key[2], fs->o_list) == NULL &&
		   arr_get_by_key(&key[4], fs->n_list) == NULL &&
		   "file should not be found");

	// Test offset key value comparison for zero size files
	file_t file_a;
	file_t* file_b;
	update_file_offset(MAX_FILE_DATA_LEN, &file_a);
	file_b = file_init("offset.txt", new_file_offset(0, NULL, fs), 0, 7);

	assert(cmp_key(&file_a, file_b, fs->o_list) == -1 &&
		   "newly created zero size file should redirect to lower indices");

	update_file_offset(50, &file_a);
	update_file_offset(50, file_b);
	assert(cmp_key(&file_a, file_b, fs->o_list) == 1 &&
		   "resized zero size file should redirect to higher indices");

	free(file_b);
	close_fs(fs);
	return 0;
}

// Tests removal of array elements by key
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
		arr_sorted_insert(f[i], fs->o_list);
		arr_sorted_insert(f[i], fs->n_list);
	}

	// Remove files using key file_t structs
	file_t* norm_f = arr_remove_by_key(&key[0], fs->o_list);
	file_t* zero_f = arr_remove_by_key(&key[3], fs->n_list);
	assert(norm_f == f[0] && zero_f == f[2] && "removed incorrect files");

	// Remove corresponding entry in opposing list
	assert(arr_remove(norm_f->n_index, fs->n_list) == norm_f &&
		   arr_remove(zero_f->o_index, fs->o_list) == zero_f &&
		   "failed to remove opposing list entry");

	// Attempt to remove files with invalid keys
	assert(arr_remove_by_key(&key[1], fs->o_list) == NULL &&
		   arr_remove_by_key(&key[2], fs->o_list) == NULL &&
		   arr_remove_by_key(&key[4], fs->n_list) == NULL &&
		   "invalid keys should return NULL");

	// Compare offset and name lists with expected
	for (int i = 0; i < 3; ++i) {
		assert(fs->o_list->list[i] == o_expect[i] &&
			   "incorrect offset order after removal");
		assert(fs->n_list->list[i] == n_expect[i] &&
			   "incorrect name order after removal");
	}

	free(norm_f);
	free(zero_f);
	close_fs(fs);
	return 0;
}

// Tests initialising and closing filesystem for memory leaks
int test_no_operation() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);
	close_fs(fs);
	return 0;
}

// Tests init_fs and close_fs for invalid argument handling and reading in
// zero size files on initialisation
int test_init_close_error_handling() {
	// Pass NULL to close_fs
	close_fs(NULL);

	//Create blank filesystem files and create a file of 0 length at offset 0
	gen_blank_files();
	char* name = "zero_size_file";
	pwrite(dir_fd, name, strlen(name), 0);
	fsync(dir_fd);

	filesys_t* fs = init_fs(f1, f2, f3, 1);

	// Read offset and length from dir_table
	file_t dir_table_file;
	pread(dir_fd, &dir_table_file.offset, sizeof(uint32_t), NAME_LEN);
	pread(dir_fd, &dir_table_file.length, sizeof(uint32_t),
			NAME_LEN + OFFSET_LEN);

	// Retrieve file from internal filesystem structure
	file_t key;
	update_file_name(name, &key);
	file_t* internal_file = arr_get_by_key(&key, fs->n_list);

	// Casting used to compare only the first 4 bytes of uint64_t offset
	assert((uint32_t)(dir_table_file.offset) == 0 &&
	       dir_table_file.length == 0 &&
		   internal_file->offset == MAX_FILE_DATA_LEN &&
		   internal_file->length == 0 && "failed to read zero size file");

	close_fs(fs);
	return 0;
}

// Tests create_file with repacking
int test_create_file_success() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);

	assert(!create_file("zero1.txt", 0, fs) &&
	       !create_file("zero2.txt", 0, fs) &&
	       !create_file("test1.txt", 50, fs) &&
		   !create_file("test2.txt", 100, fs) &&
		   !create_file("test3.txt", 800, fs) &&
		   !create_file("zero3.txt", 0, fs) &&
		   "create failed");

	// delete_file to create a gap in file_data
	assert(!delete_file("test2.txt", fs) && "delete failed");

	// create_file in gap created
	assert(!create_file("test4.txt", 20, fs) &&
	       "create in contiguous block failed");

	// Test create_file offset finder repack with NULL argument
	assert(new_file_offset(150, NULL, fs) == 870 &&
	       "offset finder unexpectedly failed");

	// Create and delete files such that a repack is required for test5.txt
	assert(!create_file("block.txt", 100, fs) &&
	       !create_file("byte.txt", 1, fs) &&
	       !delete_file("block.txt", fs) && "creating block gap failed");

	// Create file with repack
	assert(!create_file("test5.txt", 150, fs) && "create with repack failed");

	close_fs(fs);
	return 0;
}

// Attempts to create a duplicate file with the same name, and tests
// reading in existing files within init_fs
int test_create_file_exists() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);

	assert(!create_file("document.txt", 50, fs) && "create failed");

	assert(create_file("document.txt", 20, fs) == 1 &&
	       "creating duplicate should fail");

	// Close and re-initialise file system, before attempting to
	// create duplicate
	close_fs(fs);
	fs = init_fs(f1, f2, f3, 1);

	assert(create_file("document.txt", 20, fs) == 1 &&
		   "creating duplicate after re-initialising should fail");

	close_fs(fs);
	return 0;
}

// Tests behaviour of filesystem when file_data's length is exceeded,
// and when dir_table is completely filled with file entries
int test_create_file_no_space() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);

	assert(!create_file("test1.txt", 50, fs) && "create failed");

	// Create file larger than file_data
	assert(create_file("test2.txt", 975, fs) == 2 &&
	       "creating file too large for file_data should fail");

	assert(!create_file("test3.txt", 974, fs) &&
		   "max size create failed");

	close_fs(fs);

	// Check handling of full dir_table listing
	gen_blank_files();
	fs = init_fs(f1, f2, f3, 1);

	// Pointer to static array for assigning different names to files
	char* names = "abcdefghij";

	for (int i = 0; i < 10; ++i) {
		// Increment pointer after create file to change name of next file
		create_file(names++, 0, fs);
	}

	assert(create_file("dir_table_overload.txt", 0, fs) == 2 &&
		   "dir_table should be full");

	close_fs(fs);
	return 0;
}

// Tests resizing files with repacking and compares dir_table
// values with expected values
int test_resize_file_success() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);

	assert(!create_file("test1.txt", 50, fs) &&
		   !create_file("test2.txt", 10, fs) &&
		   !create_file("zero1.txt", 0, fs) &&
		   !create_file("zero2.txt", 0, fs) &&
		   "create failed");

	assert(!resize_file("test2.txt", 50, fs) && "resize failed");

	// Resize to same size
	assert(!resize_file("test1.txt", 50, fs) && "resize to same size failed");

	// Resize with repack
	assert(!resize_file("test1.txt", 100, fs) &&
		   !resize_file("test2.txt", 100, fs) && "resize repack failed");

	// Resize to zero size file with offset != 0
	assert(!resize_file("test2.txt", 0, fs) &&
	       "resize to zero size non-zero offset failed");

	// Max size resize
	assert(!resize_file("test1.txt", F1_LEN, fs) && // Does not repack
		   !resize_file("test1.txt", 0, fs) &&
		   !resize_file("zero1.txt", F1_LEN, fs) &&
		   "resize to file_data length failed");

	// Check offset and length of files
	file_t f[3]; // file_t structs for test1.txt, text2.txt and zero1.txt
	msync(fs->dir, fs->dir_table_len, MS_SYNC); // Ensure dir_table is synced
	fsync(dir_fd);

	// Read from dir_table
	pread(dir_fd, &f[0].offset, sizeof(uint32_t), NAME_LEN);
	pread(dir_fd, &f[0].length, sizeof(uint32_t), NAME_LEN + OFFSET_LEN);
	pread(dir_fd, &f[1].offset, sizeof(uint32_t), META_LEN + NAME_LEN);
	pread(dir_fd, &f[1].length, sizeof(uint32_t),
			META_LEN + NAME_LEN + OFFSET_LEN);
	pread(dir_fd, &f[2].offset, sizeof(uint32_t),
			2 * META_LEN + NAME_LEN);
	pread(dir_fd, &f[2].length, sizeof(uint32_t),
			2 * META_LEN + NAME_LEN + OFFSET_LEN);

	// Compare dir_table values with expected
	// Casting used to compare only the first 4 bytes of uint64_t offset
	assert((uint32_t)f[0].offset == 0 && f[0].length == 0 &&
		   (uint32_t)f[1].offset == 100 && f[1].length == 0 &&
		   (uint32_t)f[2].offset == 0 && f[2].length == F1_LEN &&
		   "incorrect dir_table values");

	// Resize files to create a gap such that zero2.txt requires
	// a repack on expansion
	assert(!resize_file("zero1.txt", 10, fs) &&
		   !resize_file("test1.txt", 50, fs) &&
		   !resize_file("zero1.txt", 0, fs) &&
		   !resize_file("zero2.txt", 20, fs) &&
		   "resize from zero size with repack failed");


	close_fs(fs);
	return 0;
}

// Attempts to resize files that do not exist
int test_resize_file_does_not_exist() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);

	assert(!create_file("test1.txt", 50, fs) &&
		   !create_file("test2.txt", 10, fs) &&
		   "create failed");

	assert(resize_file("test3.txt", 50, fs) == 1 &&
		   "file being resized does not exist");

	close_fs(fs);
	return 0;
}

// Attempts to resize files to a size larger than the filesystem
int test_resize_file_no_space() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);

	assert(!create_file("test1.txt", 50, fs) && "create failed");

	// Resize to length larger than filesystem
	assert(resize_file("test1.txt", 1025, fs) == 2 &&
		   "resize larger than file_data should fail");

	close_fs(fs);
	return 0;
}

// Tests filesystem repack with zero size files appended
int test_repack_success() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);

	// Create and delete files to create gaps between files
	assert(!create_file("test1.txt", 50, fs) &&
		   !create_file("test2.txt", 10, fs) &&
		   !create_file("test3.txt", 20, fs) &&
		   !create_file("test4.txt", 40, fs) &&
		   !create_file("zero1.txt", 0, fs) &&
		   !delete_file("test1.txt", fs) &&
		   !delete_file("test3.txt", fs) &&
		   "failed to create file gaps");

	repack(fs);

	// TODO: Check filenames in o_list

	close_fs(fs);
	return 0;
}

// Tests the deletion of existing file
int test_delete_file_success() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);

	assert(!create_file("test1.txt", 50, fs) &&
		   !create_file("test2.txt", 50, fs) && "create failed");

	assert(!delete_file("test1.txt", fs) && "delete failed");

	// Check remaining entry in filesystem
	file_t* last = fs->n_list->list[0];
	assert(strncmp(last->name, "test2.txt", NAME_LEN - 1) == 0 &&
	       "incorrect file entry deleted");

	close_fs(fs);
	return 0;
}

// Attempts to delete files that do not exist
int test_delete_file_does_not_exist() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);

	assert(!create_file("test1.txt", 50, fs) && "create failed");

	assert(delete_file("test2.txt", fs) == 1 && "delete should failed");

	close_fs(fs);
	return 0;
}

// Tests the renaming of existing files
int test_rename_file_success() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);

	assert(!create_file("bad.txt", 50, fs) && "create failed");

	assert(!rename_file("bad.txt", "good.txt", fs) && "rename failed");

	// Check name stored in filesystem
	file_t* f = fs->n_list->list[0];
	assert(strncmp(f->name, "good.txt", NAME_LEN - 1) == 0 &&
		   "incorrect file entry name");

	close_fs(fs);
	return 0;
}

// Attempts to rename files with conflicting name arguments
int test_rename_file_name_conflicts() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);

	assert(!create_file("bad.txt", 50, fs) &&
	       !create_file("okay.txt", 50, fs) && "create failed");

	// Test oldname file not found
	assert(rename_file("worst.txt", "good.txt", fs) == 1 &&
	       "file not found should fail");

	// Test newname file already exists
	assert(rename_file("bad.txt", "okay.txt", fs) == 1 &&
		   "file already exists should fail");

	close_fs(fs);
	return 0;
}

// Tests reading data from file_data
int test_read_file_success() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);

	assert(!create_file("basic.txt", 50, fs) && "create failed");

	// Write to file_data
	char* write_buff = "content_to_read"; // String length of 15
	pwrite(file_fd, write_buff, 15, 0);
	fsync(file_fd);
	msync(fs->file, fs->file_data_len, MS_SYNC);

	compute_hash_block(0, fs);
	msync(fs->hash, fs->hash_data_len, MS_SYNC);

	char buff[16];

	// Attempt to read 0 bytes
	assert(!read_file("basic.txt", 0, 0, buff, fs) && "zero byte read failed");

	// Attempt to read 15 bytes
	assert(!read_file("basic.txt", 0, 15, buff, fs) && "read failed");
	buff[15] = '\0';

	assert(strcmp(buff, write_buff) == 0 && "incorrect buffer content");

	close_fs(fs);
	return 0;
}

// Attempts to read from a file which does not exist
int test_read_file_does_not_exist() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);

	assert(!create_file("basic.txt", 50, fs) && "create failed");

	char buff[50];
	assert(read_file("complex.txt", 0, 10, buff, fs) == 1 &&
	       "file read from does not exists");

	close_fs(fs);
	return 0;
}

// Attempts to read a file using an invalid offset and count
int test_read_file_invalid_offset_count() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);

	assert(!create_file("basic.txt", 50, fs) && "create failed");

	char buff[50];
	assert(read_file("basic.txt", 45, 10, buff, fs) == 2 &&
		   "cannot read 10 bytes at offset 45");

	close_fs(fs);
	return 0;
}

// Attempts to read a file after hash_data has been externally modified
int test_read_file_invalid_hash() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);

	assert(!create_file("test1.txt", 50, fs) &&
		   !create_file("test2.txt", 27, fs) &&
		   "create failed");

	// Write to file
	char* buff = "abc";
	assert(!write_file("test1.txt", 0, 3, buff, fs) &&
		   "write failed");

	// Modify root hash value
	pwrite(hash_fd, "132", 3, 0);
	fsync(hash_fd);
	msync(fs->hash, fs->hash_data_len, MS_SYNC);

	assert(read_file("test1.txt", 0, 3, buff, fs) == 3 &&
		   "hash verification should fail");

	close_fs(fs);
	return 0;
}

// Tests writing to file_data with repacking
int test_write_file_success() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);

	assert(!create_file("simple.txt", 950, fs) &&
		   !create_file("impossible.txt", 10, fs) &&
	       !create_file("basic.txt", 50, fs) &&
		   !create_file("complex.txt", 5, fs) && "create failed");

	// Delete simple.txt to create space in filesystem
	assert(!delete_file("simple.txt", fs) && "delete failed");

	char* write_buff = "content_to_write"; // String length of 16
	char buff[17];

	// Write 0 bytes
	assert(!write_file("basic.txt", 0, 0, write_buff, fs) &&
	       "zero byte write failed");

	// Write 16 bytes within file bounds
	assert(!write_file("basic.txt", 0, 16, write_buff, fs) && "write failed");

	// Resize complex.txt to zero size file
	assert(!resize_file("complex.txt", 0, fs) && "resize failed");

	// Write 16 bytes, expanding file bounds and repacking
	assert(!write_file("basic.txt", 50, 16, write_buff, fs) &&
	       "write extend failed");

	assert(!read_file("basic.txt", 0, 16, buff, fs) && "read failed");
	buff[16] = '\0';

	assert(strcmp(buff, write_buff) == 0 && "incorrect buffer content");

	close_fs(fs);
	return 0;
}

// Attempts to write to a file which does not exist
int test_write_file_does_not_exist() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);

	assert(!create_file("basic.txt", 50, fs) && "create failed");

	assert(write_file("complex.txt", 0, 0, "write_content", fs) == 1 &&
		   "file written to does not exist");

	close_fs(fs);
	return 0;
}

// Attempts to write to a file using an invalid offset
int test_write_file_invalid_offset() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);

	assert(!create_file("basic.txt", 50, fs) &&
		   !create_file("complex.txt", 100, fs) && "create failed");

	assert(write_file("complex.txt", 101, 13, "write_content", fs) == 2 &&
		   "offset beyond end of file");

	close_fs(fs);
	return 0;
}

// Attempts to perform a write which exceeds the capacity of file_data
int test_write_file_no_space() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);

	assert(!create_file("basic.txt", 50, fs) &&
		   !create_file("complex.txt", 100, fs) && "create failed");

	// Write 1 byte over the maximum possible write size
	assert(write_file("complex.txt", 100, (F1_LEN - 150) + 1,
		   "write_content", fs) == 3 &&
		   "no space in filesystem for write operation");

	close_fs(fs);
	return 0;
}

// Tests the retrieval of file sizes
int test_file_size_success() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);

	assert(!create_file("big.txt", 50, fs) &&
		   !create_file("bigger.txt", 100, fs) && "create failed");

	assert(file_size("big.txt", fs) == 50 &&
	       file_size("bigger.txt", fs) == 100 &&
	       "incorrect file sizes");

	// Resize and rename file before checking size again
	assert(!resize_file("bigger.txt", 1, fs) &&
	       !rename_file("bigger.txt", "tiny.txt", fs) &&
	       file_size("tiny.txt", fs) == 1 &&
	       "incorrect file size after resize and rename");

	close_fs(fs);
	return 0;
}

// Attempts to find the size of a file which does not exist
int test_file_size_does_not_exist() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);

	assert(!create_file("big.txt", 50, fs) && "create failed");

	assert(file_size("massive.txt", fs) == -1 &&
		   "file does not exist");

	close_fs(fs);
	return 0;
}

// Tests the fletcher hash function using expected and actual values
int test_fletcher_success() {
	// 4 * sizeof(uint32_t) = 16 bytes for hashing
	// Least significant byte of input[3] == 32 (0x20)
	uint32_t input[4] = {1000, 5361, 112, 20256};
	uint32_t sixteen_byte_expected[4] = {26729, 40563, 62758, 94314};
	uint32_t thirteen_byte_expected[4] = {6505, 20339, 42534, 74090};

	uint32_t sixteen_byte_actual[4] = {0};
	uint32_t thirteen_byte_actual[4] = {0};

	fletcher((uint8_t*)input, 16, (uint8_t*)sixteen_byte_actual);
	fletcher((uint8_t*)input, 13, (uint8_t*)thirteen_byte_actual);

	// Compare fletcher output with expected values
	for (int i = 0; i < 4; ++i) {
		assert(sixteen_byte_actual[i] == sixteen_byte_expected[i] &&
			   "sixteen byte fletcher hash incorrect");

		assert(thirteen_byte_actual[i] == thirteen_byte_expected[i] &&
			   "thirteen byte fletcher hash incorrect");
	}

	return 0;
}

// Tests compute_hash_tree for consistency with compute_hash_block
// using write_file calls and external writes to file_data
int test_compute_hash_tree_success() {
	gen_blank_files();
	filesys_t* fs = init_fs(f1, f2, f3, 1);

	// Modify file_data using write_file and save root hash
	assert(!create_file("full_length.txt", 0, fs) && "create failed");

	char* write_buff = "content_to_write"; // String length of 16
	for (int i = 0; i < F1_LEN; i += 16) {
		assert(!write_file("full_length.txt", i, 16, write_buff, fs) &&
		       "write failed");
	}
	
	uint8_t root_hash_expected[HASH_LEN];
	pread(hash_fd, root_hash_expected, HASH_LEN, 0);

	close_fs(fs);

	// Reset filesystem, pwrite to file_data and call compute_hash_tree
	gen_blank_files();
	fs = init_fs(f1, f2, f3, 1);

	for (int i = 0; i < F1_LEN; i += 16) {
		pwrite(file_fd, write_buff, 16, i);
	}
	fsync(file_fd);
	msync(fs->hash, fs->hash_data_len, MS_SYNC);

	compute_hash_tree(fs);

	// Copy root hash and compare with expected
	uint8_t root_hash_actual[HASH_LEN];
	pread(hash_fd, root_hash_actual, HASH_LEN, 0);

	for (int i = 0; i < HASH_LEN; ++i) {
		assert(root_hash_actual[i] == root_hash_expected[i] &&
		       "root hashes do not match");
	}

	close_fs(fs);
	return 0;
}

/*
 * Main Method
 */

int main(int argc, char * argv[]) {
	error_count = 0;

	// Create blank files for tests to use
	file_fd = open(f1, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	dir_fd = open(f2, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	hash_fd = open(f3, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	assert(file_fd >= 0 && dir_fd >= 0 &&
		   hash_fd >= 0 && "failed to open files");
	gen_blank_files();

	// Default tests
    TEST(success);
    TEST(failure);

    // Helper tests
    TEST(test_helper_error_handling);

    // Array data structure tests
	TEST(test_array_empty);
	TEST(test_array_insert);
	TEST(test_array_get);
	TEST(test_array_remove);

	// Basic filesystem tests
	TEST(test_no_operation);
	TEST(test_init_close_error_handling);

	// TODO: parallel cases for each? at least read and write?

	// create_file tests
	TEST(test_create_file_success);
	TEST(test_create_file_exists);
	TEST(test_create_file_no_space);

	// resize_file tests
	TEST(test_resize_file_success);
	TEST(test_resize_file_does_not_exist);
	TEST(test_resize_file_no_space);

	// repack tests
	TEST(test_repack_success);

	// delete_file tests
	TEST(test_delete_file_success);
	TEST(test_delete_file_does_not_exist);

	// rename_file tests
	TEST(test_rename_file_success);
	TEST(test_rename_file_name_conflicts);

	// read_file tests
	TEST(test_read_file_success);
	TEST(test_read_file_does_not_exist);
	TEST(test_read_file_invalid_offset_count);
	TEST(test_read_file_invalid_hash);

	// write_file tests
	TEST(test_write_file_success);
	TEST(test_write_file_does_not_exist);
	TEST(test_write_file_invalid_offset);
	TEST(test_write_file_no_space);

	// file_size tests
	TEST(test_file_size_success);
	TEST(test_file_size_does_not_exist);

	// fletcher tests
	TEST(test_fletcher_success);

	// compute_hash_tree tests
	TEST(test_compute_hash_tree_success);

	// compute_hash_block is effectively tested by other test functions, being
	// used in methods such as create_file, resize_file, repack and write.

	printf("Total Errors: %d\n", error_count);

	close(file_fd);
	close(dir_fd);
	close(hash_fd);

    return error_count;
}


