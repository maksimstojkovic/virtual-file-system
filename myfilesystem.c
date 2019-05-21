#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "structs.h"
#include "helper.h"
#include "synclist.h"
#include "arr.h"
#include "myfilesystem.h"

// ONLY INCLUDE SOURCE (.C) FILES HERE!
// #include "arr.c"
// #include "helper.c"

// MOVE TO HEADER FILE
void repack_f(filesys_t* fs);
void compute_hash_block_f(size_t block_offset, filesys_t* fs);
void compute_hash_block_range(int64_t offset, int64_t length, filesys_t* fs);

// TODO: REMOVE ANY ARRAYS INITIALISED WITH VARIABLE VALUES AS LENGTH
// TODO: TRY REPLACING POST-FIX INCREMENT (++) WITH PREFIX
// TODO: TRY REPLACING MODULUS OPERATOR WITH rem=a-b*(a/b), using intermediate value (a/b)

// POSSIBLY TRY USING MS_ASYNC IN MSYNC CALLS
// DOUBLE CHECK MSYNC POINTER AND LENGTH


void * init_fs(char * f1, char * f2, char * f3, int n_processors) {
    filesys_t* fs = salloc(sizeof(*fs));

	// Initialise main filesystem mutex
	if (pthread_mutex_init(&fs->file_mutex, NULL) ||
	   	pthread_mutex_init(&fs->dir_mutex, NULL) ||
	   	pthread_mutex_init(&fs->hash_mutex, NULL) ||
	   	pthread_mutex_init(&fs->used_mutex, NULL) ||
	   	pthread_mutex_init(&fs->index_mutex, NULL)) {
		perror("init_fs: Failed to initialise mutexes");
		exit(1);
	}
	
	// Initialise synchronisation variables for parallel r/w
	fs->rw_range = salloc(sizeof(*fs->rw_range) * RW_LIMIT);
	if (sem_init(&fs->rw_sem, 0, RW_LIMIT) ||
	   	pthread_mutex_init(&fs->rw_mutex, NULL) ||
		pthread_mutex_init(&fs->fs_rw_mutex, NULL) ||
	   	pthread_cond_init(&fs->rw_cond, NULL)) {
		perror("init_fs: Failed to initialise r/w sync variables");
		exit(1);
	}
	for (int32_t i = 0; i < RW_LIMIT; i++) {
		fs->rw_range[i].start = -1;
	}
	
	// Check if files exist
	fs->file_fd = open(f1, O_RDWR);
	fs->dir_fd = open(f2, O_RDWR);
	fs->hash_fd = open(f3, O_RDWR);
	if (fs->file_fd < 0 || fs->dir_fd < 0 || fs->hash_fd < 0) {
		perror("init_fs: Failed open calls");
		exit(1);
	}

	// Store file lengths in filesystem
	struct stat s[3];
	if (fstat(fs->file_fd, &s[0]) != 0 ||
	   	fstat(fs->dir_fd, &s[1]) != 0 ||
	   	fstat(fs->hash_fd, &s[2]) != 0) {
		perror("init_fs: Failed to get file length");
		exit(1);
	}
	fs->len[0] = s[0].st_size;
	fs->len[1] = s[1].st_size;
	fs->len[2] = s[2].st_size;
	
	// Map files to memory using mmap
	fs->file = mmap(NULL, s[0].st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fs->file_fd, 0);
	fs->dir = mmap(NULL, s[1].st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fs->dir_fd, 0);
	fs->hash = mmap(NULL, s[2].st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fs->hash_fd, 0);
	if (fs->file == MAP_FAILED || fs->dir == MAP_FAILED || fs->hash == MAP_FAILED ||
	   	fs->file == NULL || fs->dir == NULL || fs->hash == NULL) {
		perror("init_fs: Failed to map files to memory");
		exit(1);
	}

	// Initialise remaining filesystem variables
	fs->nproc = n_processors;
	fs->index_len = fs->len[1] / META_LENGTH;
	fs->index = scalloc(sizeof(*fs->index) * fs->index_len);
	fs->o_list = arr_init(fs->index_len, OFFSET, fs);
	fs->n_list = arr_init(fs->index_len, NAME, fs);
	fs->used = 0;
	
	// Initialising Merkle hash tree variables
	fs->blocks = fs->len[0] / BLOCK_LENGTH;
	int32_t leaves = 1;
	while (leaves < fs->blocks) {
		// Double number of leaves if insufficient for number of blocks
		leaves *= 2;
	}
	fs->tree_len = (2 * leaves) - 1;
	fs->leaf_offset = fs->blocks - 1;
	
	// Read through dir_table for existing files
	char name[NAME_LENGTH] = {0};
	uint32_t offset_le;
	uint32_t length_le;
	uint64_t offset;
	uint32_t length;

	for (int32_t i = 0; i < fs->index_len; i++) {
		memcpy(&name, fs->dir + i * META_LENGTH, NAME_LENGTH-1);
		
		// Filenames that do not start with a null byte are valid
		if (name[0] != '\0') {
			memcpy(&offset_le, fs->dir + i * META_LENGTH + NAME_LENGTH, sizeof(uint32_t));
			memcpy(&length_le, fs->dir + i * META_LENGTH + NAME_LENGTH + OFFSET_LENGTH, sizeof(uint32_t));
			
			// Assign zero size files with offset = max length of file_data
			length = utole(length_le);
			if (length_le == 0) {
				offset = (uint64_t)MAX_NUM_BLOCKS * BLOCK_LENGTH;
			} else {
				offset = utole(offset_le);
			}
			
			// Creating file_t and adding to sorted arrays
			file_t* f = new_file_t(name, offset, length, i);
			arr_insert_s(f, fs->o_list);
			arr_insert_s(f, fs->n_list);
			
			printf("init_fs added \"%s\" o:%lu l:%u dir:%d o_i:%d n_i:%d\n",
				   f->name, f->offset, f->length, f->index, f->o_index, f->n_index);
			
			// Updating filesystem variables
			fs->index[i] = 1;
			fs->used += f->length;
		}
	}

	printf("init_fs f_len:%ld d_len:%ld h_len:%ld index_len:%d o_size:%d n_size:%d\n",
		   fs->len[0], fs->len[1], fs->len[2], fs->index_len, fs->o_list->size, fs->n_list->size);
	arr_print(fs->o_list);
	arr_print(fs->n_list);
	
	return fs;
}

void close_fs(void * helper) {
	filesys_t* fs = (filesys_t*)helper;
	
	// Sync all files and unmap memory
	msync(fs->file, fs->len[0], MS_SYNC);
	msync(fs->dir, fs->len[1], MS_SYNC);
	msync(fs->hash, fs->len[2], MS_SYNC);
	munmap(fs->file, fs->len[0]);
	munmap(fs->dir, fs->len[1]);
	munmap(fs->hash, fs->len[2]);
	
	// Close files
	close(fs->file_fd);
	close(fs->dir_fd);
	close(fs->hash_fd);
	
	// Destroy filesystem synchronisation variables
	pthread_mutex_destroy(&fs->file_mutex);
	pthread_mutex_destroy(&fs->dir_mutex);
	pthread_mutex_destroy(&fs->hash_mutex);
	pthread_mutex_destroy(&fs->used_mutex);
	pthread_mutex_destroy(&fs->index_mutex);
	pthread_mutex_destroy(&fs->fs_rw_mutex);
	pthread_mutex_destroy(&fs->rw_mutex);
	pthread_cond_destroy(&fs->rw_cond);
	sem_destroy(&fs->rw_sem);
	
	// Free heap memory allocated
	free_arr(fs->o_list);
	free_arr(fs->n_list);
	free(fs->index);
	free(fs->rw_range);
	free(fs);
}

// Finds first empty index in dir_table (index success)
static int32_t new_file_index(filesys_t* fs) {
	if (fs == NULL) {
		perror("new_file_index: NULL filesystem");
		exit(1);
	}
	
	int32_t len = fs->index_len;
	uint8_t* index = fs->index;
	
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
// (this value is used for more efficient insertion, but is set as 0 in dir_table)
uint64_t new_file_offset(size_t length, filesys_t* fs) {
	if (length < 0 || fs == NULL) {
		perror("new_file_offset: Invalid arguments");
		exit(1);
	}
	
	// Assign zero size files with offset = max length of file_data
	if (length == 0) {
		return MAX_FILE_DATA_LENGTH;
	}
	
	file_t** o_list = fs->o_list->list;
	int32_t size = fs->o_list->size;
	
	// Cases for no elements in offset list or first element not at offset 0
	if (size <= 0 || o_list[0]->offset >= length) {
		return 0;
	}
	
	// Check space between elements in offset list
	for (int32_t i = 0; i < size - 1; i++) {
		if (o_list[i+1]->offset - (o_list[i]->offset + o_list[i]->length) >= length) {
			return o_list[i]->offset + o_list[i]->length;
		}
	}
	
	// Check space between last element in offset list and end of file_data
	if (fs->len[0] - (o_list[size-1]->offset + o_list[size-1]->length) >= length) {
		return o_list[size-1]->offset + o_list[size-1]->length;
	}
	
	// Repack and sync hash_data if no large enough contiguous space found,
	// and check space at end of file_data
	repack_f(fs);
	msync(fs->hash, fs->len[2], MS_SYNC);
	if (fs->len[0] - (o_list[size-1]->offset + o_list[size-1]->length) >= length) {
		return o_list[size-1]->offset + o_list[size-1]->length;
	}

	perror("new_file_offset: Unable to find valid offset");
	exit(1);
}

int create_file(char * filename, size_t length, void * helper) {
    filesys_t* fs = (filesys_t*)helper;
	LOCK(&fs->file_mutex);
	LOCK(&fs->used_mutex);
	wait_all_ranges(fs);
	LOCK(&fs->dir_mutex);
	LOCK(&fs->index_mutex);
	LOCK(&fs->o_list->list_mutex);
	LOCK(&fs->n_list->list_mutex);
	
	printf("create_file creating \"%s\" of %lu bytes\n", filename, length);

	// Return 1 if file already exists
	file_t temp;
	update_file_name(filename, &temp);
	if (arr_get_s(&temp, fs->n_list) != NULL) {
		UNLOCK(&fs->file_mutex);
		UNLOCK(&fs->used_mutex);
		UNLOCK(&fs->dir_mutex);
		UNLOCK(&fs->index_mutex);
		UNLOCK(&fs->o_list->list_mutex);
		UNLOCK(&fs->n_list->list_mutex);
		return 1;
	}
	
	// Return 2 if insufficient space in filesystem
	if (fs->used + length > fs->len[0]) {
		UNLOCK(&fs->file_mutex);
		UNLOCK(&fs->used_mutex);
		UNLOCK(&fs->dir_mutex);
		UNLOCK(&fs->index_mutex);
		UNLOCK(&fs->o_list->list_mutex);
		UNLOCK(&fs->n_list->list_mutex);
		return 2;
	}
	LOCK(&fs->hash_mutex);
	
	// Find available index in dir_table and space (suitable offset) in file_data
	int32_t index = new_file_index(fs);
	uint64_t offset = new_file_offset(length, fs);

	// Create new file_t struct and insert into both sorted lists
	file_t* f = new_file_t(filename, offset, length, index);
	arr_insert_s(f, fs->o_list);
	arr_insert_s(f, fs->n_list);

	// Write file metadata to dir_table, update index array and sync
	write_dir_file(f, fs);
	fs->index[index] = 1;
	msync(fs->dir, fs->len[1], MS_SYNC);
	UNLOCK(&fs->dir_mutex);
	UNLOCK(&fs->index_mutex);
	UNLOCK(&fs->o_list->list_mutex);
	UNLOCK(&fs->n_list->list_mutex);

	// Write null bytes to file_data (Zero size file do not write anything),
	// update file_data used and sync
	write_null_byte(fs->file, length, f->offset);
	fs->used += length;
	msync(fs->file, fs->len[0], MS_SYNC);
	UNLOCK(&fs->file_mutex);
	UNLOCK(&fs->used_mutex);
	
	// Update hash_data and sync (only for non-zero size files)
	if (length > 0) {
		compute_hash_block_range(offset, length, fs);
	}
	msync(fs->hash, fs->len[2], MS_SYNC);
	UNLOCK(&fs->hash_mutex);
	
	printf("create_file added \"%s\" o:%lu l:%u dir:%d o_i:%d n_i:%d\n",
		   f->name, f->offset, f->length, f->index, f->o_index, f->n_index);
	
	return 0;
}

// Force resize_file - uses file_data, used, dir_table, o_list, n_list, and file
// (file_data no sync, dir_table sync)
void resize_file_f(file_t* file, size_t length, size_t copy, filesys_t* fs) {
	int64_t old_length = file->length;
	
	// Reposition file if length increased
	if (length > old_length) {
		file_t** list = fs->o_list->list;
		int32_t size = fs->o_list->size;
		
		// Check if current offset has insufficient space from the next file
		// or from the end of file_data
		if ((file->o_index < size - 1 && list[file->o_index+1]->offset - file->offset < length) ||
		    (file->o_index == size - 1 && fs->len[0] - file->offset < length)) {
			
			// Store copy bytes of data from file_data into a buffer
			// Only copy bytes are necessary as remaining bytes may be overwritten by write_file
			char* temp = salloc(sizeof(*temp) * copy);
			memcpy(temp, fs->file + file->offset, copy);
			
			// Remove the file_t* from the sorted offset list
			arr_remove(file->o_index, fs->o_list);
			
			// Repack and write copied file to end of file_data
			// No sync performed as calling function may perform additonal writes
			repack_f(fs);
			memcpy(fs->file + fs->used - old_length, temp, copy);
			free(temp);
			
			// Update file_t and dir_table entry's offset
			update_file_offset(fs->used - old_length, file);
			update_dir_offset(file, fs);
			
			// Reinsert file into sorted offset list
			arr_insert_s(file, fs->o_list);
		}
	}
	
	// Update file_t and dir_table entry's length if length changed
	if (length != old_length) {
		// Update offset variables for zero size files
		if (length == 0) {
			update_file_offset(MAX_FILE_DATA_LENGTH, file);
		}
		
		update_file_length(length, file);
		update_dir_length(file, fs);
		
		// Sync dir_table
		msync(fs->dir, fs->len[1], MS_SYNC);
	}
	
	// Update filesystem variables
	fs->used += length - old_length;
}

int resize_file(char * filename, size_t length, void * helper) {
    filesys_t* fs = (filesys_t*)helper;
	LOCK(&fs->file_mutex);
	LOCK(&fs->used_mutex);
	wait_all_ranges(fs);
	LOCK(&fs->dir_mutex);
	LOCK(&fs->index_mutex);
	LOCK(&fs->o_list->list_mutex);
	LOCK(&fs->n_list->list_mutex);
	
	printf("resize_file resizing \"%s\" to %lu bytes\n", filename, length);
	
	// Return 1 if file does not exist
	file_t temp;
	update_file_name(filename, &temp);
	file_t* f = arr_get_s(&temp, fs->n_list);
	if (f == NULL) {
		UNLOCK(&fs->file_mutex);
		UNLOCK(&fs->used_mutex);
		UNLOCK(&fs->dir_mutex);
		UNLOCK(&fs->index_mutex);
		UNLOCK(&fs->o_list->list_mutex);
		UNLOCK(&fs->n_list->list_mutex);
		return 1;
	}
	LOCK(&f->f_mutex);
	
	// Return 2 if insufficient space in filesystem
	if (fs->used + (length - f->length) > fs->len[0]) {
		UNLOCK(&fs->file_mutex);
		UNLOCK(&fs->used_mutex);
		UNLOCK(&fs->dir_mutex);
		UNLOCK(&fs->index_mutex);
		UNLOCK(&fs->o_list->list_mutex);
		UNLOCK(&fs->n_list->list_mutex);
		UNLOCK(&f->f_mutex);
		return 2;
	}
	
	// Return 0 if new length is same as old length
	if (length == f->length) {
		UNLOCK(&fs->file_mutex);
		UNLOCK(&fs->used_mutex);
		UNLOCK(&fs->dir_mutex);
		UNLOCK(&fs->index_mutex);
		UNLOCK(&fs->o_list->list_mutex);
		UNLOCK(&fs->n_list->list_mutex);
		UNLOCK(&f->f_mutex);
		return 0;
	}
	LOCK(&fs->hash_mutex);
	
	// Resize file (dir_table is synced)
	// Local variables used to remove dependency during file_data and hash_data updates
	int64_t old_length = f->length;
	resize_file_f(f, length, old_length, fs);
	int64_t new_offset = f->offset;
	UNLOCK(&fs->dir_mutex);
	UNLOCK(&fs->index_mutex);
	UNLOCK(&fs->o_list->list_mutex);
	UNLOCK(&fs->n_list->list_mutex);
	UNLOCK(&f->f_mutex);
	
	// Append null bytes as required and sync file_data
	write_null_byte(fs->file, length - old_length, f->offset + old_length);
	msync(fs->file, fs->len[0], MS_SYNC);
	UNLOCK(&fs->file_mutex);
	UNLOCK(&fs->used_mutex);
	
	// Update and sync hash_data if size increased
	if (length > old_length) {
		compute_hash_block_range(new_offset, length, fs);
	}
	msync(fs->hash, fs->len[2], MS_SYNC);
	UNLOCK(&fs->hash_mutex);
	
	printf("resize_file resized \"%s\" o:%lu l:%u dir:%d o_i:%d n_i:%d\n",
		   f->name, f->offset, f->length, f->index, f->o_index, f->n_index);

	return 0;
};

// Moves the file specified to new_offset in file_data (no sync)
// uses file_data, dir_table, file
static void repack_move(file_t* file, uint32_t new_offset, filesys_t* fs) {
	// Move file to new offset
	memmove(fs->file + new_offset, fs->file + file->offset, file->length);
	
	// Update file_t and dir_table
	update_file_offset(new_offset, file);
	update_dir_offset(file, fs);
}

// Force repack regardless of filesystem mutex
// Does not affect zero size files
// (file_data no sync, dir_table no sync, hash_data no sync)
// uses file_data, dir_table, o_list, hash_table
void repack_f(filesys_t* fs) {
	printf("repack Repacking now\n");
	
	file_t** o_list = fs->o_list->list;
	int32_t size = fs->o_list->size;
	
	// Return if no files in filesystem
	if (size <= 0) {
		return;
	}
	
	// Variable for tracking block to hash
	int64_t hash_offset = -1;
	
	// Move first file to offset 0
	// Does not affect zero size files
	LOCK(&o_list[0]->f_mutex);
	if (o_list[0]->offset != 0 && o_list[0]->length != 0) {
		repack_move(o_list[0], 0, fs);
		
		// Update hashing variables
		hash_offset = 0;
	}
	UNLOCK(&o_list[0]->f_mutex);
	
	// Iterate over sorted offset list and move data when necessary
	// Does not affect zero size files
	for (int32_t i = 1; i < size; i++) {
		LOCK(&o_list[i]->f_mutex);
		if (o_list[i]->offset > o_list[i-1]->offset + o_list[i-1]->length &&
			o_list[i]->length != 0) {
			repack_move(o_list[i], o_list[i-1]->offset + o_list[i-1]->length, fs);
			
			// Update hashing variable
			if (hash_offset < 0) {
				hash_offset = o_list[i]->offset;
			}
		}
		UNLOCK(&o_list[i]->f_mutex);
	}
	
	// Hash required area in file_data
	if (hash_offset >= 0) {
		compute_hash_block_range(hash_offset, fs->used - hash_offset, fs);
	}
	
	// printf("repack Done repacking\n");
}

void repack(void * helper) {
    filesys_t* fs = (filesys_t*)helper;
	LOCK(&fs->file_mutex);
	wait_all_ranges(fs);
	LOCK(&fs->dir_mutex);
	LOCK(&fs->o_list->list_mutex);
	LOCK(&fs->hash_mutex);
	
	// Repack, and sync file_data, dir_table hash_data
	repack_f(fs);
	msync(fs->file, fs->len[0], MS_SYNC);
	msync(fs->dir, fs->len[1], MS_SYNC);
	msync(fs->hash, fs->len[2], MS_SYNC);
		
	UNLOCK(&fs->file_mutex);
	UNLOCK(&fs->dir_mutex);
	UNLOCK(&fs->o_list->list_mutex);
	UNLOCK(&fs->hash_mutex);
}

int delete_file(char * filename, void * helper) {
    filesys_t* fs = (filesys_t*)helper;	
	LOCK(&fs->file_mutex);
	LOCK(&fs->used_mutex);
	LOCK(&fs->dir_mutex);
	LOCK(&fs->index_mutex);
	LOCK(&fs->o_list->list_mutex);
	LOCK(&fs->n_list->list_mutex);
		
	// Return 1 if file does not exist
	file_t temp;
	update_file_name(filename, &temp);
	file_t* f = arr_get_s(&temp, fs->n_list);
	if (f == NULL) {
		UNLOCK(&fs->file_mutex);
		UNLOCK(&fs->used_mutex);
		UNLOCK(&fs->dir_mutex);
		UNLOCK(&fs->index_mutex);
		UNLOCK(&fs->o_list->list_mutex);
		UNLOCK(&fs->n_list->list_mutex);
		return 1;
	}
	LOCK(&f->f_mutex);
	
	// Update used space in file_data
	fs->used -= f->length;
	UNLOCK(&fs->file_mutex);
	UNLOCK(&fs->used_mutex);
	
	// Update dir_table indexing array
	fs->index[f->index] = 0;
	UNLOCK(&fs->index_mutex);

	// Remove from arrays using indices
	arr_remove(f->o_index, fs->o_list);
	arr_remove(f->n_index, fs->n_list);
	UNLOCK(&fs->o_list->list_mutex);
	UNLOCK(&fs->n_list->list_mutex);
	
	// Write null byte in dir_table name field and sync
	write_null_byte(fs->dir, 1, f->index * META_LENGTH);
	msync(fs->dir, fs->len[1], MS_SYNC);
	UNLOCK(&fs->dir_mutex);

	printf("delete_file removed \"%s\" o:%lu l:%u dir:%d o_i:%d n_i:%d\n",
		   f->name, f->offset, f->length, f->index, f->o_index, f->n_index);

	// Free file_t struct
	UNLOCK(&f->f_mutex);
	free_file_t(f);
	
	return 0;
}

int rename_file(char * oldname, char * newname, void * helper) {
    filesys_t* fs = (filesys_t*)helper;
	LOCK(&fs->file_mutex);
	LOCK(&fs->dir_mutex);
	LOCK(&fs->n_list->list_mutex);
	
	printf("rename_file renaming \"%s\" to \"%s\"\n",
		   oldname, newname);
	
	// Return 1 if oldname file does not exist or newname file does exist
	file_t temp_old;
	file_t temp_new;
	update_file_name(oldname, &temp_old);
	update_file_name(newname, &temp_new);
	file_t* f = arr_get_s(&temp_old, fs->n_list);
	if (f == NULL || arr_get_s(&temp_new, fs->n_list) != NULL) {
		UNLOCK(&fs->file_mutex);
		UNLOCK(&fs->dir_mutex);
		UNLOCK(&fs->n_list->list_mutex);
		return 1;
	}
	LOCK(&f->f_mutex);
	
	// Update file_t struct and dir_table entry, before syncing dir_table
	update_file_name(newname, f);
	update_dir_name(f, fs);
	msync(fs->dir, fs->len[1], MS_SYNC);
	UNLOCK(&fs->file_mutex);
	UNLOCK(&fs->dir_mutex);
	UNLOCK(&fs->n_list->list_mutex);
	UNLOCK(&f->f_mutex);
	
	printf("rename_file renamed \"%s\" to \"%s\" o:%lu l:%u dir:%d o_i:%d n_i:%d\n",
		   oldname, f->name, f->offset, f->length, f->index, f->o_index, f->n_index);
	
	return 0;
}

int read_file(char * filename, size_t offset, size_t count, void * buf, void * helper) {
	filesys_t* fs = (filesys_t*)helper;
	LOCK(&fs->file_mutex);
	LOCK(&fs->dir_mutex);
	LOCK(&fs->n_list->list_mutex);
	
	// Return 1 if file does not exist
	file_t temp;
	update_file_name(filename, &temp);
	file_t* f = arr_get_s(&temp, fs->n_list);
	if (f == NULL) {
		UNLOCK(&fs->file_mutex);
		UNLOCK(&fs->dir_mutex);
		UNLOCK(&fs->n_list->list_mutex);
		return 1;
	}
	LOCK(&f->f_mutex);
	
	// Return 2 if invalid offset and count for given file
	if (offset + count > f->length) {
		UNLOCK(&fs->file_mutex);
		UNLOCK(&fs->dir_mutex);
		UNLOCK(&fs->n_list->list_mutex);
		UNLOCK(&f->f_mutex);
		return 2;
	}
	UNLOCK(&fs->dir_mutex);
	UNLOCK(&fs->n_list->list_mutex);

	
	// Read count bytes into buf at offset bytes from the start of f
	memcpy(buf, fs->file + f->offset + offset, count);
	UNLOCK(&fs->file_mutex);
	UNLOCK(&f->f_mutex);
	
	printf("read_file read \"%s\" %lu bytes at offset %lu o:%lu l:%u dir:%d o_i:%d n_i:%d\n",
		   f->name, count, offset, f->offset, f->length, f->index, f->o_index, f->n_index);
	
	return 0;
}

int write_file(char * filename, size_t offset, size_t count, void * buf, void * helper) {
    filesys_t* fs = (filesys_t*)helper;
	LOCK(&fs->file_mutex);
	LOCK(&fs->used_mutex);
	LOCK(&fs->dir_mutex);
	LOCK(&fs->index_mutex);
	LOCK(&fs->o_list->list_mutex);
	LOCK(&fs->n_list->list_mutex);
	
	printf("write_file writing to \"%s\" %lu bytes at offset %lu\n", filename, count, offset);
	
	// Return 1 if file does not exist
	file_t temp;
	update_file_name(filename, &temp);
	file_t* f = arr_get_s(&temp, fs->n_list);
	if (f == NULL) {
		UNLOCK(&fs->file_mutex);
		UNLOCK(&fs->used_mutex);
		UNLOCK(&fs->dir_mutex);
		UNLOCK(&fs->index_mutex);
		UNLOCK(&fs->o_list->list_mutex);
		UNLOCK(&fs->n_list->list_mutex);
		return 1;
	}
	LOCK(&f->f_mutex);
	
	// Return 2 if offset is greater than the current file length
	if (offset > f->length) {
		UNLOCK(&fs->file_mutex);
		UNLOCK(&fs->used_mutex);
		UNLOCK(&fs->dir_mutex);
		UNLOCK(&fs->index_mutex);
		UNLOCK(&fs->o_list->list_mutex);
		UNLOCK(&fs->n_list->list_mutex);
		UNLOCK(&f->f_mutex);
		return 2;
	}
	
	// Return 3 if insufficient space in filesystem
	if (fs->used + (offset + count - f->length) > fs->len[0]) {
		UNLOCK(&fs->file_mutex);
		UNLOCK(&fs->used_mutex);
		UNLOCK(&fs->dir_mutex);
		UNLOCK(&fs->index_mutex);
		UNLOCK(&fs->o_list->list_mutex);
		UNLOCK(&fs->n_list->list_mutex);
		UNLOCK(&f->f_mutex);
		return 3;
	}
	LOCK(&fs->hash_mutex);
	
	if (f->length > 150) {
		printf("write_file writing to \"%s\" %lu bytes at offset %lu\n", filename, count, offset);
	}
	
	// Only resize if required to write count bytes at offset
	// from the start of the file
	if (offset + count > f->length) {
		resize_file_f(f, offset + count, offset, fs);
	}
	
	// Write count bytes from buf to file_data and sync
	// (f->offset will be updated if resize occured)
	memcpy(fs->file + f->offset + offset, buf, count);
	msync(fs->file, fs->len[0], MS_SYNC);
	
	// printf("write_file wrote \"%s\" %lu bytes at offset %lu o:%lu l:%u dir:%d o_i:%d n_i:%d\n",
	// 	   f->name, count, offset, f->offset, f->length, f->index, f->o_index, f->n_index);
	
	UNLOCK(&fs->file_mutex);
	UNLOCK(&fs->used_mutex);
	UNLOCK(&fs->dir_mutex);
	UNLOCK(&fs->index_mutex);
	UNLOCK(&fs->o_list->list_mutex);
	UNLOCK(&fs->n_list->list_mutex);
	UNLOCK(&f->f_mutex);
	UNLOCK(&fs->hash_mutex);
	return 0;
}

ssize_t file_size(char * filename, void * helper) {
    filesys_t* fs = (filesys_t*)helper;
	pthread_mutex_lock(&fs->file_mutex);
	pthread_mutex_lock(&fs->dir_mutex);
	
	// Return -1 if file does not exist
	file_t temp;
	update_file_name(filename, &temp);
	file_t* f = arr_get_s(&temp, fs->n_list);
	if (f == NULL) {
		pthread_mutex_unlock(&fs->file_mutex);
		pthread_mutex_unlock(&fs->dir_mutex);
		return -1;
	}
	
	// printf("file_size ret %u \"%s\" o:%lu l:%u dir:%d o_i:%d n_i:%d\n",
	// 	   f->length, f->name, f->offset, f->length, f->index, f->o_index, f->n_index);
	
	// Return length of file
	pthread_mutex_unlock(&fs->file_mutex);
	pthread_mutex_unlock(&fs->dir_mutex);
	return f->length;
}

void fletcher(uint8_t * buf, size_t length, uint8_t * output) {
    // Implementation assumes values passed are in little endian
	// No mutexes are used as the function does not explicitly access the filesystem
	
	// Casting buffer to uint32_t for reading
	uint32_t* buff = (uint32_t*)buf;
	
	// Calculate quotient and remainder for buff
	uint64_t size = length / 4;
	uint64_t rem = length % 4;
	
	// Separate variable for incomplete unsigned integer
	uint32_t last = 0; // Initialised to zero for null byte padding
	if (rem != 0) {
		memcpy(&last, buff + size, sizeof(uint8_t) * rem);
	}
	
	// Hashing variables
	uint64_t a = 0;
	uint64_t b = 0;
	uint64_t c = 0;
	uint64_t d = 0;
	
	// Iterate over buff, updating hash variables
	for (uint64_t i = 0; i < size; i++) {
		// memcpy(&data, buf[i], sizeof(uint32_t));
		a = (a + buff[i]) % (MAX_FILE_DATA_LENGTH-1);
		b = (b + a) % (MAX_FILE_DATA_LENGTH-1);
		c = (c + b) % (MAX_FILE_DATA_LENGTH-1);
		d = (d + c) % (MAX_FILE_DATA_LENGTH-1);
	}
	
	// Hash incomplete unsigned integer if required
	if (rem != 0) {
		a = (a + (uint64_t)last) % (MAX_FILE_DATA_LENGTH-1);
		b = (b + a) % (MAX_FILE_DATA_LENGTH-1);
		c = (c + b) % (MAX_FILE_DATA_LENGTH-1);
		d = (d + c) % (MAX_FILE_DATA_LENGTH-1);
	}
	
	// Copy result to output
	// Casting not used as little endian results should have
	// least significant bits in the same position
	memcpy(output, &a, sizeof(uint32_t));
	memcpy(output + 4, &b, sizeof(uint32_t));
	memcpy(output + 8, &c, sizeof(uint32_t));
	memcpy(output + 12, &d, sizeof(uint32_t));
}

void hash_node(int32_t n_index, filesys_t* fs) {
	// If internal node, calculate hash of concatenated child hashes
	if (n_index < fs->leaf_offset) {
		uint8_t hash_cat[2 * HASH_LENGTH];
		memcpy(hash_cat, fs->hash + lc_index(n_index) * HASH_LENGTH, HASH_LENGTH);
		memcpy(hash_cat + HASH_LENGTH, fs->hash + rc_index(n_index) * HASH_LENGTH, HASH_LENGTH);
		fletcher(hash_cat, 2 * HASH_LENGTH, fs->hash + n_index * HASH_LENGTH);
		
	// Otherwise, calculate hash of file_data block for leaf node
	} else {
		fletcher(fs->file + (n_index - fs->leaf_offset) * BLOCK_LENGTH, BLOCK_LENGTH,
				 fs->hash + n_index * HASH_LENGTH);
	}
}

void hash_recurse(int32_t n_index, filesys_t* fs) {
	// Base case: if leaf node, hash file_data block and return
	if (n_index >= fs->leaf_offset) {
		hash_node(n_index, fs);
		return;
	}
	
	// Traverse left child, then right child, then current node
	hash_recurse(lc_index(n_index), fs);
	hash_recurse(rc_index(n_index), fs);
	hash_node(n_index, fs);
}

void compute_hash_tree(void * helper) {
	filesys_t* fs = (filesys_t*)helper;
	pthread_mutex_lock(&fs->file_mutex);
	pthread_mutex_lock(&fs->dir_mutex);
	
	// // Use postorder traversal to hash children before parent node
	hash_recurse(0, fs);
	
	// TODO: REMOVE
	// compute_hash_block_range(0, fs->len[0], fs);

	// Sync hash_data
	msync(fs->hash, fs->len[2], MS_SYNC);
	
	pthread_mutex_unlock(&fs->file_mutex);
	pthread_mutex_unlock(&fs->dir_mutex);
}

// Force calculation of hash block regardless of system mutexes
// Assumes hash_data is locked (hash_data no sync)
void compute_hash_block_f(size_t block_offset, filesys_t* fs) {
	// block_offset is the leaf node number
	int32_t n_index = fs->leaf_offset + block_offset;
	fletcher(fs->file + block_offset * BLOCK_LENGTH, BLOCK_LENGTH, fs->hash + n_index * HASH_LENGTH);
	
	// Traverse through parent nodes to root node, updating hashes
	uint8_t hash_cat[2 * HASH_LENGTH]; // Space for concatenated hash value
	n_index = p_index(n_index);
	while (n_index >= 0) {
		memcpy(hash_cat, fs->hash + lc_index(n_index) * HASH_LENGTH, HASH_LENGTH);
		memcpy(hash_cat + HASH_LENGTH, fs->hash + rc_index(n_index) * HASH_LENGTH, HASH_LENGTH);
		fletcher(hash_cat, 2 * HASH_LENGTH, fs->hash + n_index * HASH_LENGTH);
		n_index = p_index(n_index);
	}
}

// Update hashes for blocks affected by write to file with f_offset and f_length
void compute_hash_block_range(int64_t f_offset, int64_t f_length, filesys_t* fs) {
	int64_t first_block = f_offset / BLOCK_LENGTH;
	int64_t last_block = (f_offset + f_length - 1) / BLOCK_LENGTH;
	for (int64_t i = first_block; i <= last_block; i++) {
		compute_hash_block_f(i, fs);
	}
}

void compute_hash_block(size_t block_offset, void * helper) {
	filesys_t* fs = (filesys_t*)helper;
	LOCK(&fs->file_mutex);
	LOCK(&fs->dir_mutex);
	
	// Compute hash block and sync hash_data
	compute_hash_block_f(block_offset, fs);
	msync(fs->hash, fs->len[2], MS_SYNC);
	
	UNLOCK(&fs->file_mutex);
	UNLOCK(&fs->dir_mutex);
}
