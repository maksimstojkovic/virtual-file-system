#ifndef STRUCTS_H
#define STRUCTS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

/*
 * Defined values
 */

#define MAX_FILE_DATA_LEN ((int64_t)4294967296)
#define MAX_FILE_DATA_LEN_MIN_ONE (4294967295)
#define BLOCK_LEN (256)

#define MAX_NUM_FILES (65536)
#define NAME_LEN (64)
#define OFFSET_LEN (4)
#define META_LEN (72)

#define HASH_LEN (16)
#define HASH_OFFSET_B (4)
#define HASH_OFFSET_C (8)
#define HASH_OFFSET_D (12)

typedef enum TYPE {OFFSET, NAME} TYPE;

struct filesys_t;
typedef pthread_mutex_t mutex_t;
typedef pthread_barrier_t barrier_t;
typedef pthread_cond_t cond_t;

typedef struct warg_t {
	int id;
	int32_t start_index;
	int32_t nodes_in_level;
	struct filesys_t* fs;
} warg_t;

typedef struct file_t {
	char name[NAME_LEN];	// File name
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
	int nproc;				// Number of processors available
	int nthreads;			// Number of threads for workers
	pthread_t* tid;			// Array of thread IDs
	int finalise_index;		// Index for finalising hashes in hash_data
	int worker_count;		// Monitor for workers to start
	int active_count;		// Number of workers active
	int exit_count;			// Monitor for workers to exit
	warg_t* wargs;			// Worker arguments for threads
	mutex_t hash_mutex;		// Mutex to checking worker and exit monitors
	barrier_t hash_barrier;	// Barrier to wait for workers
	cond_t hash_cond;		// Cond to signal workers to start
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
	int32_t index_count;	// Number of entries in dir_table used
	uint8_t* index;			// Array of available indices in dir_table
	int32_t tree_len;		// Number of entries in hash tree
	int32_t leaf_offset;	// Hash offset to start of leaf nodes in hash tree
	int32_t leaf_count;		// Number of leaves in hash tree
} filesys_t;

#endif
