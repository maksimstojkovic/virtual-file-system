#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "structs.h"
#include "helper.h"
#include "arr.h"

/*
	Implementation of an Array that is Sorted by Offset or Name
	
	Zero size files are assigned and offset equal to the 2^32, hence
	uint64_t is used in file_t structs as this value exceeds the limit of
	uint32_t. Using this offset allows zero size files to be appended to the end
	of the array, increasing insertion efficiency, whilst all writes to
	the offset field of the file in dir_table will have value 0.
*/

// Compares key of file_t based on array type (a relative to b)
int32_t cmp_key(file_t* a, file_t* b, arr_t* arr) {
	if (arr->type == OFFSET) {
		if (a->offset < b->offset) {
			return -1;
		} else if (a->offset > b->offset) {
			return 1;
		} else {
			// Check if file_t* b refers to a zero size file
			// Zero size files have an offset that exceeds the bounds
			// of file_data, and are stored at the end of the array
			// (zero size files should be found by name, not offset)
			if (b->offset < arr->fs->len[0]) {
				return 0;
			} else {
				// Redirect search to lower indices if zero size file found
				return -1;
			}
		}
	} else {
		// Only compare the first 63 characters of names
		return strncmp(a->name, b->name, NAME_LENGTH - 1); 
	}
}

// Initialise array with fixed capacity (dir_table has fixed length)
arr_t* arr_init(int32_t capacity, TYPE type, filesys_t* fs) {
	// Check for valid arguments
	if (capacity < 0 || capacity > MAX_NUM_FILES ||
	   (type != OFFSET && type != NAME) || fs == NULL) {
		perror("arr_init: Invalid arguments");
		exit(1);
	}
	
	// Allocate memory for array
	arr_t* arr = salloc(sizeof(*arr));
	
	// Initialise array variables
	arr->size = 0;					// Number of elements in array
	arr->capacity = capacity;		// Max capacity of array
	arr->type = type;				// Type that the array is sorted by
	arr->fs = fs;					// Filesystem referencing the array
	arr->list =						// Array of file_t*
		salloc(sizeof(*arr->list) * capacity);
	return arr;
}

// Free file_t* elements in list of array
static void free_arr_list(arr_t* arr) {
	if (arr->size > 0) {
		file_t** list = arr->list;
		for (int32_t i = 0; i < arr->size; i++) {
			free_file(list[i]);
		}
		// Prevent double free
		arr->fs->o_list->size = 0;
		arr->fs->n_list->size = 0;
	}
}

// Free arr_t struct and file_t structs within the array
void free_arr(arr_t* arr) {
	free_arr_list(arr);
	free(arr->list);
	free(arr);
}

// Increase index of elements from index start to end - 1
static void arr_rshift(int32_t start, int32_t end, arr_t* arr) {
	TYPE type = arr->type;
	file_t** list = arr->list;
	
	for (int32_t i = end-1; i >= start; i--) {
		if (type == OFFSET) {
			list[i]->o_index++;
		} else {
			list[i]->n_index++;
		}
		list[i+1] = list[i];
	}
}

// Insert file_t* at index, ensuring adjacent elements (index success)
int32_t arr_insert(int32_t index, file_t* file, arr_t* arr) {
	if (file == NULL || arr == NULL || index < 0 || index > arr->size) {
		perror("list_insert: Invalid arguments");
		exit(1);
	}
	if (arr->size >= arr->capacity) {
		perror("list_insert: List full");
		exit(1);
	}
	
	// Shift elements to the right (higher index) if required
	if (index < arr->size) {
		arr_rshift(index, arr->size, arr);
	}
	
	// Update list and file_t variables
	arr->list[index] = file;
	++arr->size;
	if (arr->type == OFFSET) {
		file->o_index = index;
	} else {
		file->n_index = index;
	}
	return index;
}

// Get index for insert or for existing element depending on insert argument
// insert != 0 - insert index success, -1 file exists
// insert == 0 - file index success, -1 file not found
// NOTE: Zero size files are appended to the end of the offset array
static int32_t arr_get_index(file_t* file, arr_t* arr, int32_t insert) {
	if (file == NULL || arr == NULL) {
		perror("arr_get_index: Invalid arguments");
		exit(1);
	}
	
	// Cases when array is empty
	if (arr->size == 0) {
		if (insert) {
			return 0;
		} else {
			return -1;
		}
	}
	
	// Append zero size_files to end of offset array
	if (insert && arr->type == OFFSET && file->length == 0) {
		return arr->size;
	}
	
	int32_t size = arr->size;
	file_t** list = arr->list;
	
	for (int32_t i = 0; i < size; ++i) {
		if (cmp_key(file, list[i], arr) < 0) {
			// Expected offset or name position passed
			if (insert) {
				return i;
			} else {
				return -1;
			}
		} else if (cmp_key(file, arr->list[i], arr) == 0) {
			// Offset or name collision
			if (insert) {
				return -1;
			} else {
				return i;
			}
		}
	}
	
	if (insert) {
		return size;
	} else {
		return -1;
	}
}

// Insert file_t* into array with sorting (index success, -1 file exists)
int32_t arr_insert_s(file_t* file, arr_t* arr) {
	if (file == NULL || arr == NULL) {
		perror("arr_insert_s: Invalid arguments");
		exit(1);
	}
	if (arr->size >= arr->capacity) {
		perror("list_insert: List full");
		exit(1);
	}
	
	// Find insertion index (return -1 if file exists)
	int32_t index = arr_get_index(file, arr, 1);
	if (index < 0) {
		return -1;
	}

	arr_insert(index, file, arr);
	return index;
}

// TODO: Convert to parallel helper function (add thread id and rightmost file_t* to struct)
// Decrease index of elements from index start to end - 1
static void arr_lshift(int32_t start, int32_t end, arr_t* arr) {
	TYPE type = arr->type;
	file_t** list = arr->list;
	
	for (int32_t i = start; i < end; i++) {
		if (type == OFFSET) {
			list[i]->o_index--;
		} else {
			list[i]->n_index--;
		}
		list[i-1] = list[i];
	}
}

// Remove file_t* at index, keeping elements adjacent (file_t* success)
// NOTE: NO MEMORY IS FREE'D DURING THIS PROCESS
file_t* arr_remove(int32_t index, arr_t* arr) {
	if (arr == NULL || index < 0 || index >= arr->size) {
		perror("list_remove: Invalid arguments");
		exit(1);
	}
	if (arr->size <= 0) {
		perror("list_insert: List empty");
		exit(1);
	}
	
	// Retrieve file_t* from array (assumed valid from checks above)
	file_t* f = arr_get(index, arr);
	
	// Shift elements to the left (lower index) if required
	if (index < arr->size-1) {
		arr_lshift(index+1, arr->size, arr);
	}
	
	// Update list and file_t variables
	arr->size--;
	if (arr->type == OFFSET) {
		f->o_index = -1;
	} else {
		f->n_index = -1;
	}
	return f;
}

// Remove file_t* with relevant key from array (file_t* success, NULL file not found)
file_t* arr_remove_s(file_t* key, arr_t* arr) {
	if (key == NULL || arr == NULL) {
		perror("arr_remove_s: Invalid arguments");
		exit(1);
	}

	// Zero length files will fail the offset check
	if ((arr->type == OFFSET && (key->offset < 0 || key->offset >= arr->fs->len[0])) ||
		(arr->type == NAME  && key->name[0] == '\0')) {
		perror("arr_remove_s: Invalid key");
		exit(1);
	}
	
	// Find insertion index (return NULL if file not found)
	file_t* f = arr_get_s(key, arr);
	if (f == NULL) {
		return NULL;
	}

	// Remove depending on type of array
	if (arr->type == OFFSET) {
		arr_remove(f->o_index, arr);
	} else {
		arr_remove(f->n_index, arr);
	}
	return f;
}

// Return file_t* at given index in arr
file_t* arr_get(int32_t index, arr_t* arr) {
	if (index < 0 || arr == NULL || index >= arr->size) {
		perror("arr_get: Invalid arguments");
		exit(1);
	}

	return arr->list[index];
}

// Search through array for element with matching key
// Assumes the file passed has the valid key populated
// (file_t* success, NULL file not found)
file_t* arr_get_s(file_t* file, arr_t* arr) {
	if (file == NULL || arr == NULL) {
		perror("arr_get_s: Invalid arguments");
		exit(1);
	}
	
	// Check if file exists (return NULL if file not found)
	int32_t index = arr_get_index(file, arr, 0);
	if (index < 0) {
		return NULL;
	}
	
	return arr_get(index, arr);
}

// Print array contents in sorted order (used for debugging)
void arr_print(arr_t* arr) {
	for (int32_t i = 0; i < arr->size; i++) {
		file_t* f = arr_get(i, arr);
		if (arr->type == OFFSET) {
			printf("OFFSET[%d] - n:\"%s\" o:%lu l:%u dir:%d\n", i, f->name, f->offset, f->length, f->index);
		} else {
			printf("  NAME[%d] - n:\"%s\" o:%lu l:%u dir:%d\n", i, f->name, f->offset, f->length, f->index);
		}
	}
}

