// Helper methods for handling synchronisation of ranges

#include "structs.h"
#include "helper.h"
#include "synclist.h"

// Lock a range in file_data
int32_t lock_range(int64_t offset, int64_t length, filesys_t* fs) {
	// Decrement rw_sem
	SEM_WAIT(&fs->rw_sem);
	
	// Claim rw_mutex
	LOCK(&fs->fs_rw_mutex);
	LOCK(&fs->rw_mutex);
	UNLOCK(&fs->fs_rw_mutex);
	
	// Sleep while required range is in use
	int32_t index;
	while ((index = check_range(offset, length, fs)) < 0) {
		COND_WAIT(&fs->rw_cond, &fs->rw_mutex);
	}
	
	// Add range to sync list
	add_range(offset, length, index, fs);
	UNLOCK(&fs->rw_mutex);
	
	// Return index of range in synclist
	return index;
}

// Unlock a range in file_data
int32_t unlock_range(int32_t index, filesys_t* fs) {
	LOCK(&fs->rw_mutex);
	
	// Remove range from synclist
	int32_t old_index = remove_range(index, fs);
	
	// Wake all sleeping r/w threads and increment rw_sem
	COND_BROADCAST(&fs->rw_cond);
	SEM_POST(&fs->rw_sem);
	
	UNLOCK(&fs->rw_mutex);
	return old_index;
}

// Block until no ranges in file_data are being used
void wait_all_ranges(filesys_t* fs) {
	// Claim and hold rw_mutex
	LOCK(&fs->fs_rw_mutex);
	LOCK(&fs->rw_mutex);
	
	// Iterate over synclist and sleep if any range is in use
	while (check_range(0, fs->len[0], fs) < 0) {
		COND_WAIT(&fs->rw_cond, &fs->rw_mutex);
	}
	
	UNLOCK(&fs->fs_rw_mutex);
	UNLOCK(&fs->rw_mutex);
}

// Check for overlapping ranges in synclist (first free index on success, -1 on overlap)
int32_t check_range (int64_t offset, int64_t length, filesys_t* fs) {
	int32_t index = -1;
	
	// Check for invalid arguments
	if (offset < 0 || length < 0) {
		perror("check_range: Invalid arguments");
		exit(1);
	}
	
	// Iterate over synclist, checking for overlap in ranges
	range_t r;
	for (int32_t i = 0; i < RW_LIMIT; i++) {
		r = fs->rw_range[i];
		
		// Update index variable for first free entry
		if (index < 0 && r.start < 0) {
			index = i;
		}
		
		// Check for range overlap
		if (r.start >= 0 && offset <= r.end && offset + length - 1 >= r.start) {
			return -1;
		}
	}
	
	return index;
}



// Add range to synclist at index given
int32_t add_range(int64_t offset, int64_t length, int32_t index, filesys_t* fs) {
	fs->rw_range[index].start = offset;
	fs->rw_range[index].end = offset + length - 1;
	return index;
}

// Remove range from synclist at index given
int32_t remove_range(int32_t index, filesys_t* fs) {
	fs->rw_range[index].start = -1;
	return index;
}
