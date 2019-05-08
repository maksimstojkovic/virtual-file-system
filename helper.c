#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

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
	if (l_end()) {
		return (uint32_t)in;
	} else {
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
	update_file_name(name, f);
	update_file_offset(offset, f);
	update_file_length(length, f);
	f->index = index;
	f->o_index = -1;
	f->n_index = -1;
	return f;
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

// Update a file's name field in dir_table
void update_dir_name(file_t* file, filesys_t* fs) {
	pwrite(fs->dir, file->name, strlen(file->name) + 1,
		   file->index * META_LENGTH);
}

// Update a file's offset field in dir_table
void update_dir_offset(file_t* file, filesys_t* fs) {
	pwrite(fs->dir, &file->offset_le, sizeof(uint32_t),
		   file->index * META_LENGTH + NAME_LENGTH);
}

// Update a file's offset field in dir_table
void update_dir_length(file_t* file, filesys_t* fs) {
	pwrite(fs->dir, &file->length_le, sizeof(uint32_t),
		   file->index * META_LENGTH + NAME_LENGTH + OFFSET_LENGTH);
}

// Write a file to dir_table at the index within file_t
void write_dir_file(file_t* file, filesys_t* fs) {
	update_dir_name(file, fs);
	update_dir_offset(file, fs);
	update_dir_length(file, fs);
}

// Write null bytes to a file descriptor
// NOTE: DOES NOT CHECK FOR VALID OFFSET OR BYTE COUNT
void write_null_byte(int fd, int64_t count, int64_t offset) {
	for (int64_t i = 0; i < count; i++) {
		pwrite(fd, "", sizeof(char), offset + i);
	}
}


