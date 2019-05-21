#include <pthread.h>

#include "structs.h"
#include "lock.h"

// Initialise lock
lock_t* init_lock(lock_t* lock) {
	pthread_mutex_init(&lock->w_mutex, NULL);
	pthread_mutex_init(&lock->r_mutex, NULL);
	pthread_cond_init(&lock->w_cond, NULL);
	lock->r_count = 0;
	return lock;
}

// Destory lock
void destory_lock(lock_t* lock) {
	pthread_mutex_destroy(&lock->w_mutex);
	pthread_mutex_destroy(&lock->r_mutex);
	pthread_cond_destroy(&lock->w_cond);
}

// Increment read monitor
void read_lock(lock_t* lock) {
	LOCK(&lock->w_mutex);
	LOCK(&lock->r_mutex);
	UNLOCK(&lock->w_mutex);
	lock->r_count++;
	UNLOCK(&lock->r_mutex);
}

// Decrement read monitor and signal any pending writes
void read_unlock(lock_t* lock) {
	LOCK(&lock->r_mutex);
	lock->r_count--;
	COND_SIGNAL(&lock->w_cond);
	UNLOCK(&lock->r_mutex);
}

// Lock write mutex to prevent additional reads and wait for reads to finish
void write_lock(lock_t* lock) {
	LOCK(&lock->w_mutex);
	LOCK(&lock->r_mutex);
	
	while (lock->r_count > 0) {
		COND_WAIT(&lock->w_cond, &lock->r_mutex);
	}
	
	UNLOCK(&lock->r_mutex);
}

// Unlock write mutex, allowing for read/write operations
void write_unlock(lock_t* lock) {
	UNLOCK(&lock->w_mutex);
}
