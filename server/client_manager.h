#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H

#include <pthread.h>
#include "protocol.h"
#include "room_manager.h"

#define HEARTBEAT_TIMEOUT 10
#define RECONNECT_TIMEOUT 120

// Enum o stavech hráče (klienta)
typedef enum {DISCONNECTED,
              CONNECTED,
              IN_ROOM,
              ON_WAIT,
              ON_TURN,
              PAUSED,
              GAME_DONE
} PlayerStatus;


// Struktura udržování dat pro konkrétního hráče (klienta)
typedef struct{
    int socket_fd;                          // Socket klienta
    int player_id;                          // Herní ID klienta
    char nick[NICK_LEN+1];                  // Nick klienta
    PlayerStatus status;                    // Status klienta
    int is_active;                          // Status aktivnosti klienta
    int invalid_message_count;              // Counter nevalidních zpráv klienta
    time_t last_heartbeat;                  // Heartbeat pro pingování klienta
    time_t disconnect_time;                 // Čas odpojení
    int is_connected;                       // Stav připojení
    GameRoom *current_room;                 // Momentální místnost klienta
    PlayerStatus last_status;               // Poslední stav klienta
    char token[11];                         // token pro reconnect
} ClientContext;

// Kontext klienta pro klientské vlákno
typedef struct{
    int socket_fd;
    int client_index;
} ThreadContext;

// Pole zaregistrovaných klientů
extern ClientContext clients[MAX_CLIENTS];
// Mutex pro přístup do pole
extern pthread_mutex_t clients_mutex;

/**
 * @brief Inicializace pole klientů, provede základní nastavení pole, může mít nekonečněkrát modifikováno
 */
void initialize_clients();

/**
 * @brief Odstranění klienta z pole klientů
 * @param client_socket Socket klienta
 */
void remove_client(int client_socket);


/**
 * @brief Mozek serveru, člení herní status klienta, kontroluje zprávy a podle nich odesílá instrukce
 * @param arg Obsahuje context klienta pro klientské vlákno (socket a index)
 */
void *client_handler(void* arg);

/**
 * @brief Hledá hráče v poli všech hráčů na základě nicku
 * @param nick Přezdívka uživatele
 * @return
 */
int find_player_by_nick(const char* nick);

/**
 * @brief 
 */
void check_client_timeouts();

#endif 