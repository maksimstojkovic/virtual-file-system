#ifndef STRUCTS_H
#define STRUCTS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>

#define MAX_FILE_DATA_LENGTH (int64_t)4294967296
#define MAX_NUM_BLOCKS 16777216
#define BLOCK_LENGTH 256

#define MAX_NUM_FILES 65536
#define NAME_LENGTH 64
#define OFFSET_LENGTH 4
#define META_LENGTH 72

#define HASH_LENGTH 16

#define RW_LIMIT 10

#define LOCK(x) pthread_mutex_lock(x)
#define UNLOCK(x) pthread_mutex_unlock(x)
#define SEM_WAIT(x) sem_wait(x)
#define SEM_POST(x) sem_post(x)
#define COND_WAIT(x,y) pthread_cond_wait(x,y)
#define COND_SIGNAL(x) pthread_cond_signal(x)
#define COND_BROADCAST(x) pthread_cond_broadcast(x)

/*
	Filesystem Mutex Lock Order and Acquisition Hierarchy
	
	file_mutex
		used_mutex
		rw_sem
		fw_rw_mutex
			rw_mutex + rw_cond
	dir_mutex
		index_mutex
		o_list->list_mutex
		n_list->list_mutex
			f_mutex
	hash_mutex
*/

typedef enum TYPE {OFFSET, NAME} TYPE;

struct filesys_t;
typedef pthread_mutex_t mutex_t;
typedef pthread_cond_t cond_t;

typedef struct range_t {
	int64_t start; // start < 0 indicates range not in use
	int64_t end;
} range_t;

typedef struct file_t {
	char name[NAME_LENGTH];
	
	// Syncronisation variable
	mutex_t f_mutex; // Locks offset and length
	
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
	
	sem_t rw_sem; // Limit on parallel read/write, set by RW_LIMIT
	mutex_t fs_rw_mutex; // Mutex used by full blocking functions to stop new parallel operations
	mutex_t rw_mutex; // Mutex for r/w ranges
	cond_t rw_cond; // Cond for accessing the r/w range list
	range_t* rw_range; // Ranges used by r/w operation
	
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
