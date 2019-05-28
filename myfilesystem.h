#ifndef MYFILESYSTEM_H
#define MYFILESYSTEM_H

#include <sys/types.h>
#include <stdint.h>

#include "structs.h"

/*
 * Helper Methods
 */

int32_t new_file_index(filesys_t* fs);

uint64_t new_file_offset(size_t length, int64_t* hash_offset, filesys_t* fs);

int64_t resize_file_helper(file_t* file, size_t length, size_t copy, filesys_t* fs);

void repack_move(file_t* file, uint32_t new_offset, filesys_t* fs);

int64_t repack_helper(filesys_t* fs);

void hash_node(int32_t n_index, uint8_t* hash_cat, uint8_t* out, filesys_t* fs);

void compute_hash_block_helper(size_t block_offset, filesys_t* fs);

void compute_hash_block_range(int64_t offset, int64_t length, filesys_t* fs);

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
