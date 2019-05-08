#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include "structs.h"
#include "helper.h"
#include "myfilesystem.h"

// ONLY INCLUDE SOURCE (.C) FILES HERE!
#include "arr.c"
#include "helper.c"


void * init_fs(char * f1, char * f2, char * f3, int n_processors) {
    filesys_t* fs = salloc(sizeof(*fs));

	// Initialise main filesystem mutex
	if (pthread_mutex_init(&fs->mutex, NULL) != 0) {
		perror("init_fs: Failed to initialise mutex");
		exit(1);
	}
	
	// Check if files exist
	fs->file = open(f1, O_RDWR);
	fs->dir = open(f2, O_RDWR);
	fs->hash = open(f3, O_RDWR);
	if (fs->file < 0 || fs->dir < 0 || fs->hash < 0) {
		perror("init_fs: Failed open calls");
		exit(1);
	}

	// Store file lengths in filesystem
	fs->len[0] = lseek(fs->file, 0, SEEK_END);
	fs->len[1] = lseek(fs->dir, 0, SEEK_END);
	fs->len[2] = lseek(fs->hash, 0, SEEK_END);

	// Initialise remaining filesystem variables
	fs->nproc = n_processors;
	fs->index_len = fs->len[1] / META_LENGTH;
	fs->index = scalloc(sizeof(*fs->index) * fs->index_len);
	fs->o_list = arr_init(fs->index_len, OFFSET, fs);
	fs->n_list = arr_init(fs->index_len, NAME, fs);
	fs->used = 0;
	
	// Read through dir_table for existing files
	char name[NAME_LENGTH] = {0};
	uint32_t offset_le;
	uint32_t length_le;

	for (int32_t i = 0; i < fs->index_len; i++) {
		pread(fs->dir, &name, NAME_LENGTH-1, i * META_LENGTH);
		
		// Filenames that do not start with a null byte are valid
		if (name[0] != '\0') {
			pread(fs->dir, &offset_le, sizeof(uint32_t), i * META_LENGTH + NAME_LENGTH);
			pread(fs->dir, &length_le, sizeof(uint32_t), i * META_LENGTH + NAME_LENGTH + OFFSET_LENGTH);
			
			// Creating file_t and adding to sorted arrays
			file_t* f = new_file_t(name, utole(offset_le), utole(length_le), i);
			arr_insert_s(f, fs->o_list);
			arr_insert_s(f, fs->n_list);
			
			// printf("init_fs added \"%s\" o:%lu l:%u dir:%d o_i:%d n_i:%d\n",
			// 	   f->name, f->offset, f->length, f->index, f->o_index, f->n_index);
			
			// Updating filesystem variables
			fs->index[i] = 1;
			fs->used += f->length;
		}
	}

	// printf("init_fs f_len:%ld d_len:%ld h_len:%ld index_len:%d o_size:%d n_size:%d\n",
	// 	   fs->len[0], fs->len[1], fs->len[2], fs->index_len, fs->o_list->size, fs->n_list->size);
	// arr_print(fs->o_list);
	// arr_print(fs->n_list);
	
	return fs;
}

void close_fs(void * helper) {
	filesys_t* fs = (filesys_t*)helper;
	
	// Close files
	close(fs->file);
	close(fs->dir);
	close(fs->hash);
	
	// Free heap memory allocated
	free_arr(fs->o_list);
	free_arr(fs->n_list);
	free(fs->index);
	free(fs);
}

// Finds first empty index in dir_table (index success)
static int32_t new_file_index(filesys_t* fs) {
	if (fs == NULL) {
		perror("new_file_index: NULL filesystem");
		exit(1);
	}
	
	int32_t len = fs->index_len;
	char* index = fs->index;
	
	// Iterate over index array for space in dir_table
	for (int32_t i = 0; i < len; i++) {
		if (index[i] == 0) {
			return i;
		}
	}

	perror("new_file_index: Filesystem full, no index available");
	exit(1);
}

// Find offset in file_data for insertion (offset success)
// Files with zero length are assigned offset = max length of file_data
// (this value overflows to 0 when casting to uint32_t, but improves for more
//  efficient insertion into a sorted offset list)
uint64_t new_file_offset(size_t length, filesys_t* fs) {
	if (length < 0 || fs == NULL) {
		perror("new_file_offset: Invalid arguments");
		exit(1);
	}
	
	// Assign zero size files with offset = max length of file_data
	if (length == 0) {
		return (uint64_t)MAX_NUM_BLOCKS * BLOCK_LENGTH;
	}

	file_t** o_list = fs->o_list->list;
	int32_t size = fs->o_list->size;
	
	// Cases for no elements in offset list or first element not at offset 0
	if (size <= 0 || o_list[0]->offset >= length) {
		return 0;
	}
	
	for (int32_t i = 0; i < size; i++) {
		if ((i < size - 1 && o_list[i+1]->offset - (o_list[i]->offset + o_list[i]->length) >= length) ||
			(i == size - 1 && fs->len[0] - (o_list[i]->offset + o_list[i]->length) >= length)) {
			return o_list[i]->offset + o_list[i]->length;
		}
	}
	
	// Repack if no large enough contiguous space found,
	// and check space at end of file_data
	repack(fs);
	if (fs->len[0] - (o_list[size-1]->offset + o_list[size-1]->length) >= length) {
		return o_list[size-1]->offset + o_list[size-1]->length;
	}

	perror("new_file_offset: Unable to find valid offset");
	exit(1);
}

int create_file(char * filename, size_t length, void * helper) {
    filesys_t* fs = (filesys_t*)helper;
	pthread_mutex_lock(&fs->mutex);

	// Return 1 if file already exists
	file_t temp;
	update_file_name(filename, &temp);
	if (arr_get_s(&temp, fs->n_list) != NULL) {
		pthread_mutex_unlock(&fs->mutex);
		return 1;
	}
	
	// Return 2 if insufficient space in filesystem
	if (fs->used + length > fs->len[0]) {
		pthread_mutex_unlock(&fs->mutex);
		return 2;
	}

	// Find available index in dir_table and space (suitable offset) in file_data
	int32_t index = new_file_index(fs);
	uint64_t offset = new_file_offset(length, fs);

	// Create new file_t struct and insert into both sorted lists
	file_t* f = new_file_t(filename, offset, length, index);
	arr_insert_s(f, fs->o_list);
	arr_insert_s(f, fs->n_list);

	// Write file metadata to dir_table
	write_dir_file(f, fs);

	// Write null bytes to file_data if not zero size file
	write_null_byte(fs->file, length, f->offset);

	// Update filesystem variables
	fs->used += length;
	fs->index[index] = 1;
	
	// printf("create_file added \"%s\" o:%lu l:%u dir:%d o_i:%d n_i:%d\n",
	// 	   f->name, f->offset, f->length, f->index, f->o_index, f->n_index);
	
	pthread_mutex_unlock(&fs->mutex);
	return 0;
}

// Force resize_file
void resize_file_f(file_t* file, size_t length, size_t copy, filesys_t* fs) {
	int64_t old_length = file->length;
	
	// If length increases, temporarily store data in a buffer before finding
	// a new suitable offset in file_data
	if (length > old_length) {
		file_t** list = fs->o_list->list;
		int32_t size = fs->o_list->size;
		
		// Check if current offset has sufficient space from the next file
		// or from the end of file_data
		if ((file->o_index < size - 1 && 
			 list[file->o_index+1]->offset - file->offset < length) ||
		    (file->o_index == size - 1 && 
			 fs->len[0] - file->offset < length)) {
			
			// Store copy bytes of data from file_data into a buffer
			// Only copy bytes are necessary as remaining bytes may be
			// overwritten by write_file
			char* temp = salloc(sizeof(*temp) * copy);
			pread(fs->file, temp, copy, file->offset);
			
			// Remove the file_t* from the sorted offset list
			arr_remove(file->o_index, fs->o_list);
			
			// Find a new suitable offset, ignoring the current position
			// of file's data and write to that offset
			uint32_t new_offset = new_file_offset(length, fs);
			pwrite(fs->file, temp, copy, new_offset);
			free(temp);
			
			// Update file_t and dir_table entry's offset
			update_file_offset(new_offset, file);
			update_dir_offset(file, fs);
			
			// Reinsert file into sorted offset list
			arr_insert_s(file, fs->o_list);
		}
	}
	
	// Update file_t and dir_table entry's length if length changed
	if (length != old_length) {
		update_file_length(length, file);
		update_dir_length(file, fs);
	}
	
	// Update filesystem variables
	fs->used += length - old_length;
}

int resize_file(char * filename, size_t length, void * helper) {
    filesys_t* fs = (filesys_t*)helper;
	pthread_mutex_lock(&fs->mutex);
	
	// printf("resize_file resizing \"%s\" to %lu bytes\n", filename, length);
	
	// Return 1 if file does not exist
	file_t temp;
	update_file_name(filename, &temp);
	file_t* f = arr_get_s(&temp, fs->n_list);
	if (f == NULL) {
		pthread_mutex_unlock(&fs->mutex);
		return 1;
	}
	
	// Return 2 if insufficient space in filesystem
	if (fs->used + (length - f->length) > fs->len[0]) {
		pthread_mutex_unlock(&fs->mutex);
		return 2;
	}
	
	// Resize the file and append null bytes as required
	int64_t old_length = f->length;
	resize_file_f(f, length, old_length, fs);
	write_null_byte(fs->file, length - old_length, f->offset + old_length);
	
	// printf("resize_file resized \"%s\" o:%lu l:%u dir:%d o_i:%d n_i:%d\n",
	// 	   f->name, f->offset, f->length, f->index, f->o_index, f->n_index);

	pthread_mutex_unlock(&fs->mutex);
	return 0;
};

// Moves the file specified to new_offset in file_data
static void repack_move(file_t* file, uint32_t new_offset, filesys_t* fs) {
	char* buff = salloc(file->length);
	pread(fs->file, buff, file->length, file->offset);
	pwrite(fs->file, buff, file->length, new_offset);
	update_file_offset(new_offset, file);
	update_dir_offset(file, fs);
	free(buff);
}

// Force repack regardless of filesystem mutex
// Does not affect zero size files
void repack_f(filesys_t* fs) {
	file_t** o_list = fs->o_list->list;
	int32_t size = fs->o_list->size;
	
	// Move first file to offset 0
	if (o_list[0]->offset != 0 && o_list[0]->length != 0) {
		repack_move(o_list[0], 0, fs);
	}
	
	// Iterate over sorted offset list and move data when necessary
	for (int32_t i = 1; i < size; i++) {
		if (o_list[i]->offset > o_list[i-1]->offset + o_list[i-1]->length &&
			o_list[i]->length != 0) {
			repack_move(o_list[i], o_list[i-1]->offset + o_list[i-1]->length, fs);
		}
	}
}

void repack(void * helper) {
    filesys_t* fs = (filesys_t*)helper;
	pthread_mutex_lock(&fs->mutex);
	
	repack_f(fs);
	
	pthread_mutex_unlock(&fs->mutex);
}

int delete_file(char * filename, void * helper) {
    filesys_t* fs = (filesys_t*)helper;
	pthread_mutex_lock(&fs->mutex);
	
	// Return 1 if file does not exist
	file_t temp;
	update_file_name(filename, &temp);
	file_t* f = arr_get_s(&temp, fs->n_list);
	if (f == NULL) {
		pthread_mutex_unlock(&fs->mutex);
		return 1;
	}
	
	// Write null byte in file's name field in dir_table
	write_null_byte(fs->dir, 1, f->index * META_LENGTH);
	
	// Update filesystem variables
	fs->used -= f->length;
	fs->index[f->index] = 0;

	// Remove from arrays using indices to handle zero size files
	arr_remove(f->o_index, fs->o_list);
	arr_remove(f->n_index, fs->n_list);
	
	// printf("delete_file removed \"%s\" o:%lu l:%u dir:%d o_i:%d n_i:%d\n",
	// 	   f->name, f->offset, f->length, f->index, f->o_index, f->n_index);
	
	free(f);
	
	pthread_mutex_unlock(&fs->mutex);
	return 0;
}

int rename_file(char * oldname, char * newname, void * helper) {
    filesys_t* fs = (filesys_t*)helper;
	pthread_mutex_lock(&fs->mutex);
	
	// Return 1 if oldname file does not exist or newname file does exist
	file_t temp_old;
	file_t temp_new;
	update_file_name(oldname, &temp_old);
	update_file_name(newname, &temp_new);
	file_t* f = arr_get_s(&temp_old, fs->n_list);
	if ((f == NULL || arr_get_s(&temp_new, fs->n_list) != NULL)) {
		pthread_mutex_unlock(&fs->mutex);
		return 1;
	}
		
	// Update file_t struct and dir_table entry
	update_file_name(newname, f);
	update_dir_name(f, fs);
	
	// printf("rename_file renamed \"%s\" to \"%s\" o:%lu l:%u dir:%d o_i:%d n_i:%d\n",
	// 	   oldname, f->name, f->offset, f->length, f->index, f->o_index, f->n_index);
	
	pthread_mutex_unlock(&fs->mutex);
	return 0;
}

int read_file(char * filename, size_t offset, size_t count, void * buf, void * helper) {
	filesys_t* fs = (filesys_t*)helper;
	pthread_mutex_lock(&fs->mutex);
	
	// Return 1 if file does not exist
	file_t temp;
	update_file_name(filename, &temp);
	file_t* f = arr_get_s(&temp, fs->n_list);
	if (f == NULL) {
		pthread_mutex_unlock(&fs->mutex);
		return 1;
	}
	
	// Return 2 if invalid offset and count for given file
	if (offset + count > f->length) {
		pthread_mutex_unlock(&fs->mutex);
		return 2;
	}
	
	// Read count bytes into buf at offset bytes from the start of f
	pread(fs->file, buf, count, f->offset + offset);
	
	// printf("read_file read \"%s\" %lu bytes at offset %lu o:%lu l:%u dir:%d o_i:%d n_i:%d\n",
	// 	   f->name, count, offset, f->offset, f->length, f->index, f->o_index, f->n_index);
	
	pthread_mutex_unlock(&fs->mutex);
	return 0;
}

int write_file(char * filename, size_t offset, size_t count, void * buf, void * helper) {
    filesys_t* fs = (filesys_t*)helper;
	pthread_mutex_lock(&fs->mutex);
	
	// Return 1 if file does not exist
	file_t temp;
	update_file_name(filename, &temp);
	file_t* f = arr_get_s(&temp, fs->n_list);
	if (f == NULL) {
		pthread_mutex_unlock(&fs->mutex);
		return 1;
	}
	
	// Return 2 if offset is greater than the current file length
	if (offset > f->length) {
		pthread_mutex_unlock(&fs->mutex);
		return 2;
	}
	
	// Return 3 if insufficient space in filesystem
	if (fs->used + (offset + count - f->length) > fs->len[0]) {
		pthread_mutex_unlock(&fs->mutex);
		return 3;
	}
	
	// Only resize if required to write count bytes at offset
	// from the start of the file
	if (offset + count > f->length) {
		resize_file_f(f, offset + count, offset, fs);
	}
	
	// Write count bytes from buf to file_data
	// (f->offset will be updated if resize occured)
	pwrite(fs->file, buf, count, f->offset + offset);
	
	// printf("write_file wrote \"%s\" %lu bytes at offset %lu o:%lu l:%u dir:%d o_i:%d n_i:%d\n",
	// 	   f->name, count, offset, f->offset, f->length, f->index, f->o_index, f->n_index);
	
	pthread_mutex_unlock(&fs->mutex);
	return 0;
}

ssize_t file_size(char * filename, void * helper) {
    filesys_t* fs = (filesys_t*)helper;
	pthread_mutex_lock(&fs->mutex);
	
	// Return -1 if file does not exist
	file_t temp;
	update_file_name(filename, &temp);
	file_t* f = arr_get_s(&temp, fs->n_list);
	if (f == NULL) {
		pthread_mutex_unlock(&fs->mutex);
		return -1;
	}
	
	// printf("file_size ret %u \"%s\" o:%lu l:%u dir:%d o_i:%d n_i:%d\n",
	// 	   f->length, f->name, f->offset, f->length, f->index, f->o_index, f->n_index);
	
	// Return length of file
	pthread_mutex_unlock(&fs->mutex);
	return f->length;
}

void fletcher(uint8_t * buf, size_t length, uint8_t * output) {
    return;
}

void compute_hash_tree(void * helper) {
    return;
}

void compute_hash_block(size_t block_offset, void * helper) {
    return;
}
