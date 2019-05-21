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
file_t* new_file_t(char* name, uint64_t offset, uint32_t length, int32_t index);

// Free dynamically allocated file_t struct
void free_file_t(file_t* file);

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

// Write count null bytes to a file at offset
void write_null_byte(uint8_t* f, int64_t count, int64_t offset);

// Write count null bytes to a file descriptor at offset
// Used for testcases
// NOTE: DOES NOT CHECK FOR VALID OFFSET OR BYTE COUNT
void pwrite_null_byte(int fd, int64_t count, int64_t offset);

// Returns index of parent in hash array for node at index
int32_t p_index(int32_t index);

// Returns index of left child in hash array for node at index
int32_t lc_index(int32_t index);

// Returns index of right child in hash array for node at index
int32_t rc_index(int32_t index);

// Resizes files regardless of filesystem lock state
void resize_file_helper(file_t* file, size_t length, size_t copy, filesys_t* fs);

void repack_f(filesys_t* fs);
void hash_recurse(int32_t n_index, filesys_t* fs);
void compute_hash_block_f(size_t block_offset, filesys_t* fs);
void compute_hash_block_range(int64_t offset, int64_t length, filesys_t* fs);
int32_t verify_hash(size_t start_offset, size_t end_offset, filesys_t* fs);
int32_t verify_hash_range(int64_t f_offset, int64_t f_length, filesys_t* fs);

#endif
