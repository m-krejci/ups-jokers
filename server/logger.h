#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
} log_level_t;

#define DLOG(fmt, ...) \
    fprintf(stderr, "[T%lu] " fmt "\n", (unsigned long)pthread_self(), ##__VA_ARGS__)

int  log_init(const char *filename, log_level_t min_level);
void log_close(void);

void log_msg(log_level_t level,
             const char *file,
             int line,
             const char *fmt, ...);

void log_delete(void);

/* Makra – automaticky file + line */
#define LOG_DEBUG(...) log_msg(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  log_msg(LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  log_msg(LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) log_msg(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(...) log_msg(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)

#endif
