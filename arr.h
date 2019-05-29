#ifndef ARR_H
#define ARR_H

#include "structs.h"

int32_t cmp_key(file_t* a, file_t* b, arr_t* arr);

arr_t* arr_init(int32_t capacity, TYPE type, filesys_t* fs);

void free_arr_list(arr_t* arr);

void free_arr(arr_t* arr);

void arr_rshift(int32_t start, int32_t end, arr_t* arr);

int32_t arr_insert(int32_t index, file_t* file, arr_t* arr);

int32_t arr_get_index(file_t* file, arr_t* arr, int32_t insert);

int32_t arr_insert_s(file_t* file, arr_t* arr);

void arr_lshift(int32_t start, int32_t end, arr_t* arr);

file_t* arr_remove(int32_t index, arr_t* arr);

file_t* arr_remove_s(file_t* key, arr_t* arr);
	
file_t* arr_get(int32_t index, arr_t* arr);

file_t* arr_get_s(file_t* key, arr_t* arr);

//TODO: REMOVE
void arr_print(arr_t* arr);

#endif
