#include "logger.h"
#include <pthread.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>

static FILE *log_file = NULL;
static log_level_t min_log_level;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static const char *level_str[] = {
    "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

int log_init(const char *filename, log_level_t min_level)
{
    // Pokud existuje soubor resp. lze vytvorit -> načti do modu append (nepřepisuj původní)
    if (filename)
        log_file = fopen(filename, "a");
    else
        log_file = stdout;

    if (!log_file)
        return -1;

    min_log_level = min_level;
    return 0;
}

void log_close(void)
{
    if (log_file && log_file != stdout)
        fclose(log_file);
}

void log_msg(log_level_t level,
             const char *file,
             int line,
             const char *fmt, ...)
{
    if (level < min_log_level)
        return;

    pthread_mutex_lock(&log_mutex);

    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);

    char timebuf[32];
    strftime(timebuf, sizeof(timebuf),
             "%Y-%m-%d %H:%M:%S", &tm);

    fprintf(log_file,
        "[%s] [%s] [TID:%lu] %s:%d: ",
        timebuf,
        level_str[level],
        (unsigned long)pthread_self(),
        file,
        line);

    va_list args;
    va_start(args, fmt);
    vfprintf(log_file, fmt, args);
    va_end(args);

    fprintf(log_file, "\n");
    fflush(log_file);

    pthread_mutex_unlock(&log_mutex);
}

void log_delete(void)
{
    if (!log_file || log_file == stdout) {
        return;
    }

    pthread_mutex_lock(&log_mutex);

    int fd = fileno(log_file);
    if (fd != -1) {
        ftruncate(fd, 0);            
        fseek(log_file, 0, SEEK_SET);
    }

    pthread_mutex_unlock(&log_mutex);
}