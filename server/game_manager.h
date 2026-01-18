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

typedef enum{
    GAME_STATE_LOBBY,
    GAME_STATE_STARTING,
    GAME_STATE_PLAYING,
    GAME_STATE_PAUSED,
    GAME_STATE_FINISHED
} GameState;

typedef struct {
    int id;
    char name[2];
    int value;
    char suit[2];
    int is_joker;
    char code[3];
} Card;

typedef struct {
    Card cards[MAX_SEQUENCE_CARDS];
    int count;
    int owner_client_index;
} CardSequence;

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

typedef void (*SendToPlayerCallback)(int client_index, const char* msg_type, const char *message);
typedef void (*BroadcastToRoomCallback)(int room_id, const char* msg_type, const char* message, int except_index);
typedef void (*NotifyRoomCallback)(int room_id, const char* event_type, const char* data);

typedef struct{
    SendToPlayerCallback send_to_player;
    BroadcastToRoomCallback broadcast_to_room;
    NotifyRoomCallback notify_room;
} GameCallbacks;

void game_init();

GameInstance* game_create(GameRoom *room);

void game_destroy(GameInstance *game);

int game_start(GameInstance *game);

int game_process_move(GameInstance *game, int client_index, const char* action, const char* message_body);

int game_end_round(GameInstance *game);

int game_end(GameInstance *game);

int game_pause(GameInstance *game, const char* reason);

int game_resume(GameInstance *game);

int game_get_player_cards(GameInstance *game, int client_index, char* buffer, size_t buffer_size);

int game_get_player_state(GameInstance *game, int client_index, char* buffer, size_t buffer_size);

int game_get_full_state(GameInstance *game, int client_index, char *buffer, size_t buffer_size);

int game_validate_move(GameInstance *game, int client_index, const char* action);

int game_check_timeout(GameInstance *game);

int game_disconnect_handle(GameInstance *game, int client_index);

int game_reconnect_handle(GameInstance *game, int client_index);

void game_init_deck(GameInstance *game);

void game_deal_cards(GameInstance *game);

void game_next_player(GameInstance *game);

void game_calculate_scores(GameInstance *game);

int game_is_finished(GameInstance *game);

#endif