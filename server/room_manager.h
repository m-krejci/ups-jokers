#ifndef ROOM_MANAGER_H
#define ROOM_MANAGER_H

#include <pthread.h>
#include "config.h"

struct GameInstance;

#define MAX_ROOMS 7
#define MAX_PLAYERS_PER_ROOM 2
#define ROOM_NAME_LEN 15

/**
 * @brief Enum sloužící pro popis stavu místnosti
 */
typedef enum{
    ROOM_WAITING,           // Místnost čeká na hráče -- všechny nevytvořené místnosti
    ROOM_READY,             // Místnost je připravena ke hře
    ROOM_PLAYING,           // Místnost hraje
    ROOM_FINISHED           // Místnost dohrála
} RoomStatus;

/**
 * @brief Struktura udržující herní místnost
 */
typedef struct GameRoom{
    int room_id;                                    // Identifikátor místnosti
    char room_name[ROOM_NAME_LEN+1];                // Název místnosti
    RoomStatus status;                              // Status místnosti
    int owner_index;                                // Index zakladatele místnosti

    int player_indexes[MAX_PLAYERS_PER_ROOM];       // Indexy hráčů v místnosti
    int player_count;                               // Počet hráčů v místnosti
    int max_players;                                // Maximální počet hráčů v místnosti

    int ready_players[MAX_PLAYERS_PER_ROOM];        // Pole připravených hráčů
    int ready_count;                                // Číslo připravených hráčů
    
    void* game_instance;                            // Ukazatel na herní instanci
} GameRoom;

/** Pole všech existujících místností */
extern GameRoom rooms[MAX_ROOMS];                      
/** Mutex pro místnosti */
extern pthread_mutex_t rooms_mutex;

/**
 * @brief Základní inicializace místností
 */
void initialize_rooms();

/**
 * @brief Spouští hru, pokud jsou všichni hráči připraveni
 * @param room_id Identifikátor místnosti
 * @return 0 - SUCCESS, -1 - ERROR
 */
int start_game_in_room(int room_id);

/**
 * @brief Vytvoření místnosti s názvem zadaným uživatelem
 * @param room_name Název místnosti zadané uživatelem
 * @param creator_index Index klienta, který místnost zakládá
 * @return room_id - SUCCESS, -1 - ERROR
 */
int create_room(const char* room_name, int creator_index);

/**
 * @brief Umožňuje připojení k místnosti
 * @param room_id Identifikátor místnosti
 * @param client_index Index uživatele, který se chce připojit
 * @return -1: nevalidní identifikátor, -2: neexistující místnost, -3: Místnost již hraje, -4: Místnost plná, -5: Klient je v místnosti, room_id: SUCCESS
 */
int connect_room(int room_id, int client_index);

/**
 * @brief Umožňuje opustit aktuálně připojenou místnost
 * @param room_id Identifikátor místnosti
 * @param client_index Index uživatele, který se odpojuje
 * @returns -1: ERROR, 0 SUCCESS
 */
int leave_room(int room_id, int client_index);

/**
 * @brief Hledá, jestli existuje místnost s požadovaným indexem a vrací ji, pokud ano
 * @param room_id Identifikátor místnosti, která je požadována
 * @return NULL POINTER : ERROR, GameRoom instance: SUCCESS
 */
GameRoom* find_room(int room_id);

/**
 * @brief Hledá místnost na základě uživatelského indexu
 * @param client_index Index klienta
 * @return GameRoom: SUCCESS, NULL : ERROR
 */
GameRoom* find_client_room(int client_index);

/**
 * @brief Nastavuje hráče na stav připravený
 * @param room_id Identifikátor místnosti
 * @param client_index Klientský index
 * @param ready Binární hodnota -> 0 = unready, 1 = ready
 * @return -1: ERROR, 0 SUCCESS
 */
int set_player_ready(int room_id, int client_index, int ready);

/**
 * @brief Kontroluje, zda-li jsou všichni v místnosti připraveni ke hře
 * @param room Ukazatel na místnost
 * @return 0 - not all ready, 1 - all ready
 */
int check_all_ready(GameRoom *room);

/**
 * @brief Spouští hru
 * @param room_id Identifikátor místnosti
 * @return -1: ERROR, 0: SUCCESS
 */
int start_game(int room_id);

/**
 * @brief Ukončuje hru
 * @param room_id Identifikátor místnosti
 * @return -1: ERROR, 0: SUCCESS
 */
int end_game(int room_id);

/**
 * @brief Maže vytvořenou místnost
 * @param room_id Identifikátor místnosti
 * @return -1: ERROR, 0: SUCCESS
 */
int delete_room(int room_id);

/**
 * @brief Do textového buffer formátuje zprávu o místnostech pro klienty
 * @param buffer Buffer pro textovou zprávu
 * @param buffer_size Velikost bufferu
 * @return -1: ERROR, 0: SUCCESS
 */
int get_room_list(char *buffer, size_t buffer_size);

/**
 * @brief Shromažďuje informace o místnosti
 * @param room_id Identifikátor místnosti
 * @param buffer Buffer pro shromážděné informace
 * @param buffer_size Velikost bufferu
 * @return -1: ERROR, 0: SUCCESS
 */
int get_room_info(int room_id, char *buffer, size_t buffer_size);

/**
 * @brief Rozesílá zprávu všem v místnosti.
 * @param room_id Identifikátor místnosti
 * @param type_msg Typ zprávy z výčtového typu protocolu
 * @param message Zpráva pro klienta
 * @param except_client_index -1 pro všechny klienty, jinak index klienta, komu zprávu neposílá
 */
void broadcast_to_room(int room_id, const char* type_msg, const char* message, int except_client_index);

#endif