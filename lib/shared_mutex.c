#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdatomic.h>
#include <time.h>

#include "shared_mutex.h"

typedef struct {
    pthread_mutex_t mutex;
    atomic_int ready; // 0:not initialized   1:ready
    int counter;
} shared_data_t;

static int open_existing(shared_mutex_t *shared_mutex, char *name) {
    int shm_fd = -1;
    shm_fd = shm_open(name, O_RDWR, 0);
    if (shm_fd == -1) {
        perror("shm_open");
        return errno;
    }

    shared_data_t *shared_data = NULL;
    shared_data = mmap(NULL, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_data == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        return errno;
    }

    while (atomic_load_explicit(&shared_data->ready, memory_order_acquire) == 0) {
        usleep(1000); /* 1 ms */
    }

    shared_mutex->shm_fd = shm_fd;
    shared_mutex->shm = shared_data;
    return 0;
}

int shared_mutex_init(shared_mutex_t *shared_mutex, char *name) {
    int shm_fd = -1;
    shm_fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0666);
    if (shm_fd == -1) {
        if (errno == EEXIST) {
            return open_existing(shared_mutex, name);
        }
        return errno;
    }

    if (ftruncate(shm_fd, sizeof(shared_data_t)) == -1) {
        perror("ftruncate");
        close(shm_fd);
        shm_unlink(name);
        return errno;
    }

    shared_data_t *shared_data = NULL;
    shared_data = mmap(NULL, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_data == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        shm_unlink(name);
        return errno;
    }

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
    /* pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE); */

    if (pthread_mutex_init(&shared_data->mutex, &attr) != 0) {
        perror("pthread_mutex_init");
        munmap(shared_data, sizeof(*shared_data));
        close(shm_fd);
        shm_unlink(name);
        return errno;
    }
    pthread_mutexattr_destroy(&attr);

    shared_data->counter = 0;
    atomic_store_explicit(&shared_data->ready, 1, memory_order_release);

    shared_mutex->shm_fd = shm_fd;
    shared_mutex->shm = shared_data;
    return 0;
}

int shared_mutex_deinit(shared_mutex_t *shared_mutex) {
    shared_data_t *shared_data = shared_mutex->shm;
    if (shared_data != NULL) {
        munmap(shared_data, sizeof(*shared_data));
        shared_data = NULL;
    }

    int shm_fd = shared_mutex->shm_fd;
    if (shm_fd != -1) {
        close(shm_fd);
        shm_fd = -1;
    }
    return 0;
}

int shared_mutex_lock(shared_mutex_t *shared_mutex) {
    shared_data_t *shared_data = shared_mutex->shm;
    int ret = pthread_mutex_lock(&shared_data->mutex);
    if (ret == EOWNERDEAD) {
        printf("[PID %d] Recovered from owner death\n", getpid());
        pthread_mutex_consistent(&shared_data->mutex);
    } else if (ret != 0) {
        fprintf(stderr, "[PID %d] lock error: %s\n", getpid(), strerror(ret));
        return ret;
    }
    return 0;
}

int shared_mutex_unlock(shared_mutex_t *shared_mutex) {
    shared_data_t *shared_data = shared_mutex->shm;
    return pthread_mutex_unlock(&shared_data->mutex);
}
