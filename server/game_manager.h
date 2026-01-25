#ifndef GAME_MANAGER_H
#define GAME_MANAGER_H

#include "config.h"
#include <pthread.h>

#define MAX_ROOM_NAME 10
#define MAX_ROOM_PLAYERS 2
#define MAX_HAND_CARD 15
#define DECK_CARDS_COUNT 108
#define MAX_SEQUENCE_CARDS 15
#define MAX_SEQUENCES 50


typedef struct GameRoom GameRoom;
struct ClientContext;

// Enum pro stavy hry
typedef enum{
    GAME_STATE_LOBBY,
    GAME_STATE_STARTING,
    GAME_STATE_PLAYING,
    GAME_STATE_PAUSED,
    GAME_STATE_FINISHED
} GameState;

// Struktura pro uchování karty
typedef struct {
    int id;
    char name[2];
    int value;
    char suit[2];
    int is_joker;
    char code[3];
} Card;

// Struktura pro uchování postupek a setů (vyložených karet)
typedef struct {
    Card cards[MAX_SEQUENCE_CARDS];
    int count;
    int owner_client_index;
} CardSequence;

// Struktura pro uchování herního stavu uživatele
typedef struct {
    int client_index;
    int score;
    int position;
    int is_active;

    int takes_15;

    Card hand[MAX_HAND_CARD];
    int hand_count;

    int turns_played;
    int cards_played;

    int is_ready_for_next_round;
    int did_thrown;
    int took_card;
    int did_closed;
} PlayerGameState;

// Struktura hry
typedef struct{
    int room_id;
    GameState state;

    PlayerGameState players[MAX_ROOM_PLAYERS];
    int player_count;
    int current_player_index;

    Card deck[DECK_CARDS_COUNT];
    int deck_count;

    Card discard_deck[DECK_CARDS_COUNT];
    int discard_count;

    CardSequence sequences[MAX_SEQUENCES];
    int sequence_count;

    time_t round_start_time;
    time_t turn_start_time;
    int turn_timeout_seconds;

    void* event_log;
} GameInstance;

// Callbacky nevyužity 
typedef void (*SendToPlayerCallback)(int client_index, const char* msg_type, const char *message);
typedef void (*BroadcastToRoomCallback)(int room_id, const char* msg_type, const char* message, int except_index);
typedef void (*NotifyRoomCallback)(int room_id, const char* event_type, const char* data);

typedef struct{
    SendToPlayerCallback send_to_player;
    BroadcastToRoomCallback broadcast_to_room;
    NotifyRoomCallback notify_room;
} GameCallbacks;

/**
 * @brief Základní inicializace aktivních her
 */
void game_init();

/**
 * @brief Vytvoření hry v místnosti
 * @param room Instance místnosti
 * @return Instance na hru, NULL
 */
GameInstance* game_create(GameRoom *room);

/**
 * @brief Odstranění hry
 * @param game Instance odstraňované hry
 */
void game_destroy(GameInstance *game);

/**
 * @brief Spustí hru v místnosti, pokud je to možné
 * @param game Instance na hru
 * @return -1: ERROR, 0: SUCCESS
 */
int game_start(GameInstance *game);

/**
 * @brief Kontroluje, jestli tah hráčem je validní
 * @param game Instance na hru
 * @param client_index Klientský index
 * @param action Vykonávaná akce
 * @param message_body Tělo akce (karty)
 * @return <0: ERROR, 0: validní tah
 */
int game_process_move(GameInstance *game, int client_index, const char* action, const char* message_body);

/**
 * @brief
 * @param game
 * @return
 */
int game_end_round(GameInstance *game);

/**
 * @brief Ukončuje násilně hru (pro budoucí implementaci)
 * @param game Instace na hru
 * @return 0
 */
int game_end(GameInstance *game);

/**
 * @brief V případě výpadku jednoho z klientů pozastavuje hru
 * @param game Instance hry
 * @param reason Důvod pozastavení
 * @return -1: ERROR, 0: SUCCESS
 */
int game_pause(GameInstance *game, const char* reason);

/**
 * @brief Obnovuje hru při připojení klienta zpět do hry
 * @param game Instance na hru
 * @return -1: ERROR, 0: SUCCESS
 */
int game_resume(GameInstance *game);

/**
 * @brief Ze struktury načte řetězec karet hráče do bufferu
 * @param game Instance hry
 * @param client_index Klientský index (hráč)
 * @param buffer Buffer pro zprávu
 * @param buffer_size Velikost bufferu
 * @return -1: ERROR, int: Počet zapsaných znaků: SUCCESS
 */
int game_get_player_cards(GameInstance *game, int client_index, char* buffer, size_t buffer_size);

/**
 * @brief Vrací state hráče
 * @param game Instance na hru
 * @param client_index Klientský index (hráč)
 * @param buffer Buffer pro state
 * @param buffer_size Velikost bufferu
 * @return -1: ERROR, 1: SUCCESS
 */
int game_get_player_state(GameInstance *game, int client_index, char* buffer, size_t buffer_size);

/**
 * @brief Formátuje stav hry pro klienty a předává informace o kartách v ruce, vyhozené kartě, počet karet protihráče a tah nebo opak
 * @param game Instance hry
 * @param client_index Klientský index (hráč)
 * @param buffer Buffer pro zprávu (řetězec)
 * @param buffer_size Velikost bufferu
 * @return -1: ERROR, int: Velikost zprávy: SUCCESS
 */
int game_get_full_state(GameInstance *game, int client_index, char *buffer, size_t buffer_size);

/**
 * @brief Původně funkce pro demodulaci funkce process_move (pro budoucí užití)
 * @param game Instance hry
 * @param client_index Klientský index
 * @param action Tah hry
 * @return 0
 */
int game_validate_move(GameInstance *game, int client_index, const char* action);

/**
 * @brief Funkce pro budoucí implementaci
 * @param game Instance na hru
 * @return 0
 */
int game_check_timeout(GameInstance *game);

/**
 * @brief Pro budoucí implementaci
 * @param game Instance na hru
 * @param client_index Klientský index
 * @return 0
 */
int game_disconnect_handle(GameInstance *game, int client_index);

/**
 * @brief Navrácení hráče do hry po reconnectu
 * @param game Instance na hru
 * @param client_index Klientský index
 * @return -1: ERROR, 0: SUCCESS
 */
int game_reconnect_handle(GameInstance *game, int client_index);

/**
 * @brief Inicializace hrního balíčku, zamíchání karet
 * @param game Instance na hru
 */
void game_init_deck(GameInstance *game);

/**
 * @brief Rozdání karet uživatelům
 * @param game Instance na hru
 */
void game_deal_cards(GameInstance *game);

/**
 * @brief Přepnutí hráče na tahu
 * @param game Instance na hru
 */
void game_next_player(GameInstance *game);

/**
 * @brief Výpočet skore zbylých karet v rukách hráčů
 * @param game Instance na hru
 */
void game_calculate_scores(GameInstance *game);

/**
 * @brief Kontrola ukončení hry
 * @param game Instance na hru
 */
int game_is_finished(GameInstance *game);

#endif