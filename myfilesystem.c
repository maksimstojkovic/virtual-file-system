#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <assert.h>

#include "structs.h"
#include "helper.h"
#include "arr.h"
#include "myfilesystem.h"

/*
 * Filesystem Implementation
 *
 * This filesystem only uses one mutex lock for all read and write operations.
 * Write operations require exclusive control of the filesystem as they have
 * the potential to modify some, or all, of the underlying filesystem files
 * (file_data, dir_table, hash_data). Additionally, the use of a mutex during
 * read operations was considered more appropriate than a lock which allows
 * parallel reads, as in the event that multiple threads attempt to read to the
 * same buffer simultaneously, the integrity of the buffer is only guaranteed
 * if both read operations are blocking, serialising the reads. Hence, only one
 * mutex was used for the synchronisation of the filesystem.
 */

void * init_fs(char * f1, char * f2, char * f3, int n_processors) {
    // Allocate space for filesystem helper
	filesys_t* fs = salloc(sizeof(*fs));
	
	// Initialise filesystem lock
	pthread_mutex_init(&fs->lock, NULL);
	
	// Check if files exist
	fs->file_fd = open(f1, O_RDWR);
	fs->dir_fd = open(f2, O_RDWR);
	fs->hash_fd = open(f3, O_RDWR);
	assert(fs->file_fd >= 0 && fs->dir_fd >= 0 &&
	       fs->hash_fd >= 0 && "failed to open files");

	// Store file lengths in filesystem
	struct stat stats[3];
	assert(!fstat(fs->file_fd, &stats[0]) &&
		   !fstat(fs->dir_fd, &stats[1]) &&
		   !fstat(fs->hash_fd, &stats[2]) && "failed to get file lengths");

	fs->file_data_len = stats[0].st_size;
	fs->dir_table_len = stats[1].st_size;
	fs->hash_data_len = stats[2].st_size;
	
	// Map files to memory using mmap
	fs->file = mmap(NULL, fs->file_data_len, PROT_READ | PROT_WRITE,
			MAP_SHARED, fs->file_fd, 0);
	fs->dir = mmap(NULL, fs->dir_table_len, PROT_READ | PROT_WRITE,
			MAP_SHARED, fs->dir_fd, 0);
	fs->hash = mmap(NULL, fs->hash_data_len, PROT_READ | PROT_WRITE,
			MAP_SHARED, fs->hash_fd, 0);
	assert(fs->file != MAP_FAILED && fs->dir != MAP_FAILED &&
		   fs->hash != MAP_FAILED && "mmap failed");

	// Initialise filesystem variables
	fs->n_processors = n_processors;
	fs->index_len = fs->dir_table_len / META_LEN;
	fs->index_count = 0;
	fs->index = scalloc(sizeof(*fs->index) * fs->index_len);
	fs->o_list = arr_init(fs->index_len, OFFSET, fs);
	fs->n_list = arr_init(fs->index_len, NAME, fs);
	fs->used = 0;
	fs->tree_len = fs->hash_data_len / HASH_LEN;
	fs->leaf_offset = fs->tree_len / 2;
	UNUSED(fs->n_processors);

	// Read through dir_table for existing files
	char name[NAME_LEN] = {0};
	uint64_t offset = 0;
	uint32_t length = 0;
	for (int32_t i = 0; i < fs->index_len; ++i) {
		// Copy 63 characters from name field of dir_table entry
		memcpy(&name, fs->dir + i * META_LEN, NAME_LEN - 1);
		
		// Valid filenames do not start with a null byte
		if (name[0] != '\0') {
			memcpy(&length, fs->dir + i * META_LEN + NAME_LEN + OFFSET_LEN,
					sizeof(uint32_t));
			
			// Assign max length of file_data as offset for zero size files
			// See arr.c for justification
			if (length == 0) {
				offset = MAX_FILE_DATA_LEN;
			} else {
				memcpy(&offset, fs->dir + i * META_LEN + NAME_LEN,
						sizeof(uint32_t));

				// Ensure remaining bytes in offset uint64_t are 0
				memset((uint8_t*)(&offset) + sizeof(uint32_t), 0,
						sizeof(uint32_t));
			}
			
			// Create file_t and add to sorted arrays
			file_t* f = file_init(name, offset, length, i);
			arr_sorted_insert(f, fs->o_list);
			arr_sorted_insert(f, fs->n_list);
			
			// Updating filesystem variables
			fs->used += f->length;
			fs->index[i] = 1;
			++fs->index_count;
		}
	}
	
	return fs;
}

void close_fs(void * helper) {
	// Return if helper is NULL
	if (helper == NULL) {
		return;
	}
	
	filesys_t* fs = (filesys_t*)helper;
	
	munmap(fs->file, fs->file_data_len);
	munmap(fs->dir, fs->dir_table_len);
	munmap(fs->hash, fs->hash_data_len);
	
	close(fs->file_fd);
	close(fs->dir_fd);
	close(fs->hash_fd);
	
	pthread_mutex_destroy(&fs->lock);
	
	free_arr(fs->o_list);
	free_arr(fs->n_list);
	free(fs->index);
	free(fs);
}

/*
 * Returns first empty index in dir_table
 *
 * returns: Lowest empty index in dir_table
 */
int32_t new_file_index(filesys_t* fs) {
	assert(fs != NULL && "invalid args");

	// Iterate over index array for dir_table
	int32_t index = -1;
	int32_t len = fs->index_len;
	uint8_t* arr = fs->index;
	for (int32_t i = 0; i < len; ++i) {
		if (arr[i] == 0) {
			index = i;
			break;
		}
	}

	assert(index >= 0 && "dir_table full");
	return index;
}

/*
 * Returns offset in file_data for insertion
 *
 * length: length of new file
 * hash_offset: pointer to variable storing offset of first modified
 * 				byte in file_data
 *
 * returns: valid file_data offset for new file, repacking if required
 */
uint64_t new_file_offset(size_t length, int64_t* hash_offset, filesys_t* fs) {
	assert(fs != NULL && length <= fs->file_data_len && "invalid args");
	assert(fs->used + length <= fs->file_data_len &&
	       "insufficient space in file_data");

	// Return max length of file_data for zero size files
	if (length == 0) {
		return MAX_FILE_DATA_LEN;
	}

	// Find offset of first non-zero size file
	int32_t size = fs->o_list->size;
	file_t* first_non_zero_file = find_next_nonzero_file(0, fs->o_list);
	
	// Return 0 if no files, only zero size files, or if first non-zero size
	// file offset is large enough
	if (size <= 0 || first_non_zero_file == NULL ||
			first_non_zero_file->offset >= length) {
		return 0;
	}

	uint64_t start_curr_file = 0;
	uint64_t end_prev_file = 0;
	int32_t search_index = first_non_zero_file->o_index + 1;
	file_t* prev_non_zero_file = first_non_zero_file;
	file_t* curr_non_zero_file =
			find_next_nonzero_file(search_index, fs->o_list);

	// Check space between remaining elements in the offset array
	while (curr_non_zero_file != NULL) {
		start_curr_file = curr_non_zero_file->offset;
		end_prev_file =
				prev_non_zero_file->offset + prev_non_zero_file->length;

		// Return offset of first byte after end of previous
		// file if sufficient space
		if (start_curr_file - end_prev_file >= length) {
			return end_prev_file;
		}

		prev_non_zero_file = curr_non_zero_file;
		search_index = curr_non_zero_file->o_index + 1;
		curr_non_zero_file =
				find_next_nonzero_file(search_index, fs->o_list);
	}

	uint64_t end_last_file =
			prev_non_zero_file->offset + prev_non_zero_file->length;

	// Check space between last non-zero size file in offset list and the end
	// of file_data
	if (fs->file_data_len - end_last_file >= length) {
		return end_last_file;
	}
	
	// Repack file_data if no large enough contiguous space found
	if (hash_offset != NULL) {
		*hash_offset = repack_helper(fs);
	} else {
		repack_helper(fs);
	}
	
	// Return offset of first byte in contiguous block after repack
	return fs->used;
}

int create_file(char * filename, size_t length, void * helper) {
	filesys_t* fs = (filesys_t*)helper;
	LOCK(&fs->lock);

	// Return 1 if file already exists
	file_t temp;
	update_file_name(filename, &temp);
	if (arr_get_by_key(&temp, fs->n_list) != NULL) {
		UNLOCK(&fs->lock);
		return 1;
	}
	
	// Return 2 if insufficient space in file_data or dir_table
	if (fs->used + length > fs->file_data_len ||
		fs->index_count >= fs->index_len) {
		UNLOCK(&fs->lock);
		return 2;
	}
	
	// Find available index in dir_table and space in file_data
	int64_t hash_offset = -1;
	int32_t index = new_file_index(fs);
	uint64_t offset = new_file_offset(length, &hash_offset, fs);

	// Create new file_t struct and insert into both sorted lists
	file_t* f = file_init(filename, offset, length, index);
	arr_sorted_insert(f, fs->o_list);
	arr_sorted_insert(f, fs->n_list);

	// Write file metadata to dir_table and update index array
	write_dir_file(f, fs);
	fs->index[index] = 1;
	++fs->index_count;

	// Only perform file_data updates for non-zero size files
	if (length > 0) {
		// Write null bytes to file_data and update filesystem variables
		write_null_byte(fs->file, offset, length);
		fs->used += length;

		if (hash_offset >= 0) {
			// Hash blocks modified by repack until end of file
			compute_hash_block_range(hash_offset, fs->used - hash_offset, fs);
		} else {
			// Only hash blocks containing the file
			compute_hash_block_range(offset, length, fs);
		}
	}
	
	msync(fs->file, fs->file_data_len, MS_ASYNC);
	msync(fs->dir, fs->dir_table_len, MS_ASYNC);
	msync(fs->hash, fs->hash_data_len, MS_ASYNC);
	
	UNLOCK(&fs->lock);
	return 0;
}

/*
 * Helper for resizing files, independent of the filesystem lock state
 *
 * file: file_t of file being resized
 * length: new file size
 * copy: number of bytes to copy if repacking required, reduces copying during
 * 		 write_file calls which overwrite data
 *
 * returns: offset of first byte repacked if repack occurred, else -1
 */
int64_t resize_file_helper(file_t* file, size_t length, size_t copy, filesys_t* fs) {
	int64_t hash_offset = -1;
	int64_t old_length = file->length;

	// Find suitable space in file_data if length increased
	if (length > old_length) {
		int64_t old_offset = file->offset;
		file_t* next_non_zero_file = NULL;

		// Expansion of zero size files
		if (old_offset >= MAX_FILE_DATA_LEN) {
			// Remove the file from the sorted offset list
			arr_remove(file->o_index, fs->o_list);

			// Find first non-zero size file
			next_non_zero_file = find_next_nonzero_file(0, fs->o_list);

			// Repack if first non-zero size file offset is not large enough
			if (next_non_zero_file != NULL &&
					next_non_zero_file->offset < length) {
				hash_offset = repack_helper(fs);
				update_file_offset(fs->used, file);

			// Otherwise, update file offset to 0
			} else {
				update_file_offset(0, file);
			}

			update_dir_offset(file, fs);

			// Re-insert file into sorted offset list
			arr_sorted_insert(file, fs->o_list);
			
		// Expansion of non-zero size files
		} else {
			// Find next non-zero size file
			next_non_zero_file =
					find_next_nonzero_file(file->o_index + 1, fs->o_list);

			int repack_required = 0;
			if (next_non_zero_file != NULL) {
				// Repack if insufficient space between files
				repack_required =
						next_non_zero_file->offset - file->offset < length;
			} else {
				// Repack if insufficient space from end of file_data
				repack_required =
						fs->file_data_len - file->offset < length;
			}

			if (repack_required) {
				// Copy required data into a buffer
				uint8_t* temp = salloc(sizeof(*temp) * copy);
				memcpy(temp, fs->file + file->offset, copy);
				
				// Remove file from sorted offset list
				arr_remove(file->o_index, fs->o_list);

				// Repack and write buffer contents to the end of file_data
				hash_offset = repack_helper(fs);
				memcpy(fs->file + fs->used - old_length, temp, copy);
				free(temp);
				
				update_file_offset(fs->used - old_length, file);
				update_dir_offset(file, fs);

				// Re-insert file into sorted offset list
				arr_sorted_insert(file, fs->o_list);
			}
		}
	}
	
	// Update file and dir_table if length changed
	if (length != old_length) {
		update_file_length(length, file);
		update_dir_length(file, fs);

		fs->used += length - old_length;
	}

	return hash_offset;
}

int resize_file(char * filename, size_t length, void * helper) {
    filesys_t* fs = (filesys_t*)helper;
	LOCK(&fs->lock);
	
	// Return 1 if file does not exist
	file_t temp;
	update_file_name(filename, &temp);
	file_t* f = arr_get_by_key(&temp, fs->n_list);
	if (f == NULL) {
		UNLOCK(&fs->lock);
		return 1;
	}
	
	// Return 2 if insufficient space in file_data
	if (fs->used + length - f->length > fs->file_data_len) {
		UNLOCK(&fs->lock);
		return 2;
	}
	
	// Return 0 if new length is same as old length
	if (length == f->length) {
		UNLOCK(&fs->lock);
		return 0;
	}
	
	int64_t old_length = f->length;
	int64_t hash_offset = resize_file_helper(f, length, old_length, fs);

	if (length > old_length) {
		write_null_byte(fs->file, f->offset + old_length, length - old_length);

		if (hash_offset >= 0) {
			// Hash blocks modified by repack until end of file
			compute_hash_block_range(hash_offset,
					f->offset + length - hash_offset, fs);
		} else {
			// Only hash blocks containing the file
			compute_hash_block_range(f->offset + old_length,
					length - old_length, fs);
		}
	}

	msync(fs->file, fs->file_data_len, MS_ASYNC);
	msync(fs->dir, fs->dir_table_len, MS_ASYNC);
	msync(fs->hash, fs->hash_data_len, MS_ASYNC);

	UNLOCK(&fs->lock);
	return 0;
}

/*
 * Moves file data to new offset in file_data
 *
 * file: file_t of file being moved
 * new_offset: new offset in file_data to move file contents to
 */
void repack_move(file_t* file, uint32_t new_offset, filesys_t* fs) {
	if (file->length > 0) {
		memmove(fs->file + new_offset, fs->file + file->offset, file->length);
	}

	update_file_offset(new_offset, file);
	update_dir_offset(file, fs);
}

/*
 * Helper for repacking files, independent of the filesystem lock state
 * The order of files is maintained during repack
 *
 * returns: offset of first byte modified if repack occurred, else -1
 */
int64_t repack_helper(filesys_t* fs) {
	file_t** o_list = fs->o_list->list;
	int32_t size = fs->o_list->size;
	
	// Return if no files in filesystem
	if (fs->used <= 0 || size <= 0) {
		return -1;
	}
	
	// Variable for tracking blocks to hash
	int64_t hash_offset = -1;


	// Ensure first file at offset 0
	uint64_t start_curr_file = o_list[0]->offset;
	uint64_t end_prev_file = 0;
	int is_zero_size = o_list[0]->length == 0;
	if (start_curr_file > 0) {
		repack_move(o_list[0], 0, fs);

		if (!is_zero_size) {
			hash_offset = 0;
		}
	}
	
	// Iterate over sorted offset array and move data when necessary
	for (int32_t i = 1; i < size; ++i) {
		start_curr_file = o_list[i]->offset;
		end_prev_file = o_list[i - 1]->offset + o_list[i - 1]->length;
		is_zero_size = o_list[i]->length == 0;

		// Break if newly created zero size files encountered
		if (start_curr_file >= MAX_FILE_DATA_LEN) {
			break;
		}

		if (start_curr_file > end_prev_file) {
			repack_move(o_list[i], end_prev_file, fs);
			
			if (hash_offset < 0 && !is_zero_size) {
				hash_offset = o_list[i]->offset;
			}
		}
	}
	
	return hash_offset;
}

void repack(void * helper) {
    filesys_t* fs = (filesys_t*)helper;
	LOCK(&fs->lock);
	
	int64_t hash_offset = repack_helper(fs);
	
	// Hash blocks modified during repack
	if (hash_offset >= 0) {
		compute_hash_block_range(hash_offset, fs->used - hash_offset, fs);
	}
	
	msync(fs->file, fs->file_data_len, MS_ASYNC);
	msync(fs->dir, fs->dir_table_len, MS_ASYNC);
	msync(fs->hash, fs->hash_data_len, MS_ASYNC);
	
	UNLOCK(&fs->lock);
}

int delete_file(char * filename, void * helper) {
    filesys_t* fs = (filesys_t*)helper;	
	LOCK(&fs->lock);
		
	// Return 1 if file does not exist
	file_t temp;
	update_file_name(filename, &temp);
	file_t* f = arr_get_by_key(&temp, fs->n_list);
	if (f == NULL) {
		UNLOCK(&fs->lock);
		return 1;
	}
	
	fs->used -= f->length;
	fs->index[f->index] = 0;
	--fs->index_count;

	// Remove from arrays using indices
	arr_remove(f->o_index, fs->o_list);
	arr_remove(f->n_index, fs->n_list);
	
	// Write null byte in dir_table name field
	write_null_byte(fs->dir, f->index * META_LEN, 1);
	
	free_file(f);
	
	msync(fs->dir, fs->dir_table_len, MS_ASYNC);
	
	UNLOCK(&fs->lock);
	return 0;
}

int rename_file(char * oldname, char * newname, void * helper) {
    filesys_t* fs = (filesys_t*)helper;
	LOCK(&fs->lock);
	
	file_t temp;
	update_file_name(oldname, &temp);
	file_t* f = arr_get_by_key(&temp, fs->n_list);

	// Return 0 if names are the same and oldname file exists
	if (f != NULL && strcmp(oldname, newname) == 0) {
		UNLOCK(&fs->lock);
		return 0;
	}

	// Return 1 if oldname file does not exist or newname file already exists
	update_file_name(newname, &temp);
	if (f == NULL || arr_get_by_key(&temp, fs->n_list) != NULL) {
		UNLOCK(&fs->lock);
		return 1;
	}
	
	update_file_name(newname, f);
	update_dir_name(f, fs);
	
	msync(fs->dir, fs->dir_table_len, MS_ASYNC);
	
	UNLOCK(&fs->lock);
	return 0;
}

int read_file(char * filename, size_t offset, size_t count, void * buf, void * helper) {
	filesys_t* fs = (filesys_t*)helper;

	// Filesystem mutex locked in read_file to ensure integrity of buffer
	// during parallel reads to the same buffer
	LOCK(&fs->lock);
	
	// Return 1 if file does not exist
	file_t temp;
	update_file_name(filename, &temp);
	file_t* f = arr_get_by_key(&temp, fs->n_list);
	if (f == NULL) {
		UNLOCK(&fs->lock);
		return 1;
	}
	
	// Return 2 if invalid offset and count for given file
	if (offset + count > f->length) {
		UNLOCK(&fs->lock);
		return 2;
	}
	
	// Return 3 if invalid hashes
	if (verify_hash_range(f->offset + offset, count, fs) != 0) {
		UNLOCK(&fs->lock);
		return 3;
	}
	
	// Return 0 if no bytes to read
	if (count == 0) {
		UNLOCK(&fs->lock);
		return 0;
	}
	
	memcpy(buf, fs->file + f->offset + offset, count);
	
	UNLOCK(&fs->lock);
	return 0;
}

int write_file(char * filename, size_t offset, size_t count, void * buf, void * helper) {
    filesys_t* fs = (filesys_t*)helper;
	LOCK(&fs->lock);
	
	// Return 1 if file does not exist
	file_t temp;
	update_file_name(filename, &temp);
	file_t* f = arr_get_by_key(&temp, fs->n_list);
	if (f == NULL) {
		UNLOCK(&fs->lock);
		return 1;
	}
	
	// Return 2 if offset is invalid
	if (offset > f->length) {
		UNLOCK(&fs->lock);
		return 2;
	}
	
	// Return 3 if insufficient space in file_data
	if (fs->used + offset + count - f->length > fs->file_data_len) {
		UNLOCK(&fs->lock);
		return 3;
	}
	
	// Return 0 if no bytes to write
	if (count == 0) {
		UNLOCK(&fs->lock);
		return 0;
	}
	
	// Resize if write exceeds bounds of file
	int32_t hash_offset = -1;
	if (offset + count > f->length) {
		hash_offset = resize_file_helper(f, offset + count, offset, fs);
	}
	
	memcpy(fs->file + f->offset + offset, buf, count);
	
	if (hash_offset >= 0) {
		// Hash from first repacked byte to end of used file_data
		compute_hash_block_range(hash_offset, fs->used - hash_offset, fs);
	} else {
		// Only hash the bytes modified
		compute_hash_block_range(f->offset + offset, count, fs);
	}
	
	msync(fs->file, fs->file_data_len, MS_ASYNC);
	msync(fs->dir, fs->dir_table_len, MS_ASYNC);
	msync(fs->hash, fs->hash_data_len, MS_ASYNC);
	
	UNLOCK(&fs->lock);
	return 0;
}

ssize_t file_size(char * filename, void * helper) {
    filesys_t* fs = (filesys_t*)helper;
	LOCK(&fs->lock);
	
	// Return -1 if file does not exist
	file_t temp;
	update_file_name(filename, &temp);
	file_t* f = arr_get_by_key(&temp, fs->n_list);
	if (f == NULL) {
		UNLOCK(&fs->lock);
		return -1;
	}
	
	// Return length of file
	ssize_t length = f->length;
	UNLOCK(&fs->lock);
	return length;
}

void fletcher(uint8_t * buf, size_t length, uint8_t * output) {
	assert(buf != NULL && output != NULL && "invalid args");
	
	// Casting buffer to uint32_t for reading
	uint32_t* buff = (uint32_t*)buf;
	
	// Calculate quotient and remainder for buff
	uint64_t size = length / 4;
	uint64_t rem = length % 4;
	
	uint64_t a = 0;
	uint64_t b = 0;
	uint64_t c = 0;
	uint64_t d = 0;
	for (uint64_t i = 0; i < size; ++i) {
		a = (a + buff[i]) % MAX_FILE_DATA_LEN_MINUS_ONE;
		b = (b + a) % MAX_FILE_DATA_LEN_MINUS_ONE;
		c = (c + b) % MAX_FILE_DATA_LEN_MINUS_ONE;
		d = (d + c) % MAX_FILE_DATA_LEN_MINUS_ONE;
	}
	
	// Hash last unsigned integer if required
	if (rem != 0) {
		uint32_t last = 0; // Initialised to zero for null byte padding
		memcpy(&last, buff + size, sizeof(uint8_t) * rem);
		a = (a + last) % MAX_FILE_DATA_LEN_MINUS_ONE;
		b = (b + a) % MAX_FILE_DATA_LEN_MINUS_ONE;
		c = (c + b) % MAX_FILE_DATA_LEN_MINUS_ONE;
		d = (d + c) % MAX_FILE_DATA_LEN_MINUS_ONE;
	}
	
	// Copy result to output buffer at required offset
	memcpy(output, &a, sizeof(uint32_t));
	memcpy(output + HASH_OFFSET_B, &b, sizeof(uint32_t));
	memcpy(output + HASH_OFFSET_C, &c, sizeof(uint32_t));
	memcpy(output + HASH_OFFSET_D, &d, sizeof(uint32_t));
}

/*
 * Writes the hash for the node at n_index to address out
 *
 * n_index: index of node in hash tree
 * hash_cat: address of stack array used for concatenating hashes
 * out: address that output hash is written to
 */
void hash_node(int32_t n_index, uint8_t* hash_cat, uint8_t* out, filesys_t* fs) {
	// If internal node, calculate hash of concatenated child hashes
	if (n_index < fs->leaf_offset) {
		memcpy(hash_cat, fs->hash + lc_index(n_index) * HASH_LEN, HASH_LEN);
		memcpy(hash_cat + HASH_LEN, fs->hash + rc_index(n_index) * HASH_LEN,
				HASH_LEN);
		fletcher(hash_cat, 2 * HASH_LEN, out);
		
	// Otherwise, calculate hash of file_data block for leaf node
	} else {
		fletcher(fs->file + (n_index - fs->leaf_offset) * BLOCK_LEN,
				BLOCK_LEN, out);
	}
}

void compute_hash_tree(void * helper) {
	filesys_t* fs = (filesys_t*)helper;
	LOCK(&fs->lock);
	
	// Variables for bottom-to-top level traversal of hash tree
	uint8_t* hash_addr = fs->hash;
	int32_t n_index = fs->leaf_offset;
	int32_t n_count = 0;
	int32_t nodes_in_level = n_index + 1;
	uint8_t hash_cat[2 * HASH_LEN];

	while (nodes_in_level > 0) {
		// Write hash of current node to hash_data
		hash_node(n_index, hash_cat, hash_addr + n_index * HASH_LEN, fs);
		
		if (++n_count < nodes_in_level) {
			++n_index;
		} else {
			// Move to next level once current level is completed
			nodes_in_level /= 2;
			n_index = nodes_in_level - 1;
			n_count = 0;
		}
	}

	msync(fs->hash, fs->hash_data_len, MS_ASYNC);

	UNLOCK(&fs->lock);
}

/*
 * Helper for updating hash for block at offset specified, independent of
 * filesystem lock state
 *
 * offset: index of block in file_data
 */
void compute_hash_block_helper(size_t block_offset, filesys_t* fs) {
	// Calculate the index of the leaf node for the block
	int32_t n_index = fs->leaf_offset + block_offset;
	
	// Update the leaf node hash
	fletcher(fs->file + block_offset * BLOCK_LEN, BLOCK_LEN,
			fs->hash + n_index * HASH_LEN);
	
	// Update parent node hashes all the way to the root node
	uint8_t hash_cat[2 * HASH_LEN];
	while ((n_index = p_index(n_index)) >= 0) {
		memcpy(hash_cat, fs->hash + lc_index(n_index) * HASH_LEN, HASH_LEN);
		memcpy(hash_cat + HASH_LEN, fs->hash + rc_index(n_index) * HASH_LEN,
				HASH_LEN);
		
		fletcher(hash_cat, 2 * HASH_LEN, fs->hash + n_index * HASH_LEN);
	}
}

/*
 * Update hashes for file_data blocks in the range specified
 *
 * offset: file_data offset of first byte modified
 * length: number of adjacent bytes modified
 */
void compute_hash_block_range(int64_t offset, int64_t length, filesys_t* fs) {
	// Return if length is 0
	if (length <= 0) {
		return;
	}

	assert(fs != NULL && "invalid args");
	
	// Determine first and last block modified
	int64_t first_block = offset / BLOCK_LEN;
	int64_t last_block = (offset + length - 1) / BLOCK_LEN;
	
	// Update hash tree for each block modified
	for (int64_t i = first_block; i <= last_block; ++i) {
		compute_hash_block_helper(i, fs);
	}
}

void compute_hash_block(size_t block_offset, void * helper) {
	filesys_t* fs = (filesys_t*)helper;
	LOCK(&fs->lock);
	
	compute_hash_block_helper(block_offset, fs);
	
	msync(fs->hash, fs->hash_data_len, MS_ASYNC);
	
	UNLOCK(&fs->lock);
}

/*
 * Compare hashes for blocks in the range specified
 *
 * offset: offset in file_data to start verification
 * length: number of bytes to verify
 *
 * return: 0 on success, -1 on failed verification
 */
int32_t verify_hash_range(int64_t offset, int64_t length, filesys_t* fs) {
	// Return 0 if length is 0
	if (length <= 0) {
		return 0;
	}

	assert(fs != NULL && "invalid args");
	
	// Determine first and last block to verify
	int64_t first_block = offset / BLOCK_LEN;
	int64_t last_block = (offset + length - 1) / BLOCK_LEN;
	
	// Verify hashes for each block
	int32_t n_index;
	uint8_t curr_hash[HASH_LEN];
	uint8_t hash_cat[2 * HASH_LEN];
	for (int32_t i = first_block; i <= last_block; ++i) {
		// Get leaf node index for block
		n_index = fs->leaf_offset + i;
		
		// Compare hashes from leaf to root
		while (n_index >= 0) {
			hash_node(n_index, hash_cat, curr_hash, fs);

			// Return 1 if verification failed
			if (memcmp(curr_hash,
					fs->hash + n_index * HASH_LEN, HASH_LEN) != 0) {
				return 1;
			}
			n_index = p_index(n_index);
		}
	}
	
	return 0;
}		
