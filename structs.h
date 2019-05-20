#ifndef STRUCTS_H
#define STRUCTS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

#define MAX_FILE_DATA_LENGTH (int64_t)4294967296
#define MAX_NUM_BLOCKS 16777216
#define BLOCK_LENGTH 256

#define MAX_NUM_FILES 65536
#define NAME_LENGTH 64
#define OFFSET_LENGTH 4
#define META_LENGTH 72

#define HASH_LENGTH 16

/*
	Filesystem Mutex Lock Order and Acquisition Hierarchy
	
	file_mutex
		used_mutex
	dir_mutex
		index_mutex
		o_list->list_mutex
		n_list->list_mutex
			f_mutex
	hash_mutex
*/
//fs_mutex - course

typedef enum TYPE {OFFSET, NAME} TYPE;

struct filesys_t;
typedef pthread_mutex_t mutex_t;

typedef struct file_t {
	char name[NAME_LENGTH];
	
	// Course mutex for file_t fields offset and length
	mutex_t f_mutex;
	
	// Unsigned long used for offset to handle zero size files
	// with a file_data length of 2^32 bytes
	uint64_t offset;
	uint32_t length;
	
	// Little endian format for writing to dir_table
	uint32_t offset_le;
	uint32_t length_le;
	
	int32_t index; // dir_table index
	int32_t o_index; // offset list index
	int32_t n_index; // name list index
} file_t;

typedef struct arr_t {
	mutex_t list_mutex;
	int32_t size;
	int32_t capacity;
	TYPE type;
	struct filesys_t* fs;
	file_t** list;
} arr_t;

typedef struct filesys_t {
	int32_t nproc;
	
	// Mutexes for each mmap'd file
	mutex_t file_mutex;
	mutex_t dir_mutex;
	mutex_t hash_mutex;
	
	int file_fd;
	int dir_fd;
	int hash_fd;
	uint8_t* file;
	uint8_t* dir;
	uint8_t* hash;
	
	int64_t len[3]; // Length of file_data, dir_table, hash_data in bytes
	arr_t* o_list;
	arr_t* n_list;
	
	mutex_t used_mutex; // Used variable mutex
	int64_t used; // Memory used in file_data
	
	mutex_t index_mutex; // dir_table indexing mutex
	int32_t index_len; // Number of entries in dir_table
	uint8_t* index; // Array for tracking available indices in dir_table
	
	int32_t blocks; // Number of blocks in file_data/entries in hash_data
	int32_t tree_len; // Number of entries in Merkle hash tree
	int32_t leaf_offset; // Offset to start of leaf nodes in hash tree
} filesys_t;

#endif
