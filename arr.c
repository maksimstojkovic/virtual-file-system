#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "structs.h"
#include "helper.h"
#include "arr.h"

// TODO: update _s functions with a more verbose name

/*
 * Implementation of an array that sorts file_t structs based on name or offset
 *
 * The purpose of this array was to improve the efficiency of common operations
 * such as finding a file by name, and performing a filesystem repack. An array
 * sorted by name allows a binary search algorithm to be used for both the
 * insertion of new files, and retrieval of existing files. Additionally, by
 * iterating over an array sorted by offset, repack can be streamlined, as
 * opposed to sorting files during each individual repack.
 *
 * Zero size files are assigned an offset equal to the maximum size of
 * file_data (2^32), as it allows all new zero size files to be appended to end
 * of the offset array, minimising the cost of create_file. Comparison of files
 * using a key accounts for the insertion of zero size files at the end of the
 * offset array, redirecting the binary search to lower indices.
 *
 * Although internally these files have an offset of 2^32, the offset update
 * helper for dir_table writes 0 to the dir_table file, as expected by the
 * filesystem specifications. To accommodate this, uint64_t is used instead
 * of uint32_t.
 */


/*
 * Compares the key field of file_t structs based on array type
 *
 * a: first file_t being compared
 * b: second file_t being compared
 * arr: address of array, used to determine array sorting type
 *
 * returns: -1, 0, or 1, representing the position of a relative to the
 * 			position of b
 */
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
			// (zero size files should be found by name, not offset)
			if (b->length > 0) {
				// Valid non-zero size file found
				return 0;
			} else {
				// Redirect search to lower indices for zero size files
				// Should never happen as keys are checked in other methods
				return -1;
			}
		}
	} else {
		// Only compare the first 63 characters of names
		return strncmp(a->name, b->name, NAME_LEN - 1);
	}
}

/*
 * Initialises an array with the fixed capacity and type specified
 * A fixed capacity is used as the size of dir_table does not change over time
 *
 * capacity: maximum number of file_t struct pointers in array
 * type: whether the array should be sorted by offset or name
 *
 * returns: address of dynamically allocated arr_t struct representing
 * 			the array
 * 			See structs.h for more information about arr_t fields
 */
arr_t* arr_init(int32_t capacity, TYPE type, filesys_t* fs) {
	// Check for valid arguments
	if (capacity < 0 || capacity > MAX_NUM_FILES ||
	   (type != OFFSET && type != NAME) || fs == NULL) {
		perror("arr_init: Invalid arguments");
		exit(1);
	}
	
	arr_t* arr = salloc(sizeof(*arr));
	
	arr->size = 0;
	arr->capacity = capacity;
	arr->type = type;
	arr->fs = fs;
	arr->list = salloc(sizeof(*arr->list) * capacity);
	
	return arr;
}

/*
 * Frees file_t structs pointed to by the array
 *
 * arr: address of arr_t struct containing file_t pointers to free
 */
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

/*
 * Frees arr_t struct files and file_t structs pointed to in the list
 * of the array
 *
 * arr: address of arr_t struct containing file_t pointers to free
 */
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

/*
 * Increase the index of elements from start to end by 1, creating
 * space for insertion
 *
 * start: lowest array index being shifted
 * end: highest array index being shifted
 * arr: address of arr_t struct containing a list of file_t pointers
 */
void arr_rshift(int32_t start, int32_t end, arr_t* arr) {
	TYPE type = arr->type;
	file_t** list = arr->list;
	
	// Iterate over array elements, incrementing the relevant
	// index and shifting pointers
	for (int32_t i = end; i >= start; --i) {
		if (type == OFFSET) {
			++list[i]->o_index;
		} else {
			++list[i]->n_index;
		}
		list[i + 1] = list[i];
	}
}

/*
 * Inserts file_t pointer at index specified, ensuring that elements remain
 * adjacent after insertion
 *
 * index: position in array to insert file_t pointer
 * file: file_t pointer being inserted into the array
 * arr: address of arr_t struct containing a list of file_t pointers
 *
 * returns: index on success
 */
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

/*
 * Get index for insertion of a new file_t pointer, or for an existing element
 * Binary search is used to traverse the sorted array
 * Zero size files are appended to the end of offset arrays
 *
 * file: file_t struct with the appropriate field populated for the array
 * 		 (offset if offset sorted array, or name if name sorted array)
 * arr: address of arr_t struct containing a list of file_t pointers
 * insert: whether the function should return an index for insertion, or the
 * 		   index of an existing file_t
 *
 * returns: if insert != 0 => insertion index on success, -1 if file exists
 * 			if insert == 0 => file index on success, -1 if file not found
 */
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

/*
 * Sorted insertion of a file_t struct into an array
 *
 * file: address of file_t being inserted into the array
 * arr: address of arr_t struct containing a list of file_t pointers
 *
 * returns: index on success, -1 if file exists
 */
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

/*
 * Decrease the index of elements from start to end by 1, overwriting the
 * address of a file_t pointer being removed
 *
 * start: lowest array index being shifted
 * end: highest array index being shifted
 * arr: address of arr_t struct containing a list of file_t pointers
 */
void arr_lshift(int32_t start, int32_t end, arr_t* arr) {
	TYPE type = arr->type;
	file_t** list = arr->list;
	
	// Iterate over array elements, decrementing the relevant index
	// and shifting pointers
	for (int32_t i = start; i <= end; ++i) {
		if (type == OFFSET) {
			--list[i]->o_index;
		} else {
			--list[i]->n_index;
		}
		list[i - 1] = list[i];
	}
}

/*
 * Removes file_t pointer at index specified, ensuring that elements remain
 * adjacent after removal
 * NO MEMORY IS FREED DURING THIS PROCESS
 *
 * index: position in array to insert file_t pointer
 * arr: address of arr_t struct containing a list of file_t pointers
 *
 * returns: file_t pointer removed on success
 */
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
	
	// Retrieve file_t* from array
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

/*
 * Sorted removal of a file_t struct from an array
 *
 * key: address of file_t with same key as file being removed
 * arr: address of arr_t struct containing a list of file_t pointers
 *
 * returns: removed file_t* on success
 * 			NULL if file not found or invalid key
 */
file_t* arr_remove_s(file_t* key, arr_t* arr) {
	// Check for valid arguments
	if (key == NULL || arr == NULL) {
		perror("arr_remove_s: Invalid arguments");
		exit(1);
	}

	// Check for valid key value
	if ((arr->type == OFFSET && (key->offset < 0 ||
		key->offset >= arr->fs->file_data_len)) ||
		(arr->type == NAME  && key->name[0] == '\0')) {
		return NULL;
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

/*
 * Retrieves the file_t pointer at the given index of an array
 *
 * index: position of file_t* in array
 * arr: address of arr_t struct containing a list of file_t pointers
 */
file_t* arr_get(int32_t index, arr_t* arr) {
	// Check for valid arguments
	if (index < 0 || arr == NULL || index >= arr->size) {
		perror("arr_get: Invalid arguments");
		exit(1);
	}

	return arr->list[index];
}

/*
 * Searches through the list of file_t pointers in an arr_t struct for an
 * element with a matching key value
 *
 * key: address of a file_t struct with the appropriate field for the array
 * 		type populated
 * arr: address of arr_t struct containing a list of file_t pointers
 *
 * returns: file_t* of matching file in array on success
 * 			NULL if file not found or invalid key
 */
file_t* arr_get_s(file_t* key, arr_t* arr) {
	// Check for valid arguments
	if (key == NULL || arr == NULL) {
		perror("arr_get_s: Invalid arguments");
		exit(1);
	}
	
	// Check for valid key value
	if ((arr->type == OFFSET && (key->offset < 0 ||
		key->offset >= arr->fs->file_data_len)) ||
		(arr->type == NAME  && key->name[0] == '\0')) {
		return NULL;
	}
	
	// Check if file exists (return NULL if file not found)
	int32_t index = arr_get_index(key, arr, 0);
	if (index < 0) {
		return NULL;
	}
	
	return arr_get(index, arr);
}
