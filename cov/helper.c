#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <assert.h>

#include "structs.h"
#include "helper.h"

/*
 * Safe malloc helper with error checking
 *
 * size: number of bytes in heap memory which should be allocated
 *
 * returns: address to block in heap memory of specified size
 */
void* salloc(size_t size) {
	void* m = malloc(size);
	assert(m != NULL && "malloc failed");
	return m;
}

/*
 * Safe calloc helper with error checking
 *
 * size: number of bytes in heap memory which should be allocated
 *
 * returns: address to block in heap memory of specified size, with all bytes
 * 			set to 0.
 */
void* scalloc(size_t size) {
	void* m = calloc(size, 1);
	assert(m != NULL && "calloc failed");
	return m;
}

/*
 * Creates a new dynamically allocated file_t struct
 *
 * name: name of file
 * offset: file offset in file_data
 * length: number of bytes in file_data used by file
 * index: entry number in dir_table
 *
 * returns: address of heap allocated file_t struct
 */
file_t* file_init(char* name, uint64_t offset, uint32_t length, int32_t index) {
	file_t* f = salloc(sizeof(*f));
	update_file_name(name, f);
	update_file_offset(offset, f);
	update_file_length(length, f);
	f->index = index;
	f->o_index = -1;
	f->n_index = -1;
	return f;
}

/*
 * Frees file_t struct
 *
 * file: address of heap allocated file_t struct being freed
 */
void free_file(file_t* file) {
	free(file);
}

/*
 * Updates name field of file_t struct
 * Other file_t field update helpers are defined as macros in helper.h
 *
 * name: new name for file_t struct
 * file: address of heap allocated file_t struct to update
 */
void update_file_name(char* name, file_t* file) {
	strncpy(file->name, name, NAME_LEN - 1);
	file->name[NAME_LEN - 1] = '\0';
}

/*
 * Updates the offset field for a file in dir_table
 * Other dir_table field update helpers are defined as macros in helper.h
 *
 * file: address of heap allocated file_t struct to update
 */
void update_dir_offset(file_t* file, filesys_t* fs) {
	// Write 0 to dir_table if newly created zero size file
	if (file->offset >=  MAX_FILE_DATA_LEN) {
		memset(fs->dir + file->index * META_LEN + NAME_LEN,
			   '\0', sizeof(uint32_t));

	// Otherwise write the internal file offset to dir_table
	} else {
		memcpy(fs->dir + file->index * META_LEN + NAME_LEN,
			   &file->offset, sizeof(uint32_t));
	}
}

/*
 * Writes file_t fields to dir_table at its index
 *
 * file: address of heap allocated file_t struct to update
 */
void write_dir_file(file_t* file, filesys_t* fs) {
	update_dir_name(file, fs);
	update_dir_offset(file, fs);
	update_dir_length(file, fs);
}

/*
 * Finds the first non-zero size file in file_data, starting at index
 *
 * index: first index to start searching for non-zero size files
 * o_list: address of offset sorted array
 *
 * returns: file_t* for first non-zero size file from index
 * 			to the end of the array
 * 			NULL if no non-zero size files found
 */
file_t* find_next_nonzero_file(int32_t index, arr_t* arr) {
	file_t** o_list = arr->list;
	int32_t size = arr->size;
	uint64_t start_curr_file = 0;
	uint64_t curr_file_len = 0;

	for (int32_t i = index; i < size; ++i) {
		start_curr_file = o_list[i]->offset;
		curr_file_len = o_list[i]->length;

		// Break if newly created zero size files encountered
		if (start_curr_file >= MAX_FILE_DATA_LEN) {
			break;
		}

		// Non-zero size file found
		if (curr_file_len > 0) {
			return o_list[i];
		}
	}

	return NULL;
}

/*
 * Writes null bytes to a memory mapped file (mmap) at the offset specified
 *
 * f: pointer to start of a memory mapped file
 * offset: offset from the beginning of the file to write null bytes at
 * count: number of null bytes to write
 *
 * returns: number of null bytes written
 */
uint64_t write_null_byte(uint8_t* f, int64_t offset, int64_t count) {
	// Return if no bytes to write
	if (count <= 0) {
		return 0;
	}

	assert(count < MAX_FILE_DATA_LEN && offset >= 0 &&
	       offset < MAX_FILE_DATA_LEN && "invalid args");
	
	// Write null bytes to file
	memset(f + offset, '\0', count);
	return count;
}

/*
 * Writes count null bytes to a file descriptor at offset
 *
 * fd: file descriptor to write to
 * offset: offset from the beginning of the file to write null bytes at
 * count: number of null bytes to write
 *
 * returns: number of null bytes written
 */
uint64_t pwrite_null_byte(int fd, int64_t offset, int64_t count) {
	// Return if no bytes to write
	if (count <= 0) {
		return 0;
	}

	assert(fd >= 0 && count <= MAX_FILE_DATA_LEN &&
	       offset <= MAX_FILE_DATA_LEN && "invalid args");

	for (int64_t i = 0; i < count; ++i) {
		pwrite(fd, "", sizeof(char), offset + i);
	}

	return count;
}
