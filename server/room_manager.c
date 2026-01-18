#include "room_manager.h"
#include "client_manager.h"
#include  "game_manager.h"
#include "logger.h"
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

GameRoom rooms[MAX_ROOMS];
pthread_mutex_t rooms_mutex = PTHREAD_MUTEX_INITIALIZER;

void initialize_rooms(){
    pthread_mutex_lock(&rooms_mutex);

    for(int i = 0; i < MAX_ROOMS; i++){
        rooms[i].room_id = -1;          // neaktivní místnost
        rooms[i].room_name[0] = '\0';      // prázdný řetězec
        rooms[i].status = ROOM_WAITING;
        rooms[i].owner_index = -1;
        rooms[i].player_count = 0;
        rooms[i].max_players = MAX_PLAYERS_PER_ROOM;
        rooms[i].ready_count = 0;
        rooms[i].game_instance = NULL;
        
        for (int j = 0; j < MAX_PLAYERS_PER_ROOM; j++){
            rooms[i].player_indexes[j] = -1;
            rooms[i].ready_players[j] = 0;
        }
    }
    pthread_mutex_unlock(&rooms_mutex);
    LOG_INFO("Místnosti nainicializovány\n");
}

int start_game_in_room(int room_id){
    GameRoom *room = find_room(room_id);
    if(!room){
        return -1;
    }

    if(room->status != ROOM_READY){
        LOG_ERROR("Místnost není připravena\n");
        return -1;
    }

    GameInstance *game = game_create(room);
    if(!game){
        return -1;
    }

    room->game_instance = game;
    room->status = ROOM_PLAYING;

    if(game_start(game) != 0){
        game_destroy(game);
        room->game_instance = NULL;
        return -1;
    }
    return 0;
}

int create_room(const char* room_name, int creator_index) {
    // 1. Validace vstupů
    if (!room_name || strlen(room_name) == 0 || strlen(room_name) > ROOM_NAME_LEN) {
        return -1;
    }

    pthread_mutex_lock(&rooms_mutex);

    // 2. Nalezení volného slotu pro místnost
    int room_id = -1;
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].room_id == -1) {
            room_id = i;
            break;
        }
    }

    // Pokud není volné místo v poli místností
    if (room_id == -1) {
        pthread_mutex_unlock(&rooms_mutex);
        return -1;
    }

    // 3. Inicializace nalezené místnosti
    GameRoom *room = &rooms[room_id];
    
    // Nastavení základních údajů
    room->room_id = room_id;
    strncpy(room->room_name, room_name, ROOM_NAME_LEN);
    room->room_name[ROOM_NAME_LEN] = '\0';
    room->owner_index = creator_index;
    room->status = ROOM_WAITING;
    room->max_players = MAX_PLAYERS_PER_ROOM;
    room->game_instance = NULL;

    // !!! KLÍČOVÉ: Vyčištění polí pro hráče (prevence "Chyby ready") !!!
    for (int j = 0; j < MAX_PLAYERS_PER_ROOM; j++) {
        room->player_indexes[j] = -1;
        room->ready_players[j] = 0;
    }

    // 4. Přidání zakladatele do místnosti
    room->player_indexes[0] = creator_index;
    room->player_count = 1;
    room->ready_count = 0; // Zakladatel začíná jako NOT READY (musí kliknout)

    pthread_mutex_unlock(&rooms_mutex);

    // Logování pro debugování na serveru
    LOG_INFO("Vytvořena nová místnost: %s (ID: %d)\n", room->room_name, room->room_id);
    LOG_INFO("Vlastník (index %d) zapsán do slotu 0 místnosti %d\n", creator_index, room->room_id);

    // VRACÍME room_id, aby jej client_manager mohl poslat zpět klientovi
    return room_id; 
}

int connect_room(int room_id, int client_index){
    if(room_id < 0 || room_id >= MAX_ROOMS){
        return -1;
    }

    pthread_mutex_lock(&rooms_mutex);

    GameRoom *room = &rooms[room_id];

    if (room->room_id == -1){
        pthread_mutex_unlock(&rooms_mutex);
        LOG_ERROR("Chyba: Místnost %d neexistuje\n", room_id);
        return -2;
    }

    if(room->status != ROOM_WAITING){
        pthread_mutex_unlock(&rooms_mutex);
        LOG_ERROR("Chyba: Hra již začala\n");
        return -3;
    }

    if(room->player_count >= room->max_players){
        pthread_mutex_unlock(&rooms_mutex);
        LOG_ERROR("Chyba: Místnost plná\n");
        return -4;
    }

    for(int i = 0; i < MAX_PLAYERS_PER_ROOM; i++){
        if(room->player_indexes[i] == client_index){
            pthread_mutex_unlock(&rooms_mutex);
            LOG_ERROR("Chyba: Klient %d už je v místnosti\n", client_index);
            return -5;
        }
    }

    for(int i = 0; i < MAX_PLAYERS_PER_ROOM; i++){
        if(room->player_indexes[i] == -1){
            room->player_indexes[i] = client_index;
            room->ready_players[i] = 0;
            room->player_count++;
            break;
        }
    }

    pthread_mutex_unlock(&rooms_mutex);

    LOG_INFO("Klient %d připojen k místnosti %d (%d/%d)\n", client_index, room_id, room->player_count, room->max_players);

    return room_id;
}

int leave_room(int room_id, int client_index){
    if (room_id < 0 || room_id >= MAX_ROOMS){
        return -1;
    }

    pthread_mutex_lock(&rooms_mutex);

    GameRoom *room = &rooms[room_id];

    if (room->room_id == -1){
        pthread_mutex_unlock(&rooms_mutex);
        LOG_ERROR("Chyba: místnost %d neexistuje\n", room_id);
        return -1;
    }

    int found = 0;
    for(int i = 0; i < MAX_PLAYERS_PER_ROOM; i++){
        if(room->player_indexes[i] == client_index){
            room->player_indexes[i] = -1;
            if(room->ready_players[i]){
                room->ready_count--;
            }
            room->ready_players[i] = 0;
            room->player_count--;
            found = 1;
            break;
        }
    }

    if(!found){
        pthread_mutex_unlock(&rooms_mutex);
        LOG_ERROR("Chyba: Klient %d nebyl v místnosti %d nalezen\n", client_index, room_id);
        return -1;
    }

    LOG_INFO("Klient %d opustil místnost %d (%d/%d)\n", client_index, room_id, room->player_count, room->max_players);

    LOG_INFO("Místnost %d : player count == %d\n", room_id, room->player_count);
    if(room->player_count == 0){
        LOG_INFO("Místnost %d je prázdná -- mažu\n", room_id);
        room->room_id = -1;
        room->room_name[0] = '\0';
        pthread_mutex_unlock(&rooms_mutex);
        return 0;
    }

    if(room->owner_index == client_index){
        for(int i = 0; i < MAX_PLAYERS_PER_ROOM; i++){
            if(room->player_indexes[i] != -1){
                room->owner_index = room->player_indexes[i];
                LOG_INFO("Nový vlastník mísnosti %d: %d\n", room_id, room->owner_index);
                break;
            }
        }
    }
    pthread_mutex_unlock(&rooms_mutex);
    return 0;
}

GameRoom* find_room(int room_id){
    if(room_id < 0 || room_id >= MAX_ROOMS){
        return NULL;
    }

    if(rooms[room_id].room_id == -1){
        return NULL;
    }

    return &rooms[room_id];
}

GameRoom* find_client_room(int client_index){
    pthread_mutex_lock(&rooms_mutex);

    for(int i = 0; i < MAX_ROOMS; i++){
        if(rooms[i].room_id == -1){
            continue;
        }

        for(int j = 0; j< MAX_PLAYERS_PER_ROOM; j++){
            if(rooms[i].player_indexes[j] == client_index){
                pthread_mutex_unlock(&rooms_mutex);
                return &rooms[i];
            }
        }
    }
    pthread_mutex_unlock(&rooms_mutex);
    return NULL;
}

int set_player_ready(int room_id, int client_index, int ready){
    if(room_id < 0 || room_id >= MAX_ROOMS){
        return -1;
    }

    pthread_mutex_lock(&rooms_mutex);
    GameRoom *room = &rooms[room_id];

    if(room->room_id == -1){
        pthread_mutex_unlock(&rooms_mutex);
        return -1;
    }

    for(int i = 0; i < MAX_PLAYERS_PER_ROOM; i++){
        LOG_INFO("Ready: %d | %d\n", room->player_indexes[i], client_index);
        if(room->player_indexes[i] == client_index){
            int was_ready = room->ready_players[i];
            room->ready_players[i] = ready ? 1 : 0;

            if(was_ready && !ready){
                room->ready_count--;
            }else if(!was_ready && ready){
                room->ready_count++;
            }
            LOG_INFO("Stav: %d, Ready: %d\n", was_ready, room->ready_count);
            LOG_INFO("Klient %d v místnosti %d: %s (ready: %d/%d)\n",
            client_index, room_id, ready? "READY" : "NOT READY", room->ready_count, room->player_count);

            pthread_mutex_unlock(&rooms_mutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&rooms_mutex);
    return -1;
}

int check_all_ready(GameRoom *room){
    if(!room || room->player_count < 2){
        return 0;
    }

    return room->ready_count == room->player_count;
}

int start_game(int room_id){
    if(room_id < 0 || room_id >= MAX_ROOMS){
        return -1;
    }

    pthread_mutex_lock(&rooms_mutex);

    GameRoom *room = &rooms[room_id];

    if(room->room_id == -1){
        pthread_mutex_unlock(&rooms_mutex);
        return -1;
    }

    if(room->status != ROOM_WAITING && room->status != ROOM_READY){
        pthread_mutex_unlock(&rooms_mutex);
        LOG_ERROR("Chyba: Hra již běží nebo skončila\n");
        return -1;
    }

    if(!check_all_ready(room)){
        pthread_mutex_unlock(&rooms_mutex);
        LOG_ERROR("Chyba: Ne všichni hráči jsou ready\n");
        return -1;
    }

    room->status = ROOM_PLAYING;

    pthread_mutex_unlock(&rooms_mutex);
    LOG_INFO("Hra začíná v místnosti %d\n", room_id);
    return 0;
}

int end_game(int room_id){
    if(room_id < 0 || room_id >= MAX_ROOMS){
        return -1;
    }
    pthread_mutex_lock(&rooms_mutex);

    GameRoom *room = &rooms[room_id];

    if(room->room_id == -1){
        pthread_mutex_unlock(&rooms_mutex);
        return -1;
    }

    room->status = ROOM_FINISHED;
    pthread_mutex_unlock(&rooms_mutex);

    LOG_INFO("Hra v místnosti %d skončila\n", room_id);
    return 0;
}

int delete_room(int room_id){
    if(room_id < 0 || room_id >= MAX_ROOMS){
        return -1;
    }

    pthread_mutex_lock(&rooms_mutex);

    GameRoom *room = &rooms[room_id];

    if(room->room_id == -1){
        pthread_mutex_unlock(&rooms_mutex);
        return -1;
    }

    if(room->player_count > 0){
        pthread_mutex_unlock(&rooms_mutex);
        LOG_ERROR("Chyba: Místnost %d není prázdná\n", room_id);
        return -1;
    }

    room->room_id = -1;
    room->room_name[0] = '\0';

    pthread_mutex_unlock(&rooms_mutex);
    LOG_INFO("Místnost %d smazána\n", room_id);

    return 0;
}

int get_room_list(char *buffer, size_t buffer_size){
    if(!buffer || buffer_size == 0){
        return 0;
    }

    pthread_mutex_lock(&rooms_mutex);

    buffer[0] = '\0';
    int count = 0;

    for(int i = 0; i < MAX_ROOMS; i++){
        if(rooms[i].room_id == -1){
            continue;
        }

        char room_line[256];
        const char *status_str;

        switch(rooms[i].status){
            case ROOM_WAITING:
                status_str = "W";
                break;
            case ROOM_READY: 
                status_str = "R";
                break;
            case ROOM_PLAYING:
                status_str = "P";
                break;
            case ROOM_FINISHED:
                status_str = "F";
                break;
            default:
                status_str = "U";
                break;
        }

        snprintf(room_line, sizeof(room_line), "%d|%s|(%d/%d)|%s,\n", 
        rooms[i].room_id, rooms[i].room_name, rooms[i].player_count, rooms[i].max_players, status_str);

        if(strlen(buffer) + strlen(room_line) < buffer_size -1){
            strcat(buffer, room_line);
            count++;
        }else{
            break;
        }
    }
    pthread_mutex_unlock(&rooms_mutex);
    return count;
}

int get_room_info(int room_id, char *buffer, size_t buffer_size){
    if(!buffer || buffer_size == 0){
        return -1;
    }

    if(room_id < 0 || room_id >= MAX_ROOMS){
        return -1;
    }

    // ✅ Zamkni ve správném pořadí
    pthread_mutex_lock(&clients_mutex);
    pthread_mutex_lock(&rooms_mutex);

    GameRoom *room = &rooms[room_id];

    if(room->room_id == -1){
        pthread_mutex_unlock(&rooms_mutex);
        pthread_mutex_unlock(&clients_mutex);
        return -1;
    }

    char temp[256];
    temp[0] = '\0';
    int offset = 0;
    
    // offset += snprintf(temp, sizeof(temp), "%d|%s|", room_id, room->room_name);
    
    for(int i = 0; i < MAX_PLAYERS_PER_ROOM; i++){
        int client_index = room->player_indexes[i];
        if(client_index != -1 && client_index < MAX_CLIENTS){
            offset += snprintf(temp + offset, sizeof(temp) - offset, 
                             "%s|%s|%s,", 
                             clients[client_index].nick, 
                             room->ready_players[i] ? "READY" : "NOT READY",
                             (client_index == room->owner_index) ? "OWNER" : "GUEST");
        } 
    }
    
    strncpy(buffer, temp, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';

    pthread_mutex_unlock(&rooms_mutex);
    pthread_mutex_unlock(&clients_mutex);

    return strlen(buffer);
}

void broadcast_to_room(int room_id, const char* type_msg, const char* message, int except_client_index){
    if(room_id < 0 || room_id > MAX_ROOMS){
        return;
    }

    pthread_mutex_lock(&rooms_mutex);
    pthread_mutex_lock(&clients_mutex);

    GameRoom *room = &rooms[room_id];

    if(room->room_id == -1){
        pthread_mutex_unlock(&clients_mutex);
        pthread_mutex_unlock(&rooms_mutex);
        
        return;
    }

    for(int i = 0; i < MAX_PLAYERS_PER_ROOM; i++){
        int client_index = room->player_indexes[i];

        if(client_index != -1 && client_index != except_client_index && client_index < MAX_CLIENTS){
            int sock = clients[client_index].socket_fd;

            if(sock > 0){
                send_message(sock, type_msg, message);
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    pthread_mutex_unlock(&rooms_mutex);
}