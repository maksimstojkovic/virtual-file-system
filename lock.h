#ifndef LOCK_H
#define LOCK_H

#include "structs.h"

// Initialise lock
lock_t* init_lock(lock_t* lock);

// Destory lock
void destory_lock(lock_t* lock);

// Increment read monitor
void read_lock(lock_t* lock);

// Decrement read monitor and signal any pending writes
void read_unlock(lock_t* lock);

// Lock write mutex to prevent additional reads and wait for reads to finish
void write_lock(lock_t* lock);

// Unlock write mutex, allowing for read/write operations
void write_unlock(lock_t* lock);

#endif
