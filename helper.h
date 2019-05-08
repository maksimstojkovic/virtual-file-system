#ifndef HELPER_H
#define HELPER_H

#include "structs.h"

// Safe malloc helper
void* salloc(size_t size);

// Safe calloc helper
void* scalloc(size_t size);

// Check if system is little endian
int l_end();

// Unsigned to little endian and vice versa
// (called when updating file_t offset/length, or reading dir_table)
uint32_t utole(uint64_t in);

// Create new dynamically allocated file_t struct
file_t* new_file_t(char* name, uint32_t offset, uint32_t length, int32_t index);

// Update file_t struct name
void update_file_name(char* name, file_t* file);

// Update file_t struct offset values
void update_file_offset(uint64_t offset, file_t* file);

// Update file_t struct lenth values
void update_file_length(uint32_t length, file_t* file);

// Update a file's name field in dir_table
void update_dir_name(file_t* file, filesys_t* fs);

// Update a file's offset field in dir_table
void update_dir_offset(file_t* file, filesys_t* fs);

// Update a file's offset field in dir_table
void update_dir_length(file_t* file, filesys_t* fs);

// Write a file to dir_table at the index within file_t
void write_dir_file(file_t* file, filesys_t* fs);

// Write null bytes to a file descriptor
// NOTE: DOES NOT CHECK FOR VALID OFFSET OR BYTE COUNT
void write_null_byte(int fd, int64_t count, int64_t offset);

#endif
