#ifndef ROOM_MANAGER_H
#define ROOM_MANAGER_H

#include <pthread.h>
#include "config.h"

struct GameInstance;

#define MAX_ROOMS 7
#define MAX_PLAYERS_PER_ROOM 2
#define ROOM_NAME_LEN 15

/**
 * Enum sloužící pro popis stavu místnosti
 */
typedef enum{
    ROOM_WAITING,
    ROOM_READY,
    ROOM_PLAYING,
    ROOM_FINISHED
} RoomStatus;

/**
 * 
 */
typedef struct GameRoom{
    int room_id;
    char room_name[ROOM_NAME_LEN+1];
    RoomStatus status;
    int owner_index;

    int player_indexes[MAX_PLAYERS_PER_ROOM];
    int player_count;
    int max_players;

    int ready_players[MAX_PLAYERS_PER_ROOM];
    int ready_count;

    void* game_instance;
} GameRoom;

/** */
extern GameRoom rooms[MAX_ROOMS];
/** */
extern pthread_mutex_t rooms_mutex;

/**
 * 
 */
void initialize_rooms();

/**
 * 
 */
int start_game_in_room(int room_id);

/**
 * 
 */
int create_room(const char* room_name, int creator_index);

/**
 * 
 */
int connect_room(int room_id, int client_index);

/**
 * 
 */
int leave_room(int room_id, int client_index);

/**
 * 
 */
GameRoom* find_room(int room_id);

/**
 * 
 */
GameRoom* find_client_room(int client_index);

/**
 * 
 */
int set_player_ready(int room_id, int client_index, int ready);

/**
 * 
 */
int check_all_ready(GameRoom *room);

/**
 * 
 */
int start_game(int room_id);

/**
 * 
 */
int end_game(int room_id);

/**
 * 
 */
int delete_room(int room_id);

/**
 * 
 */
int get_room_list(char *buffer, size_t buffer_size);

/**
 * 
 */
int get_room_info(int room_id, char *buffer, size_t buffer_size);

/**
 * 
 */
void broadcast_to_room(int room_id, const char* type_msg, const char* message, int except_client_index);

#endif