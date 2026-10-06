#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>

static FILE *log_file = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline void log_init(const char *path) {
    log_file = fopen(path, "a");
    if (!log_file) perror("fopen log");
}

// Igual que printf, pero con timestamp y escribe en consola y en el archivo
static inline void log_msg(const char *fmt, ...) {
    char ts[32];
    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_info);

    pthread_mutex_lock(&log_mutex);

    va_list args;
    va_start(args, fmt);
    printf("[%s] ", ts);
    vprintf(fmt, args);
    va_end(args);

    if (log_file) {
        va_start(args, fmt);
        fprintf(log_file, "[%s] ", ts);
        vfprintf(log_file, fmt, args);
        va_end(args);
        fflush(log_file);
    }

    pthread_mutex_unlock(&log_mutex);
}

#endif
