#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "structs.h"
#include "helper.h"

// Variables for checking endianness
static const unsigned int le_int = 1;
static const char* le_char = (char*)&le_int;

// Safe malloc helper
void* salloc(size_t size) {
	void* m = malloc(size);
	if (m == NULL) {
		perror("salloc: malloc failed");
		exit(1);
	}
	return m;
}

// Safe calloc helper
void* scalloc(size_t size) {
	void* m = calloc(size, 1);
	if (m == NULL) {
		perror("scalloc: calloc failed");
		exit(1);
	}
	return m;
}

// Check if system is little endian
int l_end() {
	return *le_char;
}

// Unsigned to little endian and vice versa
// (called when updating file_t offset/length, or reading dir_table)
uint32_t utole(uint64_t in) {
	// Return 0 if zero size file
	if (in >= (uint64_t)MAX_NUM_BLOCKS * BLOCK_LENGTH) {
		return 0;
	}
	
	if (l_end()) {
		// Return casted value if little endian
		return (uint32_t)in;
	} else {
		// Reverse bytes if big endian
		uint32_t in_cast = (uint32_t)in;
		uint32_t out = 0;
		for (int i = 0; i < 4; i++) {
			*((char*)&out + i) = *((char*)&in_cast + (3-i));
		}
		return out;
	}
}

// Create new dynamically allocated file_t struct
file_t* new_file_t(char* name, uint64_t offset, uint32_t length, int32_t index) {
	file_t* f = salloc(sizeof(*f));
	// if (pthread_mutex_init(&f->f_mutex, NULL)) {
	// 	perror("new_file_t: Failed to initialise mutex");
	// 	exit(1);
	// }
	update_file_name(name, f);
	update_file_offset(offset, f);
	update_file_length(length, f);
	f->index = index;
	f->o_index = -1;
	f->n_index = -1;
	return f;
}

// Free dynamically allocated file_t struct
void free_file_t(file_t* file) {
	// pthread_mutex_destroy(&file->f_mutex);
	free(file);
}

// Update file_t struct name
void update_file_name(char* name, file_t* file) {
	strncpy(file->name, name, NAME_LENGTH - 1);
	file->name[NAME_LENGTH - 1] = '\0';
}

// Update file_t struct offset values
void update_file_offset(uint64_t offset, file_t* file) {
	file->offset = offset;
	file->offset_le = utole(offset);
}

// Update file_t struct lenth values
void update_file_length(uint32_t length, file_t* file) {
	file->length = length;
	file->length_le = utole(length);
}

// Update a file's name field in dir_table (does not sync)
void update_dir_name(file_t* file, filesys_t* fs) {
	memcpy(fs->dir + file->index * META_LENGTH, file->name, strlen(file->name)+1);
	// pwrite(fs->dir, file->name, strlen(file->name) + 1,
	// 	   file->index * META_LENGTH);
}

// Update a file's offset field in dir_table (does not sync)
void update_dir_offset(file_t* file, filesys_t* fs) {
	memcpy(fs->dir + file->index * META_LENGTH + NAME_LENGTH,
		   &file->offset_le, sizeof(uint32_t));
	// pwrite(fs->dir, &file->offset_le, sizeof(uint32_t),
	// 	   file->index * META_LENGTH + NAME_LENGTH);
}

// Update a file's offset field in dir_table (does not sync)
void update_dir_length(file_t* file, filesys_t* fs) {
	memcpy(fs->dir + file->index * META_LENGTH + NAME_LENGTH + OFFSET_LENGTH,
		   &file->length_le, sizeof(uint32_t));
	// pwrite(fs->dir, &file->length_le, sizeof(uint32_t),
	// 	   file->index * META_LENGTH + NAME_LENGTH + OFFSET_LENGTH);
}

// Write a file to dir_table at the index within file_t (does not sync)
void write_dir_file(file_t* file, filesys_t* fs) {
	update_dir_name(file, fs);
	update_dir_offset(file, fs);
	update_dir_length(file, fs);
}

// Write count null bytes to a file at offset
void write_null_byte(uint8_t* f, int64_t count, int64_t offset) {
	// Return if no bytes to write
	if (count <= 0) {
		return;
	}
	
	printf("%ld %ld\n", count, offset);
	
	// Check for invalid arguments
	if (count >= MAX_FILE_DATA_LENGTH || offset < 0 ||
	   	offset >= MAX_FILE_DATA_LENGTH) {
		perror("write_null_byte: Invalid arguments");
		exit(1);
	}
	
	// Write null bytes to mmap'd file
	memset(f+offset, '\0', count);
}

// Write count null bytes to a file descriptor at offset
// Used for testcases
// NOTE: DOES NOT CHECK FOR VALID OFFSET OR BYTE COUNT
void pwrite_null_byte(int fd, int64_t count, int64_t offset) {
	for (int64_t i = 0; i < count; i++) {
		pwrite(fd, "", sizeof(char), offset + i);
	}
}

// Returns index of parent in hash array for node at index
int32_t p_index(int32_t index) {
	// Root node parent index
	if (index <= 0) {
		return -1;
	}
	
	// Odd and even node parent indices respectively
	if (index % 2 == 1) {
		return index / 2;
	} else {
		return (index / 2) - 1;
	}
}

// Returns index of left child in hash array for node at index
int32_t lc_index(int32_t index) {
	return (2 * index) + 1;
}

// Returns index of right child in hash array for node at index
int32_t rc_index(int32_t index) {
	return (2 * index) + 2;
}


