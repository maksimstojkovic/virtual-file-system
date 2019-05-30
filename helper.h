#ifndef HELPER_H
#define HELPER_H

#include "structs.h"

/*
 * Macros
 */

// Parent and children index macros
#define p_index(n_index) ((((n_index)%2)==1)?((n_index)/2):(((n_index)/2)-1))
#define lc_index(n_index) ((2*(n_index))+1)
#define rc_index(n_index) ((2*(n_index))+2)

// file_t offset and length update macros
#define update_file_offset(off,file) (((file)->offset)=(off))
#define update_file_length(len,file) (((file)->length)=(len))

// dir_table name and length update macros
#define update_dir_name(file,fs) \
	(memcpy((fs)->dir + (file)->index * META_LEN, \
	(file)->name, strlen((file)->name) + 1))
#define update_dir_length(file,fs) \
	(memcpy(fs->dir + file->index * META_LEN + NAME_LEN + OFFSET_LEN, \
	&file->length, sizeof(uint32_t)))

// Macros for locking and unlocking synchronisation variables
#define LOCK(mutex) pthread_mutex_lock(mutex)
#define UNLOCK(mutex) pthread_mutex_unlock(mutex)

// Macro for suppressing unused variable warnings
#define UNUSED(x) ((void)(x))

void* salloc(size_t size);

void* scalloc(size_t size);

file_t* file_init(char* name, uint64_t offset, uint32_t length, int32_t index);

void free_file(file_t* file);

void update_file_name(char* name, file_t* file);

void update_dir_offset(file_t* file, filesys_t* fs);

void write_dir_file(file_t* file, filesys_t* fs);

uint64_t write_null_byte(uint8_t* f, int64_t offset, int64_t count);

uint64_t pwrite_null_byte(int fd, int64_t count, int64_t offset);

#endif
