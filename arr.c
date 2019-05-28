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
	// Check for valid arguments
	if (a == NULL || b == NULL || arr == NULL) {
		perror("cmp_key: Invalid arguments");
		exit(1);
	}
	
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
			if (b->length > 0) {
				return 0; // Valid non-zero size file found
			} else {
				return -1; // Redirect search to lower indices if zero size file found
			}
		}
	} else {
		// Only compare the first 63 characters of names
		return strncmp(a->name, b->name, NAME_LEN - 1);
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
	
	arr_t* arr = salloc(sizeof(*arr));
	
	arr->size = 0;					// Number of elements in array
	arr->capacity = capacity;		// Total capacity of array
	arr->type = type;				// Type that array is sorted by
	arr->fs = fs;					// Reference to filesystem
	arr->list =						// Array elements
		salloc(sizeof(*arr->list) * capacity);
	
	return arr;
}

// Free file_t* elements in list of array
void free_arr_list(arr_t* arr) {
	// Check for valid arguments
	if (arr == NULL) {
		perror("free_arr_list: Invalid arguments");
		exit(1);
	}
	
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
	// Check for valid arguments
	if (arr == NULL) {
		perror("free_arr: Invalid arguments");
		exit(1);
	}
	
	free_arr_list(arr);
	free(arr->list);
	free(arr);
}

// Increase index of elements from index start to end
void arr_rshift(int32_t start, int32_t end, arr_t* arr) {
	TYPE type = arr->type;
	file_t** list = arr->list;
	
	// Iterate over array elements, incrementing the relevant index and shifting pointers
	for (int32_t i = end; i >= start; --i) {
		if (type == OFFSET) {
			++list[i]->o_index;
		} else {
			++list[i]->n_index;
		}
		list[i + 1] = list[i];
	}
}

// Insert file_t* at index, ensuring adjacent elements (index success)
int32_t arr_insert(int32_t index, file_t* file, arr_t* arr) {
	// Check for valid arguments
	if (file == NULL || arr == NULL || index < 0 || index > arr->size) {
		perror("list_insert: Invalid arguments");
		exit(1);
	}
	if (index < 0 || index > arr->size) {
		perror("list_insert: Invalid index");
		exit(1);
	}
	if (arr->size >= arr->capacity) {
		perror("list_insert: List full");
		exit(1);
	}
	
	// Shift elements to the right (higher index) if required
	if (index < arr->size) {
		arr_rshift(index, arr->size - 1, arr);
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
// Binary search algorithm used to determine index to return
// insert != 0 - insert index on success, -1 file exists
// insert == 0 - file index on success, -1 file not found
// NOTE: Zero size files are appended to the end of the offset array
int32_t arr_get_index(file_t* file, arr_t* arr, int32_t insert) {
	// Check for valid arguments
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
	
	// Index variables for low, high and middle index of binary search
	int32_t l = 0;
	int32_t h = size - 1;
	int32_t m = 0;
	int32_t cmp = 0;
	
	// Loop until file_t found, or low and high converge with the middle index
	while (1) {
		m = (l + h) / 2;
		cmp = cmp_key(file, list[m], arr);
		
		// TODO: REMOVE
		// printf("%d %d %d %d %s\n", l, h, m, cmp, m_file->name);
		
		if (cmp < 0) {
			// Check for low and middle index convergence
			if (l >= m) {
				if (insert) {
					return m; // Insert at current index
				} else {
					return -1; // File not found
				}
			}
			
			// Update high index for next iteration
			h = m - 1;
			
		} else if (cmp > 0) {
			// Check for high and middle index convergence
			if (h <= m) {
				if (insert) {
					return m + 1; // Insert at next index
				} else {
					return -1; // File not found
				}
			}
			
			// Update low index for next iteration
			l = m + 1;
			
		} else {
			if (insert) {
				return -1; // File exists
			} else {
				return m; // Return index of file found
			}
		}
	}
}

// Insert file_t* into array with sorting (index success, -1 file exists)
int32_t arr_insert_s(file_t* file, arr_t* arr) {
	// Check for valid arguments
	if (file == NULL || arr == NULL) {
		perror("arr_insert_s: Invalid arguments");
		exit(1);
	}
	if (arr->size >= arr->capacity) {
		perror("list_insert: List full");
		exit(1);
	}
	
	// Find insertion index
	int32_t index = arr_get_index(file, arr, 1);
	if (index < 0) {
		return -1; // File exists
	}

	arr_insert(index, file, arr);
	return index;
}

// Decrease index of elements from index start to end
void arr_lshift(int32_t start, int32_t end, arr_t* arr) {
	TYPE type = arr->type;
	file_t** list = arr->list;
	
	// Iterate over array elements, decrementing the relevant index and shifting pointers
	for (int32_t i = start; i <= end; ++i) {
		if (type == OFFSET) {
			--list[i]->o_index;
		} else {
			--list[i]->n_index;
		}
		list[i - 1] = list[i];
	}
}

// Remove file_t* at index, keeping elements adjacent (file_t* success)
// NOTE: NO MEMORY IS FREE'D DURING THIS PROCESS
file_t* arr_remove(int32_t index, arr_t* arr) {
	// Check for valid arguments
	if (arr == NULL || index < 0 || index >= arr->size) {
		perror("list_remove: Invalid arguments");
		exit(1);
	}
	if (index < 0 || index >= arr->size) {
		perror("list_remove: Invalid index");
		exit(1);
	}
	if (arr->size <= 0) {
		perror("list_insert: List empty");
		exit(1);
	}
	
	// Retrieve file_t* from array (assumed valid from checks above)
	file_t* f = arr_get(index, arr);
	
	// Shift elements to the left (lower index) if required
	if (index < arr->size - 1) {
		arr_lshift(index + 1, arr->size - 1, arr);
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
	// Check for valid arguments
	if (key == NULL || arr == NULL) {
		perror("arr_remove_s: Invalid arguments");
		exit(1);
	}

	// Check for valid key value
	if ((arr->type == OFFSET && (key->offset < 0 || key->offset >= arr->fs->len[0])) ||
		(arr->type == NAME  && key->name[0] == '\0')) {
		perror("arr_remove_s: Invalid key");
		exit(1);
	}
	
	// Find file_t index in array (return NULL if file not found)
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
	// Check for valid arguments
	if (index < 0 || arr == NULL || index >= arr->size) {
		perror("arr_get: Invalid arguments");
		exit(1);
	}

	return arr->list[index];
}

// Search through array for element with matching key value
// (file_t* success, NULL file not found)
file_t* arr_get_s(file_t* key, arr_t* arr) {
	// Check for valid arguments
	if (key == NULL || arr == NULL) {
		perror("arr_get_s: Invalid arguments");
		exit(1);
	}
	
	// Check for valid key value
	if ((arr->type == OFFSET && (key->offset < 0 || key->offset >= arr->fs->len[0])) ||
		(arr->type == NAME  && key->name[0] == '\0')) {
		perror("arr_remove_s: Invalid key");
		exit(1);
	}
	
	// Check if file exists (return NULL if file not found)
	int32_t index = arr_get_index(key, arr, 0);
	if (index < 0) {
		return NULL;
	}
	
	return arr_get(index, arr);
}

//TODO: REMOVE HEADER AND CODE
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

