#ifndef ARR_H
#define ARR_H

#include "structs.h"

// Compares key of file_t based on array type (a relative to b)
int32_t cmp_key(file_t* a, file_t* b, arr_t* arr);

// Initialise array with fixed capacity (dir_table has fixed length)
arr_t* arr_init(int32_t capacity, TYPE type, filesys_t* fs);

// Free file_t* elements in list of array
void free_arr_list(arr_t* arr);

// Free arr_t struct and file_t structs within the array
void free_arr(arr_t* arr);

// Increase index of elements from index start to end - 1
void arr_rshift(int32_t start, int32_t end, arr_t* arr);

// Insert file_t* at index, ensuring adjacent elements (index success)
int32_t arr_insert(int32_t index, file_t* file, arr_t* arr);

// Get index for insert or for existing element depending on insert argument
// Binary search algorithm used to determine index to return
// insert != 0 - insert index on success, -1 file exists
// insert == 0 - file index on success, -1 file not found
// NOTE: Zero size files are appended to the end of the offset array
int32_t arr_get_index(file_t* file, arr_t* arr, int32_t insert);

// Insert file_t* into array with sorting (index success, -1 file exists)
int32_t arr_insert_s(file_t* file, arr_t* arr);

// Decrease index of elements from index start to end - 1
void arr_lshift(int32_t start, int32_t end, arr_t* arr);

// Remove file_t* at index, keeping elements adjacent (file_t* success)
// NOTE: NO MEMORY IS FREE'D DURING THIS PROCESS
file_t* arr_remove(int32_t index, arr_t* arr);

// Remove file_t* with relevant key from array (file_t* success, NULL file not found)
file_t* arr_remove_s(file_t* key, arr_t* arr);
	
// Return file_t* at given index in arr
file_t* arr_get(int32_t index, arr_t* arr);

// Search through array for element with matching key value
// (file_t* success, NULL file not found)
file_t* arr_get_s(file_t* key, arr_t* arr);

// Print array contents in sorted order (used for debugging)
void arr_print(arr_t* arr);

#endif
