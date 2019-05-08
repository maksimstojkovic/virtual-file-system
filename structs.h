#ifndef STRUCTS_H
#define STRUCTS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define MAX_NUM_FILES 65536
#define NAME_LENGTH 64
#define OFFSET_LENGTH 4
#define META_LENGTH 72

typedef enum TYPE {OFFSET, NAME} TYPE;

struct filesys_t;
typedef pthread_mutex_t mutex_t;

typedef struct file_t {
	char name[NAME_LENGTH];
	
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
	int32_t size;
	int32_t capacity;
	TYPE type;
	struct filesys_t* fs;
	file_t** list;
} arr_t;

typedef struct filesys_t {
	int32_t nproc;
	int file;
	int dir;
	int hash;
	mutex_t mutex; // Filesystem mutex for blocking operations
	int64_t len[3]; // Length of file, dir, hash in bytes
	arr_t* o_list;
	arr_t* n_list;
	int64_t used; // Memory used in file_data
	int32_t index_len;
	char* index;
	// TODO: Include calloc'd buffers
} filesys_t;

#endif
