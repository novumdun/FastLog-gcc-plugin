#ifndef __SHARED_MUTEX_H__
#define __SHARED_MUTEX_H__

typedef struct shared_mutex {
    int shm_fd;
    void *shm;
} shared_mutex_t;

extern int shared_mutex_init(shared_mutex_t *shared_mutex, char *name);
extern int shared_mutex_deinit(shared_mutex_t *shared_mutex);
extern int shared_mutex_lock(shared_mutex_t *shared_mutex);
extern int shared_mutex_unlock(shared_mutex_t *shared_mutex);

#endif
