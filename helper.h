#ifndef HELPER_H
#define HELPER_H

#include "structs.h"

// MACROS

// File length macros
#define FILE_DATA_LEN (fs->len[0])
#define DIR_TABLE_LEN (fs->len[1])
#define HASH_DATA_LEN (fs->len[2])

// Parent and children index macros
#define p_index(x) ((((x)%2)==1)?((x)/2):(((x)/2)-1))
#define lc_index(x) ((2*(x))+1)
#define rc_index(x) ((2*(x))+2)

// TODO: CHECK FOR OFFSET AND LENGTH THAT PASSED VALUES ARE PROPERLY CASTED/RIGHT TYPE
// file_t offset and length update macros
#define update_file_offset(x,y) (((y)->offset)=(x))
#define update_file_length(x,y) (((y)->length)=(x))

// dir_table name and length update macros
#define update_dir_name(file,fs) \
	(memcpy((fs)->dir + (file)->index * META_LEN, \
	(file)->name, strlen((file)->name) + 1))
#define update_dir_length(file,fs) \
	(memcpy(fs->dir + file->index * META_LEN + NAME_LEN + OFFSET_LEN, \
	&file->length, sizeof(uint32_t)))

// Macros for locking and unlocking synchronisation variables
#define LOCK(x) pthread_mutex_lock(x)
#define UNLOCK(x) pthread_mutex_unlock(x)

// Macro for suppressing unused variable warnings
#define UNUSED(x) ((void)(x))

// Safe malloc helper
void* salloc(size_t size);

// Safe calloc helper
void* scalloc(size_t size);

// Create new dynamically allocated file_t struct
file_t* file_init(char* name, uint64_t offset, uint32_t length, int32_t index);

// Free dynamically allocated file_t struct
void free_file(file_t* file);

// Update file_t struct name
void update_file_name(char* name, file_t* file);

// Update a file's offset field in dir_table
// Other dir_table update helpers are defined as macros in helper.h
void update_dir_offset(file_t* file, filesys_t* fs);

// Write a file to dir_table at the index within file_t
void write_dir_file(file_t* file, filesys_t* fs);

// Write count null bytes to a file at offset
void write_null_byte(uint8_t* f, int64_t offset, int64_t count);

// Write count null bytes to a file descriptor at offset (used for testcases)
// Assumes valid arguments
void pwrite_null_byte(int fd, int64_t count, int64_t offset);

#endif
