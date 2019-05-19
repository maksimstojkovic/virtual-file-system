#ifndef ARR_H
#define ARR_H

#include "structs.h"

// Compares key of file_t based on array type (a relative to b)
int cmp_key(file_t* a, file_t* b, arr_t* arr);

// Initialise array with given capacity and filesys_t*
// Array is fixed in size (dir_table has fixed length)
arr_t* arr_init(int32_t capacity, TYPE type, filesys_t* fs);

// Free entire arr_t struct
void free_arr(arr_t* arr);

// Insert file_t* at index, ensuring adjacent elements (0 success)
int arr_insert(int32_t index, file_t* file, arr_t* arr);

// Insert file_t* into array with sorting (index success, -1 file exists)
int32_t arr_insert_s(file_t* file, arr_t* arr);

// Remove file_t* at index, keeping elements adjacent (file_t* success)
// NOTE: NO MEMORY IS FREE'D DURING THIS PROCESS
file_t* arr_remove(int32_t index, arr_t* arr);

// TODO: EDGE CASE WITH FILE AT OFFSET fs->len[0] with size 0, should succeed?
// Remove file_t* with relevant key from array(file_t* success, NULL file not found)
file_t* arr_remove_s(file_t* key, arr_t* arr);
	
// Return file_t* at given index in arr
file_t* arr_get(int32_t index, arr_t* arr);

// Search through array for element with matching key
// Assumes the file passed has the valid key populated
// (file_t* success, NULL file not found)
file_t* arr_get_s(file_t* file, arr_t* arr);

// Print array contents in sorted order
void arr_print(arr_t* arr);

#endif
