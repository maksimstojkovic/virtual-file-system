#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>

#include "structs.h"
#include "lock.h"
#include "helper.h"
#include "arr.h"
#include "myfilesystem.h"

// TODO: REMOVE ANY ARRAYS INITIALISED WITH VARIABLE VALUES AS LENGTH
// TODO: TRY REPLACING POST-FIX INCREMENT (++) WITH PREFIX
// TODO: TRY REPLACING MODULUS OPERATOR WITH rem=a-b*(a/b), using intermediate value (a/b)

// POSSIBLY TRY USING MS_ASYNC IN MSYNC CALLS
// DOUBLE CHECK MSYNC POINTERS AND LENGTHS


void * init_fs(char * f1, char * f2, char * f3, int n_processors) {
    filesys_t* fs = salloc(sizeof(*fs));
	
	// Initialise filesystem lock
	init_lock(&fs->lock);
	
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
	if (fs->file == MAP_FAILED || fs->dir == MAP_FAILED || fs->hash == MAP_FAILED) {
		perror("init_fs: Failed to map files to memory");
		exit(1);
	}

	// Initialise filesystem variables
	fs->nproc = n_processors;
	fs->index_len = fs->len[1] / META_LENGTH;
	fs->index = scalloc(sizeof(*fs->index) * fs->index_len);
	fs->o_list = arr_init(fs->index_len, OFFSET, fs);
	fs->n_list = arr_init(fs->index_len, NAME, fs);
	fs->used = 0;
	fs->tree_len = fs->len[2] / HASH_LENGTH;
	fs->leaf_offset = fs->tree_len / 2;
	
	// Read through dir_table for existing files
	char name[NAME_LENGTH] = {0};
	uint64_t offset;
	uint32_t length;
	for (int32_t i = 0; i < fs->index_len; i++) {
		memcpy(&name, fs->dir + i * META_LENGTH, NAME_LENGTH-1);
		
		// Valid filenames do not start with a null byte
		if (name[0] != '\0') {
			memcpy(&length, fs->dir + i * META_LENGTH + NAME_LENGTH + OFFSET_LENGTH, sizeof(uint32_t));
			
			// Assign zero size files with offset = max length of file_data
			// See arr.c for explanation
			if (length == 0) {
				offset = MAX_FILE_DATA_LENGTH;
			} else {
				memcpy(&offset, fs->dir + i * META_LENGTH + NAME_LENGTH, sizeof(uint32_t));
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
	
	// Unmap open files
	munmap(fs->file, fs->len[0]);
	munmap(fs->dir, fs->len[1]);
	munmap(fs->hash, fs->len[2]);
	
	// Close file descriptors
	close(fs->file_fd);
	close(fs->dir_fd);
	close(fs->hash_fd);
	
	// Destory filesystem lock
	destory_lock(&fs->lock);
	
	// Free heap memory allocated
	free_arr(fs->o_list);
	free_arr(fs->n_list);
	free(fs->index);
	free(fs);
}

// Returns first empty index in dir_table (index success)
static int32_t new_file_index(filesys_t* fs) {
	if (fs == NULL) {
		perror("new_file_index: NULL filesystem");
		exit(1);
	}
	
	// Iterate over index array for space in dir_table
	int32_t len = fs->index_len;
	uint8_t* index = fs->index;
	for (int32_t i = 0; i < len; i++) {
		if (index[i] == 0) {
			return i;
		}
	}

	perror("new_file_index: Filesystem full, no index available");
	exit(1);
}

// Return offset in file_data for insertion (offset success)
uint64_t new_file_offset(size_t length, filesys_t* fs) {
	// Check for valid arguments
	if (fs == NULL || length > fs->len[0]) {
		perror("new_file_offset: Invalid arguments");
		exit(1);
	}
	
	// Set offset of zero size files to max length of file_data
	if (length == 0) {
		return MAX_FILE_DATA_LENGTH;
	}
	
	file_t** o_list = fs->o_list->list;
	int32_t size = fs->o_list->size;
	
	// Cases for no elements in offset list, or first element not at offset 0
	if (size <= 0 || o_list[0]->offset >= length) {
		return 0;
	}
	
	// Check space between elements in offset list
	for (int32_t i = 1; i < size; i++) {
		if (o_list[i]->offset - (o_list[i-1]->offset + o_list[i-1]->length) >= length) {
			return o_list[i-1]->offset + o_list[i-1]->length;
		}
	}
	
	// Check space between last element in offset list and end of file_data
	if (fs->len[0] - (o_list[size-1]->offset + o_list[size-1]->length) >= length) {
		return o_list[size-1]->offset + o_list[size-1]->length;
	}
	
	// Repack hash_data if no large enough contiguous space found
	repack_f(fs);
	
	// Check space at end of file_data
	if (fs->len[0] - (o_list[size-1]->offset + o_list[size-1]->length) >= length) {
		return o_list[size-1]->offset + o_list[size-1]->length;
	}

	perror("new_file_offset: Unable to find valid offset");
	exit(1);
}

int create_file(char * filename, size_t length, void * helper) {
	filesys_t* fs = (filesys_t*)helper;
	write_lock(&fs->lock);
	
	printf("create_file creating \"%s\" of %lu bytes\n", filename, length);

	// Return 1 if file already exists
	file_t temp;
	update_file_name(filename, &temp);
	if (arr_get_s(&temp, fs->n_list) != NULL) {
		write_unlock(&fs->lock);
		return 1;
	}
	
	// Return 2 if insufficient space in filesystem
	if (fs->used + length > fs->len[0]) {
		write_unlock(&fs->lock);
		return 2;
	}
	
	// Find available index in dir_table and space in file_data
	int32_t index = new_file_index(fs);
	uint64_t offset = new_file_offset(length, fs);

	// Create new file_t struct and insert into both sorted lists
	file_t* f = new_file_t(filename, offset, length, index);
	arr_insert_s(f, fs->o_list);
	arr_insert_s(f, fs->n_list);

	// Write file metadata to dir_table and update index array
	write_dir_file(f, fs);
	msync(fs->dir, fs->len[1], MS_ASYNC);
	fs->index[index] = 1;

	// Write null bytes to file_data and update filesystem variables
	write_null_byte(fs->file, length, f->offset);
	msync(fs->file, fs->len[0], MS_ASYNC);
	fs->used += length;
	
	// Update hash_data
	if (length > 0) {
		compute_hash_block_range(offset, length, fs);
	}
	msync(fs->hash, fs->len[2], MS_ASYNC);
	
	printf("create_file added \"%s\" o:%lu l:%u dir:%d o_i:%d n_i:%d\n",
		   f->name, f->offset, f->length, f->index, f->o_index, f->n_index);
	
	write_unlock(&fs->lock);
	return 0;
}

// Resizes files regardless of filesystem lock state
void resize_file_helper(file_t* file, size_t length, size_t copy, filesys_t* fs) {
	int64_t old_length = file->length;
	
	// Find suitable space in file_data if length increased
	if (length > old_length) {
		file_t** list = fs->o_list->list;
		int32_t size = fs->o_list->size;
		
		// Check for insufficient space from the next file or the end of file_data
		if ((file->o_index < size - 1 && list[file->o_index+1]->offset - file->offset < length) ||
		    (file->o_index == size - 1 && fs->len[0] - file->offset < length)) {
			// Store copy bytes of data from file_data into a buffer
			uint8_t* temp = salloc(sizeof(*temp) * copy);
			memcpy(temp, fs->file + file->offset, copy);
			
			// Remove the file_t* from the sorted offset list
			arr_remove(file->o_index, fs->o_list);
			
			// Repack and write copied file to end of file_data
			repack_f(fs);
			memcpy(fs->file + fs->used - old_length, temp, copy);
			msync(fs->file, fs->len[0], MS_ASYNC);
			free(temp);
			
			// Update file_t and dir_table entry's offset
			update_file_offset(fs->used - old_length, file);
			update_dir_offset(file, fs);
			
			// Reinsert file into sorted offset list
			arr_insert_s(file, fs->o_list);
		}
	}
	
	// Update file_t and dir_table if length changed
	if (length != old_length) {
		// Update offset variable for zero size files
		if (length == 0) {
			// TODO: Check if writing to dir_table
			update_file_offset(MAX_FILE_DATA_LENGTH, file);
		}
		
		// Update file struct and dir_table
		update_file_length(length, file);
		update_dir_length(file, fs);
		msync(fs->dir, fs->len[1], MS_ASYNC);
	}
	
	// Update filesystem variables
	fs->used += length - old_length;
}

int resize_file(char * filename, size_t length, void * helper) {
    filesys_t* fs = (filesys_t*)helper;
	write_lock(&fs->lock);
	
	printf("resize_file resizing \"%s\" to %lu bytes\n", filename, length);
	
	// Return 1 if file does not exist
	file_t temp;
	update_file_name(filename, &temp);
	file_t* f = arr_get_s(&temp, fs->n_list);
	if (f == NULL) {
		write_unlock(&fs->lock);
		return 1;
	}
	
	// Return 2 if insufficient space in filesystem
	if (fs->used + (length - f->length) > fs->len[0]) {
		write_unlock(&fs->lock);
		return 2;
	}
	
	// Return 0 if new length is same as old length
	if (length == f->length) {
		write_unlock(&fs->lock);
		return 0;
	}
	
	// Resize file
	int64_t old_length = f->length;
	resize_file_helper(f, length, old_length, fs);
	
	// Append null bytes as required
	printf("%ld %ld %ld\n", length, old_length, f->offset);
	write_null_byte(fs->file, length - old_length, f->offset + old_length);
	msync(fs->file, fs->len[0], MS_ASYNC);
	
	// Update hash_data if size increased
	if (length > old_length) {
		compute_hash_block_range(f->offset, length, fs);
	}
	msync(fs->hash, fs->len[2], MS_ASYNC);
	
	printf("resize_file resized \"%s\" o:%lu l:%u dir:%d o_i:%d n_i:%d\n",
		   f->name, f->offset, f->length, f->index, f->o_index, f->n_index);

	write_unlock(&fs->lock);
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
// uses file_data, dir_table, o_list, hash_table (assumes no active read/write operations)
void repack_f(filesys_t* fs) {
	// printf("repack Repacking now\n");
	
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
	if (o_list[0]->offset != 0 && o_list[0]->length != 0) {
		repack_move(o_list[0], 0, fs);
		
		// Update hashing variables
		hash_offset = 0;
	}
	
	// Iterate over sorted offset list and move data when necessary
	// Does not affect zero size files
	for (int32_t i = 1; i < size; i++) {
		if (o_list[i]->offset > o_list[i-1]->offset + o_list[i-1]->length &&
			o_list[i]->length != 0) {
			repack_move(o_list[i], o_list[i-1]->offset + o_list[i-1]->length, fs);
			
			// Update hashing variable
			if (hash_offset < 0) {
				hash_offset = o_list[i]->offset;
			}
		}
	}
	
	// Hash required area in file_data
	if (hash_offset >= 0) {
		compute_hash_block_range(hash_offset, fs->used - hash_offset, fs);
	}
	
	// printf("repack Done repacking\n");
}

void repack(void * helper) {
    filesys_t* fs = (filesys_t*)helper;
	write_lock(&fs->lock);
	
	// Repack, and sync file_data, dir_table hash_data
	repack_f(fs);
	msync(fs->file, fs->len[0], MS_ASYNC);
	msync(fs->dir, fs->len[1], MS_ASYNC);
	msync(fs->hash, fs->len[2], MS_ASYNC);
	
	write_unlock(&fs->lock);
}

int delete_file(char * filename, void * helper) {
    filesys_t* fs = (filesys_t*)helper;	
	write_lock(&fs->lock);
		
	// Return 1 if file does not exist
	file_t temp;
	update_file_name(filename, &temp);
	file_t* f = arr_get_s(&temp, fs->n_list);
	if (f == NULL) {
		write_unlock(&fs->lock);
		return 1;
	}
	
	// Update used space in file_data
	fs->used -= f->length;
	
	// Update dir_table indexing array
	fs->index[f->index] = 0;
	// UNLOCK(&fs->index_mutex);

	// Remove from arrays using indices
	arr_remove(f->o_index, fs->o_list);
	arr_remove(f->n_index, fs->n_list);
	
	// Write null byte in dir_table name field and sync
	write_null_byte(fs->dir, 1, f->index * META_LENGTH);
	msync(fs->dir, fs->len[1], MS_SYNC);
	
	printf("delete_file removed \"%s\" o:%lu l:%u dir:%d o_i:%d n_i:%d\n",
		   f->name, f->offset, f->length, f->index, f->o_index, f->n_index);
	
	// Free file_t struct
	free_file_t(f);
	
	write_unlock(&fs->lock);
	return 0;
}

int rename_file(char * oldname, char * newname, void * helper) {
    filesys_t* fs = (filesys_t*)helper;
	write_lock(&fs->lock);
	
	printf("rename_file renaming \"%s\" to \"%s\"\n",
		   oldname, newname);
	
	// Return 1 if oldname file does not exist or newname file does exist
	file_t temp_old;
	file_t temp_new;
	update_file_name(oldname, &temp_old);
	update_file_name(newname, &temp_new);
	file_t* f = arr_get_s(&temp_old, fs->n_list);
	if (f == NULL || arr_get_s(&temp_new, fs->n_list) != NULL) {
		write_unlock(&fs->lock);
		return 1;
	}
	
	// Update file_t struct and dir_table entry, before syncing dir_table
	update_file_name(newname, f);
	update_dir_name(f, fs);
	msync(fs->dir, fs->len[1], MS_SYNC);
	
	printf("rename_file renamed \"%s\" to \"%s\" o:%lu l:%u dir:%d o_i:%d n_i:%d\n",
		   oldname, f->name, f->offset, f->length, f->index, f->o_index, f->n_index);
	
	write_unlock(&fs->lock);
	return 0;
}

int read_file(char * filename, size_t offset, size_t count, void * buf, void * helper) {
	filesys_t* fs = (filesys_t*)helper;
	read_lock(&fs->lock);
	
	printf("read_file reading \"%s\" %lu bytes at offset %lu\n", filename, count, offset);
	
	// Return 1 if file does not exist
	file_t temp;
	update_file_name(filename, &temp);
	file_t* f = arr_get_s(&temp, fs->n_list);
	if (f == NULL) {
		read_unlock(&fs->lock);
		return 1;
	}
	
	// Return 2 if invalid offset and count for given file
	if (offset + count > f->length) {
		read_unlock(&fs->lock);
		return 2;
	}
	
	// Return 3 if invalid hashes
	if (verify_hash_range(f->offset + offset, count, fs) != 0) {
		read_unlock(&fs->lock);
		return 3;
	}
	
	// Read count bytes into buf at offset bytes from the start of f
	memcpy(buf, fs->file + f->offset + offset, count);
	
	// printf("read_file read \"%s\" %lu bytes at offset %lu o:%lu l:%u dir:%d o_i:%d n_i:%d\n",
	// 	   f->name, count, offset, f->offset, f->length, f->index, f->o_index, f->n_index);
	
	read_unlock(&fs->lock);
	return 0;
}

int write_file(char * filename, size_t offset, size_t count, void * buf, void * helper) {
    filesys_t* fs = (filesys_t*)helper;
	write_lock(&fs->lock);
	
	// printf("write_file writing to \"%s\" %lu bytes at offset %lu\n", filename, count, offset);
	// printf("\"%s\" %lu bytes at offset %lu\n", filename, count, offset);
	
	// Return 1 if file does not exist
	file_t temp;
	update_file_name(filename, &temp);
	file_t* f = arr_get_s(&temp, fs->n_list);
	if (f == NULL) {
		write_unlock(&fs->lock);
		return 1;
	}
	// LOCK(&f->f_mutex);
	// UNLOCK(&fs->n_list->list_mutex);
	
	// Return 2 if offset is greater than the current file length
	if (offset > f->length) {
		write_unlock(&fs->lock);
		return 2;
	}
	
	// Return 3 if insufficient space in filesystem
	if (fs->used + (offset + count - f->length) > fs->len[0]) {
		write_unlock(&fs->lock);
		return 3;
	}
	
	// Only resize if required to write count bytes at offset
	// from the start of the file
	if (offset + count > f->length) {
		resize_file_helper(f, offset + count, offset, fs);
	}
	
	// Local variables to eliminate file dependency
	int64_t write_offset = f->offset + offset;
	
	// Write count bytes from buf to file_data and sync
	// (f->offset will be updated if resize occured)
	memcpy(fs->file + write_offset, buf, count);
	msync(fs->file, fs->len[0], MS_ASYNC);
	
	// Update and sync hash_data if size increased
	// compute_hash_block_range(write_offset, count, fs);
	hash_recurse(0, fs);
	msync(fs->hash, fs->len[2], MS_ASYNC);
	
	// printf("write_file wrote \"%s\" %lu bytes at offset %lu o:%lu l:%u dir:%d o_i:%d n_i:%d\n",
	// 	   f->name, count, offset, f->offset, f->length, f->index, f->o_index, f->n_index);
	
	
	write_unlock(&fs->lock);
	return 0;
}

ssize_t file_size(char * filename, void * helper) {
    filesys_t* fs = (filesys_t*)helper;
	read_lock(&fs->lock);
	
	// Return -1 if file does not exist
	file_t temp;
	update_file_name(filename, &temp);
	file_t* f = arr_get_s(&temp, fs->n_list);
	if (f == NULL) {
		read_unlock(&fs->lock);
		return -1;
	}
	
	// printf("file_size ret %u \"%s\" o:%lu l:%u dir:%d o_i:%d n_i:%d\n",
	// 	   f->length, f->name, f->offset, f->length, f->index, f->o_index, f->n_index);
	
	// Return length of file
	read_unlock(&fs->lock);
	return f->length;
}

void fletcher(uint8_t * buf, size_t length, uint8_t * output) {
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

void hash_node(int32_t n_index, uint8_t* out, filesys_t* fs) {
	// If internal node, calculate hash of concatenated child hashes
	if (n_index < fs->leaf_offset) {
		uint8_t hash_cat[2 * HASH_LENGTH];
		memcpy(hash_cat, fs->hash + lc_index(n_index) * HASH_LENGTH, HASH_LENGTH);
		memcpy(hash_cat + HASH_LENGTH, fs->hash + rc_index(n_index) * HASH_LENGTH, HASH_LENGTH);
		fletcher(hash_cat, 2 * HASH_LENGTH, out);
		
	// Otherwise, calculate hash of file_data block for leaf node
	} else {
		fletcher(fs->file + (n_index - fs->leaf_offset) * BLOCK_LENGTH, BLOCK_LENGTH, out);
	}
}

void hash_recurse(int32_t n_index, filesys_t* fs) {
	// Base case: if leaf node, hash file_data block and return
	if (n_index >= fs->leaf_offset) {
		hash_node(n_index, fs->hash + n_index * HASH_LENGTH, fs);
		return;
	}
	
	// Traverse left child, then right child, then current node
	hash_recurse(lc_index(n_index), fs);
	hash_recurse(rc_index(n_index), fs);
	hash_node(n_index, fs->hash + n_index * HASH_LENGTH, fs);
}

void compute_hash_tree(void * helper) {
	filesys_t* fs = (filesys_t*)helper;
	write_lock(&fs->lock);
	
	// Use postorder traversal to hash children before parent node
	hash_recurse(0, fs);

	// Sync hash_data
	msync(fs->hash, fs->len[2], MS_ASYNC);

	write_unlock(&fs->lock);
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

int32_t verify_hash(size_t start_offset, size_t end_offset, filesys_t* fs) {
	int32_t n_index;
	uint8_t hash[HASH_LENGTH];
	for (int32_t i = start_offset; i <= end_offset; i++) {
		n_index = fs->leaf_offset + i;
		while (n_index > 0) {
			hash_node(n_index, hash, fs);
			if (memcmp(hash, fs->hash + n_index * HASH_LENGTH, HASH_LENGTH) != 0) {
				return 1;
			}
			n_index = p_index(n_index);
		}
	}
	return 0;
}

int32_t verify_hash_range(int64_t f_offset, int64_t f_length, filesys_t* fs) {
	int64_t first_block = f_offset / BLOCK_LENGTH;
	int64_t last_block = (f_offset + f_length - 1) / BLOCK_LENGTH;
	printf("First: %ld Last %ld\n", first_block, last_block);
	return verify_hash(first_block, last_block, fs);
}		

void compute_hash_block(size_t block_offset, void * helper) {
	filesys_t* fs = (filesys_t*)helper;
	write_lock(&fs->lock);
	
	// Compute hash block and sync hash_data
	compute_hash_block_f(block_offset, fs);
	msync(fs->hash, fs->len[2], MS_ASYNC);
	
	write_unlock(&fs->lock);
}
