#ifndef MYFILESYSTEM_H
#define MYFILESYSTEM_H
#include <sys/types.h>
#include <stdint.h>

/*
 * Helper Methods
 */
// Find offset in file_data for insertion (offset success)
// Files with zero length are assigned offset = max length of file_data
// (this value overflows to 0 when casting to uint32_t, but improves for more
//  efficient insertion into a sorted offset list)
uint64_t new_file_offset(size_t length, filesys_t* fs);

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
