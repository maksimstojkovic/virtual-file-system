#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "structs.h"
#include "helper.h"

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

// Create new dynamically allocated file_t struct
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

// Free dynamically allocated file_t struct
void free_file(file_t* file) {
	free(file);
}

// Update file_t struct name
// Other file_t field helpers are defined as macros in helper.h
void update_file_name(char* name, file_t* file) {
	strncpy(file->name, name, NAME_LENGTH - 1);
	file->name[NAME_LENGTH - 1] = '\0';
}

// Update a file's offset field in dir_table
// Other dir_table update helpers are defined as macros in helper.h
void update_dir_offset(file_t* file, filesys_t* fs) {
	// If file length is greater than zero, write its offset
	if (file->length > 0) {
		memcpy(fs->dir + file->index * META_LENGTH + NAME_LENGTH,
		   &file->offset, sizeof(uint32_t));
	
	// Otherwise write 0 for zero size files
	} else {
		memset(fs->dir + file->index * META_LENGTH + NAME_LENGTH, '\0', sizeof(uint32_t));
	}
}

// Write a file to dir_table at the index within the file_t
void write_dir_file(file_t* file, filesys_t* fs) {
	update_dir_name(file, fs);
	update_dir_offset(file, fs);
	update_dir_length(file, fs);
}

// Write count null bytes to a file at offset
void write_null_byte(uint8_t* f, int64_t offset, int64_t count) {
	// Return if no bytes to write
	if (count <= 0) {
		return;
	}
	
	// Check for invalid arguments
	if (count >= MAX_FILE_DATA_LENGTH || offset < 0 ||
	   	offset >= MAX_FILE_DATA_LENGTH) {
		perror("write_null_byte: Invalid arguments");
		exit(1);
	}
	
	// Write null bytes to file
	memset(f + offset, '\0', count);
}

// Write count null bytes to a file descriptor at offset (used for testcases)
// Assumes valid arguments
void pwrite_null_byte(int fd, int64_t count, int64_t offset) {
	for (int64_t i = 0; i < count; i++) {
		pwrite(fd, "", sizeof(char), offset + i);
	}
}


