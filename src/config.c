/*
 * Copyright (c) 2014-2026, dengbo19920415@hotmail.com
 *
 * Runtime support for FastLog.
 * Index ranges are allocated from a shared JSON file protected by flock(),
 * using the cJSON library for parsing/serialization.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <semaphore.h>

#include "cJSON.h"

static int is_pid_alive(pid_t pid) {
    if (pid <= 0)
        return 0;
    if (kill(pid, 0) == 0)
        return 1;
    return (errno == EPERM); /* exists but we lack permission → still alive */
}

static void get_process_name(char *buf, size_t buflen) {
    int fd = open("/proc/self/comm", O_RDONLY);
    if (fd >= 0) {
        ssize_t n = read(fd, buf, buflen - 1);
        close(fd);
        if (n > 0) {
            while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
                n--;
            buf[n] = '\0';
            if (n > 0)
                return;
        }
    }

    char path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len > 0) {
        path[len] = '\0';
        const char *base = strrchr(path, '/');
        base = base ? base + 1 : path;
        strncpy(buf, base, buflen - 1);
        buf[buflen - 1] = '\0';
        return;
    }

    strncpy(buf, "unknown", buflen - 1);
    buf[buflen - 1] = '\0';
}

/* -------------------------------------------------------------------------- */
/* JSON index database helpers (cJSON)                                        */
/*                                                                            */
/* Schema:                                                                    */
/* {                                                                          */
/*   "next_index": N,                                                         */
/*   "clients": {                                                             */
/*     "procname": {                                                          */
/*       "pid": [ [start, end], ... ],                                        */
/*       ...                                                                  */
/*     },                                                                     */
/*     ...                                                                    */
/*   }                                                                        */
/* }                                                                          */
/* -------------------------------------------------------------------------- */

static cJSON *load_db_from_fd(int fd) {
    struct stat st;
    if (fstat(fd, &st) < 0)
        return NULL;
    if (st.st_size == 0)
        return cJSON_CreateObject(); /* empty → fresh DB */
    if (st.st_size > 1 << 20)        /* sanity: 1 MiB */
        return NULL;

    char *buf = malloc((size_t)st.st_size + 1);
    if (!buf)
        return NULL;
    ssize_t n = pread(fd, buf, (size_t)st.st_size, 0);
    if (n != st.st_size) {
        free(buf);
        return NULL;
    }
    buf[n] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    return root; /* may be NULL on parse error */
}

static int save_db_to_fd(int fd, cJSON *root) {
    char *json = cJSON_Print(root);
    if (!json)
        return -1;
    size_t len = strlen(json);

    if (ftruncate(fd, 0) < 0) {
        cJSON_free(json);
        return -1;
    }
    ssize_t n = pwrite(fd, json, len, 0);
    cJSON_free(json);
    if (n != (ssize_t)len)
        return -1;
    fsync(fd);
    return 0;
}

/* Ensure root has "next_index" (number) and "clients" (object). */
static void ensure_schema(cJSON *root) {
    if (!cJSON_GetObjectItem(root, "next_index"))
        cJSON_AddNumberToObject(root, "next_index", 0);
    if (!cJSON_GetObjectItem(root, "clients"))
        cJSON_AddObjectToObject(root, "clients");
}

/*
 * Walk every client → every pid.  Delete entries whose process is dead.
 * Uses a two-pass approach because cJSON_DeleteItemFromObject invalidates
 * iteration if we delete while walking.
 */
static void prune_dead_pids(cJSON *root) {
    cJSON *clients = cJSON_GetObjectItem(root, "clients");
    if (!cJSON_IsObject(clients))
        return;

    cJSON *client;
    cJSON_ArrayForEach(client, clients) {
        if (!cJSON_IsObject(client))
            continue;

        /* Collect dead pid keys first */
        char *dead[128];
        int n_dead = 0;

        cJSON *pid_item;
        cJSON_ArrayForEach(pid_item, client) {
            if (n_dead >= (int)(sizeof(dead) / sizeof(dead[0])))
                break;
            pid_t pid = (pid_t)atoi(pid_item->string);
            if (!is_pid_alive(pid))
                dead[n_dead++] = pid_item->string;
        }

        for (int i = 0; i < n_dead; i++)
            cJSON_DeleteItemFromObject(client, dead[i]);
    }
}

/*
 * Allocate a contiguous index range of `size` slots.
 * The allocation is recorded under the current process name + pid
 * in the shared JSON file.  Dead PIDs belonging to the same process
 * name (and all other clients) are pruned first.
 *
 * Returns the start index (>= 0) or -1 on error.
 */
int fastlog_get_idx_base_from_config(int size, char *index_file_name) {
    if (size <= 0)
        return -1;

    char proc_name[64];
    get_process_name(proc_name, sizeof(proc_name));
    pid_t my_pid = getpid();
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d", (int)my_pid);

    char index_sem_name[64];
    snprintf(index_sem_name, sizeof(index_sem_name) - 1, "/%s_sem", index_file_name);
    sem_t *alloc_lock = sem_open(index_sem_name, O_CREAT, 0666, 1);
    if (alloc_lock == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }

    sem_wait(alloc_lock);

    char index_file_path[64];
    snprintf(index_file_path, sizeof(index_sem_name) - 1, "/tmp/%s", index_file_name);
    int fd = open(index_file_path, O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        fprintf(stderr, "fastlog: open index file \"%s\": %s\n", index_file_path, strerror(errno));
        sem_post(alloc_lock);
        return -1;
    }

    cJSON *root = load_db_from_fd(fd);
    if (!root) {
        /* Corrupt or unreadable → start fresh */
        root = cJSON_CreateObject();
        if (!root) {
            close(fd);
            sem_post(alloc_lock);
            return -1;
        }
    }
    ensure_schema(root);

    /* 1. Prune dead PIDs */
    prune_dead_pids(root);

    /* 2. Locate / create our client object */
    cJSON *clients = cJSON_GetObjectItem(root, "clients");
    cJSON *client = cJSON_GetObjectItem(clients, proc_name);
    if (!client) {
        client = cJSON_CreateObject();
        cJSON_AddItemToObject(clients, proc_name, client);
    }

    /* 3. Locate / create our pid array */
    cJSON *pid_arr = cJSON_GetObjectItem(client, pid_str);
    if (!pid_arr) {
        pid_arr = cJSON_CreateArray();
        cJSON_AddItemToObject(client, pid_str, pid_arr);
    }

    /* 4. Allocate from the high-water mark */
    cJSON *next_item = cJSON_GetObjectItem(root, "next_index");
    int start = next_item ? next_item->valueint : 0;
    int end = start + size;
    if (end < start) { /* overflow */
        cJSON_Delete(root);
        close(fd);
        sem_post(alloc_lock);
        return -1;
    }

    /* Append [start, end] */
    cJSON *range = cJSON_CreateArray();
    cJSON_AddItemToArray(range, cJSON_CreateNumber(start));
    cJSON_AddItemToArray(range, cJSON_CreateNumber(end));
    cJSON_AddItemToArray(pid_arr, range);

    /* Update next_index */
    cJSON_SetNumberValue(next_item, end);

    /* 5. Persist */
    if (save_db_to_fd(fd, root) < 0) {
        fprintf(stderr, "fastlog: failed to write index file\n");
        cJSON_Delete(root);
        close(fd);
        sem_post(alloc_lock);
        return -1;
    }

    cJSON_Delete(root);
    close(fd);
    sem_post(alloc_lock);
    sem_close(alloc_lock);
    return start;
}