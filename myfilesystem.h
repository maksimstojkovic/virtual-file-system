#ifndef MYFILESYSTEM_H
#define MYFILESYSTEM_H

#include <sys/types.h>
#include <stdint.h>

#include "structs.h"

/*
 * Helper Methods
 */

// Returns first empty index in dir_table
int32_t new_file_index(filesys_t* fs);

// Return offset in file_data for insertion
uint64_t new_file_offset(size_t length, int64_t* hash_offset, filesys_t* fs);

// Resizes files regardless of filesystem lock state, returns first repack offset
// Returns index of first byte repacked if repack occurred
int64_t resize_file_helper(file_t* file, size_t length, size_t copy, filesys_t* fs);

// Moves the file specified to new_offset in file_data
void repack_move(file_t* file, uint32_t new_offset, filesys_t* fs);

// Repack files regardless of filesystem lock state, returns first repack offset
// Does not affect zero size files
// Returns index of first byte repacked
int64_t repack_helper(filesys_t* fs);

// Writes the hash for the node at n_index to out buffer
void hash_node(int32_t n_index, uint8_t* hash_cat, uint8_t* out, filesys_t* fs);

// Update hashes for block at offset specified, regardless of filesystem lock state
void compute_hash_block_helper(size_t block_offset, filesys_t* fs);

// Update hashes for modified blocks in range specified
void compute_hash_block_range(int64_t offset, int64_t length, filesys_t* fs);

// Compare hashes for blocks in the range specified
// Returns 0 on success, 1 on failure
int32_t verify_hash_range(int64_t offset, int64_t length, filesys_t* fs);

/*
 * Main Methods
 */

void * init_fs(char * f1, char * f2, char * f3, int n_processors);

void close_fs(void * helper);

int create_file(char * filename, size_t length, void * helper);

int resize_file(char * filename, size_t length, void * helper);

void repack(void * helper);

int delete_file(char * filename, void * helper);

int rename_file(char * oldname, char * newname, void * helper);

int read_file(char * filename, size_t offset, size_t count, void * buf, void * helper);

int write_file(char * filename, size_t offset, size_t count, void * buf, void * helper);

ssize_t file_size(char * filename, void * helper);

void fletcher(uint8_t * buf, size_t length, uint8_t * output);

void compute_hash_tree(void * helper);

void compute_hash_block(size_t block_offset, void * helper);

#endif
