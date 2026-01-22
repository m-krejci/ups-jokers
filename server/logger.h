/**
 * @file logger.h
 * @author Matyáš Krejčí (krejcim@students.zcu.cz)
 * @brief Implementace loggeru pro server
 * @version 1.0
 * @date 2025-12-26
 */


#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

// Výčtový typ levelů loggeru
typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
} log_level_t;

// Makro sloužící pro rychlý výpis do konzole
#define DLOG(fmt, ...) \
    fprintf(stderr, "[T%lu] " fmt "\n", (unsigned long)pthread_self(), ##__VA_ARGS__)

/**
 * @brief Inicializace loggeru.
 * @param filename Název souboru pro zápis
 * @param min_level Minimální úroveň loggeru
 */
int  log_init(const char *filename, log_level_t min_level);

/**
 * @brief Funkce pro řádné uzavření souboru
 */
void log_close(void);

/**
 * @brief Funkce pro makra k zápisu zprávy do souboru
 * @param level Úroveň loggovací zprávy
 * @param file Soubor pro zápis
 * @param line 
 * @param fmt
 */
void log_msg(log_level_t level,
             const char *file,
             int line,
             const char *fmt, ...);

/**
 * @brief Vymazání obsahu souboru nad definovaným loggerem
 */
void log_delete(void);

/* Makra – automaticky file + line */
#define LOG_DEBUG(...) log_msg(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  log_msg(LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  log_msg(LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) log_msg(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(...) log_msg(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)

#endif
