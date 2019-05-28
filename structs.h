#ifndef STRUCTS_H
#define STRUCTS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

// Defined values

#define MAX_FILE_DATA_LENGTH (int64_t)4294967296
#define MAX_FILE_DATA_LENGTH_MIN_ONE 4294967295
#define BLOCK_LENGTH 256

#define MAX_NUM_FILES 65536
#define NAME_LENGTH 64
#define OFFSET_LENGTH 4
#define META_LENGTH 72

#define HASH_LENGTH 16
#define HASH_OFFSET_B 4
#define HASH_OFFSET_C 8
#define HASH_OFFSET_D 12

typedef enum TYPE {OFFSET, NAME} TYPE;

struct filesys_t;
typedef pthread_mutex_t mutex_t;
typedef pthread_cond_t cond_t;

typedef struct file_t {
	char name[NAME_LENGTH];	// File name
	uint64_t offset;		// File offset in file_data
	uint32_t length;		// File length in bytes
	int32_t index; 			// dir_table index
	int32_t o_index; 		// Offset array index
	int32_t n_index; 		// Name array index
} file_t;

typedef struct arr_t {
	int32_t size;			// Number of elements in array
	int32_t capacity;		// Total capacity of array
	TYPE type;				// Type that array is sorted by
	struct filesys_t* fs;	// Reference to filesystem
	file_t** list;			// Array elements
} arr_t;

typedef struct filesys_t {
	int32_t nproc;			// Number of processors available
	mutex_t lock;			// Filesystem lock
	int file_fd;			// file_data file descriptor
	int dir_fd;				// dir_table file descriptor
	int hash_fd;			// hash_data file descriptor
	uint8_t* file;			// Pointer to mmap of file_data
	uint8_t* dir;			// Pointer to mmap of dir_table
	uint8_t* hash;			// Pointer to mmap of hash_data
	int64_t len[3]; 		// Length of file_data, dir_table and hash_data
	arr_t* o_list;			// Array of files sorted by offset
	arr_t* n_list;			// Array of files sorted by name
	int64_t used;			// Memory used in file_data
	int32_t index_len;		// Maximum number of entries in dir_table
	uint8_t* index;			// Array of available indices in dir_table
	int32_t index_count;	// Number of entries in dir_table used
	int32_t tree_len;		// Number of entries in hash tree
	int32_t leaf_offset;	// Hash offset to start of leaf nodes in hash tree
} filesys_t;

#endif
