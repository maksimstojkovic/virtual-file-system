#ifndef SYNCLIST_H
#define SYNCLIST_H

#include "structs.h"

// Lock a range in file_data
int32_t lock_range(int64_t offset, int64_t length, filesys_t* fs);

// Unlock a range in file_data
int32_t unlock_range(int32_t index, filesys_t* fs);

// Block until no ranges in file_data are being used
void wait_all_ranges(filesys_t* fs);

// Check for overlapping ranges in synclist (first free index on success, -1 on overlap)
int32_t check_range (int64_t offset, int64_t length, filesys_t* fs);

// Add range to synclist at index given
int32_t add_range(int64_t offset, int64_t length, int32_t index, filesys_t* fs);

// Remove range from synclist at index given
int32_t remove_range(int32_t index, filesys_t* fs);

#endif
