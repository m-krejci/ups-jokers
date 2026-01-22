#include "client_manager.h"
#include "room_manager.h"
#include "game_manager.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h> // pro shutdown

// #include "game_manager.h"



ClientContext clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void initialize_clients(){
    pthread_mutex_lock(&clients_mutex);
    
    // Inicializuj všechny sloty jako prázdné
    for(int i = 0; i < MAX_CLIENTS; i++) {
        memset(&clients[i], 0, sizeof(ClientContext));
        clients[i].socket_fd = -1;
        clients[i].player_id = -1;
        clients[i].is_connected = 0;
        clients[i].is_active = 0;
        clients[i].status = DISCONNECTED;
        clients[i].last_heartbeat = 0;
        clients[i].disconnect_time = 0;
    }
    
    pthread_mutex_unlock(&clients_mutex);
    LOG_INFO("Klienti nainicializováni (MAX_CLIENTS=%d)\n", MAX_CLIENTS);
}

void remove_client(int client_socket){
    pthread_mutex_lock(&clients_mutex);
    // odstraní klienta z paměti
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i].socket_fd == client_socket) {
            close(clients[i].socket_fd);
            clients[i].socket_fd = -1;
            clients[i].is_connected = 0;
            clients[i].is_active = 0;
            clients[i].disconnect_time = time(NULL);
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

int find_player_by_nick(const char* nick){
    for(int i = 0; i < MAX_CLIENTS; i++){
        if(clients[i].nick[0] != '\0' && strcmp(clients[i].nick, nick) == 0){
            // printf("%d\n",clients[i].socket_fd);
            return i;
        }
    }

    return -1;
}

void check_client_timeouts(){
    time_t now = time(NULL);

    pthread_mutex_lock(&clients_mutex);
    for(int i = 0; i < MAX_CLIENTS; i++){
        if(clients[i].socket_fd == -1 && !clients[i].is_active && clients[i].nick[0] == '\0'){
            continue;
        }

        if(clients[i].is_connected){
            if(clients[i].socket_fd >= 0){
                send_message(clients[i].socket_fd, "PING", "");
            }

            if (now - clients[i].last_heartbeat > HEARTBEAT_TIMEOUT) {
    LOG_INFO("Klient '%s' timeout (heartbeat) - odpojuji \n", clients[i].nick);

    DLOG("HB TIMEOUT idx=%d nick='%s' fd=%d is_conn=%d last_hb=%ld now=%ld",
         i, clients[i].nick, clients[i].socket_fd, clients[i].is_connected,
         (long)clients[i].last_heartbeat, (long)now);

    // --- pod mutexem si "vezmu" starý socket a označím stav ---
    int oldfd = clients[i].socket_fd;
    GameRoom *room = clients[i].current_room; // jen pointer, nepřistupovat mimo mutex k jeho vnitřku, pokud není thread-safe

    clients[i].socket_fd = -1;
    clients[i].is_connected = 0;
    clients[i].disconnect_time = now;
    clients[i].is_active = 0;

    // (volitelné) uložit last_status apod., pokud to používáš
    clients[i].last_status = clients[i].status;

    // --- mimo mutex udělám IO operace (send/shutdown/close/broadcast) ---
    pthread_mutex_unlock(&clients_mutex);

    if (oldfd >= 0) {
        // pokud chceš poslat info, pošli ho PŘED shutdown,
        // ale počítej s tím, že může selhat (klient už může být pryč)
        send_message(oldfd, LBBY, "Ztraceno spojení (heartbeat)");

        // klíčové: probudit recv() ve starém klientském vlákně
        shutdown(oldfd, SHUT_RDWR);
        close(oldfd);
    }

    if (room) {
        // broadcast mimo clients_mutex (jak už děláš)
        broadcast_to_room(room->room_id, PAUS, "Protihráč se odpojil", -1);

        // pokud game_pause není thread-safe vůči dalším akcím, řeš to vlastním mutexem hry/room
        if (room->game_instance) {
            game_pause((GameInstance*)room->game_instance, "Protihráč se odpojil");
        }
    }

    pthread_mutex_lock(&clients_mutex);
}
        }
         else {
            if(clients[i].is_active){
                continue;
            }
            if (now - clients[i].disconnect_time > RECONNECT_TIMEOUT) {
                LOG_INFO("Mažu data hráče '%s' (reconnect timeout)\n", clients[i].nick);

                if (clients[i].current_room) {
                    GameRoom *room = clients[i].current_room;
                    int room_id = room->room_id;

                    if (room->game_instance) {
                        pthread_mutex_unlock(&clients_mutex);
                        broadcast_to_room(room_id, LBBY, "Protihráč se nestihl znovu připojit. Hra končí.", i);
                        pthread_mutex_lock(&clients_mutex);
                        
                        game_destroy((GameInstance*)room->game_instance);
                        room->game_instance = NULL;
                        room->status = ROOM_WAITING;
                        room->player_count--;
                        delete_room(room->room_id);
                    }

                    for (int j = 0; j < MAX_PLAYERS_PER_ROOM; j++) {
                        int other_idx = room->player_indexes[j];
                        if (other_idx != -1 && other_idx != i) {
                            clients[other_idx].status = CONNECTED;
                            room->ready_players[j] = 0;
                        }
                    }
                    room->ready_count = 0;

                    leave_room(room_id, i);
                }

                memset(&clients[i], 0, sizeof(ClientContext));
                clients[i].socket_fd = -1;
                clients[i].player_id = -1;
                clients[i].status = DISCONNECTED;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void generate_token(char *token, int length) {
    // Definice povolených znaků (abeceda pro token)
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    int charset_size = sizeof(charset) - 1;

    for (int i = 0; i < length; i++) {
        int key = rand() % charset_size;
        token[i] = charset[key];
        // token[i] = charset[0];
    }
    
    // Každý řetězec v C musí být zakončen nulovým znakem
    token[length] = '\0';
}

void* client_handler(void* arg){
    ThreadContext *context = (ThreadContext*)arg;
    int client_sock = context->socket_fd;
    int client_index = context->client_index;
    free(context);

    // printf("Index: %d\n",client_index);

    ClientContext *client = &clients[client_index];

    // Inicializace klienta
    pthread_mutex_lock(&clients_mutex);
    client->socket_fd = client_sock;
    client->player_id = client_index;
    client->status = DISCONNECTED;
    client->invalid_message_count = 0;
    client->is_active = 1;
    client->is_connected = 0;
    client->disconnect_time = 0;
    client->last_heartbeat = time(NULL);
    // memset(client->nick, 0, NICK_LEN + 1);
    pthread_mutex_unlock(&clients_mutex);

    DLOG("THREAD START slot=%d fd=%d nick='%s' is_conn=%d",
     client_index, client_sock, client->nick, client->is_connected);



    // printf("=== ClientContext ===\n");
    // printf("socket_fd           : %d\n", client->socket_fd);
    // printf("player_id           : %d\n", client->player_id);
    // printf("nick                : '%s'\n", client->nick);
    // printf("status              : %d\n", client->status);
    // printf("last_status         : %d\n", client->last_status);
    // printf("is_active           : %d\n", client->is_active);
    // printf("is_connected        : %d\n", client->is_connected);
    // printf("invalid_msg_count   : %d\n", client->invalid_message_count);

    // printf("last_heartbeat      : %ld\n", (long)client->last_heartbeat);
    // printf("disconnect_time     : %ld\n", (long)client->disconnect_time);

    // printf("current_room        : %p\n", (void *)client->current_room);
    // printf("token               : '%s'\n", client->token);

    // printf("=====================\n");



    
    LOG_INFO("Vlákno spuštěno pro klienta (fd=%d, index=%d)\n", client_sock, client_index);
    // printf("Vlákno spuštěno pro klienta (fd=%d, index=%d, nick=%s)\n", client_sock, client_index, client->nick);

    while(1){
        ProtocolHeader header;
        memset(&header, 0, sizeof(header));
        char* message_body = NULL;

        int message_status = read_full_message(client_sock, &header, &message_body);

        // Kontrola chyb
        // if(message_status == -1) {
        //     // Klient se odpojil nebo síťová chyba
        //     LOG_INFO("Klient se odpojil (fd=%d)\n", client_sock);
        //     printf("Klient se odpojil.\n");
        //     if(message_body) free(message_body);
        //     break;
        // }

        if (message_status == -1) {
            LOG_INFO("Klient se odpojil (fd=%d, idx=%d, nick=%s)\n",
                    client_sock, client_index, clients[client_index].nick);

            DLOG("DISCONNECT fd=%d slot=%d (before) ctx_fd=%d is_conn=%d nick='%s'",
     client_sock, client_index, clients[client_index].socket_fd,
     clients[client_index].is_connected, clients[client_index].nick);


            pthread_mutex_lock(&clients_mutex);

            // jen když to pořád odpovídá tomuhle socketu (ochrana proti reconnect swapu)
            if (clients[client_index].socket_fd == client_sock) {
                clients[client_index].last_status = clients[client_index].status;
                clients[client_index].is_connected = 0;
                clients[client_index].is_active = 0;
                clients[client_index].disconnect_time = time(NULL);
                clients[client_index].socket_fd = -1;
            }

            pthread_mutex_unlock(&clients_mutex);

            DLOG("DISCONNECT slot=%d (after) ctx_fd=%d is_conn=%d disc_time=%ld",
     client_index, clients[client_index].socket_fd,
     clients[client_index].is_connected, (long)clients[client_index].disconnect_time);


            close(client_sock); // zavřít fd vlákna
            if (message_body) free(message_body);
            break;
        }

        
        if(message_status < 0) {
            // Chyba protokolu
            LOG_ERROR("Chyba protokolu: kód %d (fd=%d)\n", message_status, client_sock);
            // printf("Chyba protokolu: kód %d (fd=%d)\n", message_status, client_sock);
            if(message_body) free(message_body);
            close(client_sock);
            break;
        }

        pthread_mutex_lock(&clients_mutex);
        client->last_heartbeat = time(NULL);
        pthread_mutex_unlock(&clients_mutex);

        LOG_INFO("Přijato: type='%s' len=%d body='%s'\n", 
               header.type_msg, 
               header.message_len,
               message_body ? message_body : "(empty)");

        int should_disconnect = 0;

        pthread_mutex_lock(&clients_mutex);

        switch(client->status){
            case DISCONNECTED: {

                if(strcmp(header.type_msg, LOGI) == 0) {
                    if(!message_body || strlen(message_body) == 0 || strlen(message_body) > NICK_LEN) {
                        send_error(client->socket_fd, "Neplatná délka nicku");
                        close(client_sock);
                        break;
                    }

                    // Tady je potřeba pokus o rozparsování, pokud se ve zprávě nachází | delimeter
                    char nick[NICK_LEN + 1] = {0};
                    char token[TOKEN_LEN + 1] = {0};
                    int has_token = 0;
                    
                    const char *sep = strchr(message_body, '|');

                    if (sep) {
                        size_t nick_len = sep - message_body;
                        
                        const char *token_ptr = sep + 1;
                        size_t token_len_received = strlen(token_ptr);

                        if (nick_len == 0 || nick_len > NICK_LEN) {
                            send_error(client->socket_fd, "Neplatná délka nicku");
                            break;
                        }

                        if (token_len_received != TOKEN_LEN) {
                            send_error(client->socket_fd, "Neplatná délka tokenu");
                            break; 
                        }

                        memcpy(nick, message_body, nick_len);
                        nick[nick_len] = '\0';

                        memcpy(token, token_ptr, TOKEN_LEN);
                        token[TOKEN_LEN] = '\0';

                        has_token = 1;
                        // printf("Reconnect pokus: nick=%s, token=%s\n", nick, token);
                        // printf("Pokus o reconnect.\n");
                    }
                    else{
                        size_t nick_len = strlen(message_body);

                        if(nick_len == 0 || nick_len > NICK_LEN){
                            send_error(client->socket_fd, "Neplatná délka nicku");
                            break;
                        }
                        strncpy(nick, message_body, NICK_LEN);
                        nick[NICK_LEN] = '\0';

                        // printf("Nové připojení: nick=%s\n", nick);
                    }
                
                    DLOG("LOGI recv slot=%d fd=%d body='%s'", client_index, client_sock, message_body);

                    // POKUS O RECONNECT - najdi hráče podle nicku
                    int existing_idx = find_player_by_nick(nick);

                    DLOG("RECO CHECK nick='%s' token_recv='%s' token_ctx='%s' has_token=%d existing_idx=%d existing_is_conn=%d existing_fd=%d",
                    nick, token, (existing_idx>=0 ? clients[existing_idx].token : "-"),
                    has_token, existing_idx,
                    existing_idx>=0 ? clients[existing_idx].is_connected : -1,
                    existing_idx>=0 ? clients[existing_idx].socket_fd : -1);


                    
                    if(existing_idx >= 0) {
                        if(clients[existing_idx].is_connected || !has_token){
                            LOG_DEBUG("DEBUG: Jméno je obsazené (is_connected=1)\n");
                            send_error(client->socket_fd, "Uživatel již existuje");
                            close(client->socket_fd);
                            break;
                        }
                        DLOG("RECO TOKEN CMP idx=%d recv='%s' ctx='%s' strcmp=%d",
                        existing_idx, token, clients[existing_idx].token,
                        strcmp(clients[existing_idx].token, token));

                        if (strcmp(clients[existing_idx].token, token) != 0){
                            send_error(client->socket_fd, "Neplatný token");
                            break;
                        }

                        // printf("Hráč %s se reconnectuje.", nick);
                        if (existing_idx != client_index){
                            DLOG("RECO SWAP newfd=%d -> idx=%d (oldfd=%d)",
                            client_sock, existing_idx, clients[existing_idx].socket_fd);

                            clients[existing_idx].socket_fd = client_sock;
                            clients[existing_idx].is_active = 1;

                            client->socket_fd = -1;
                            client->is_active = 0;

                            client_index = existing_idx;
                            client = &clients[client_index];
                        } else{
                            client->socket_fd = client_sock;
                            client->is_active = 1;
                        }

                            clients[client_index].is_connected = 1;
                            clients[client_index].last_heartbeat = time(NULL);
                            clients[client_index].status = clients[client_index].last_status;
                            
                            // DŮLEŽITÉ: UNLOCK PŘED voláním externích funkcí!
                            pthread_mutex_unlock(&clients_mutex);
                            
                            send_message(clients[client_index].socket_fd, RECO, "Reconnect úspěšný");
                            switch(client->last_status){
                                case CONNECTED:
                                    send_message(client->socket_fd, OKAY, "LOBBY");
                                    break;
                                
                                case IN_ROOM:
                                    send_message(client->socket_fd, OKAY, "LOBBY");
                                    break;

                                case ON_TURN:
                                    send_message(client->socket_fd, OKAY, "TURN");
                                    break;
                                
                                case ON_WAIT:
                                    send_message(client->socket_fd, OKAY, "WAIT");
                                    break;

                                case GAME_DONE:
                                    send_message(client->socket_fd, OKAY, "LOBBY");
                                    break;

                                case PAUSED:
                                    send_message(client->socket_fd, OKAY, "PAUSED");
                                    break;
                                
                                default:
                                    send_message(client->socket_fd, OKAY, "LOBBY");
                                    break;
                        }
                            LOG_INFO("Reconnect úspesny");
                            usleep(10000);
                            
                            // Nyní můžeme bezpečně volat get_room_info
                            pthread_mutex_lock(&clients_mutex);
                            GameRoom *room = clients[client_index].current_room;
                            int room_id = room ? room->room_id : -1;
                            // void *game_inst = (room && room->game_instance) ? room->game_instance : NULL;
                            pthread_mutex_unlock(&clients_mutex);
                            
                            if (room) {
                                GameInstance *game = NULL;

                                pthread_mutex_lock(&clients_mutex);
                                game = room->game_instance;

                                if (game && game->state == GAME_STATE_PAUSED) {
                                    game_resume(game);
                                }
                                pthread_mutex_unlock(&clients_mutex);


                                pthread_mutex_lock(&clients_mutex);
                                game = room->game_instance;
                                pthread_mutex_unlock(&clients_mutex);

                                if (game) {
                                    char full_state[4096];
                                    if (game_get_full_state(game, client_index,
                                                            full_state, sizeof(full_state)) > 0) {
                                        send_message(clients[client_index].socket_fd,
                                                    STAT, full_state);
                                    }

                                    broadcast_to_room(room_id, RESU, "Hráč se vrátil do hry, obnovuji hru", -1);
                                } else{
                                    printf("Jop byl jenom v místnosti\n");
                                    client->status = CONNECTED;
                                }
                            }
                            break;
                        }
                    
                    
                    // NOVÝ HRÁČ
                    LOG_DEBUG("Vytvářím nového hráče '%s' na slotu %d\n", message_body, client_index);
                    strncpy(client->nick, message_body, NICK_LEN);
                    client->nick[NICK_LEN] = '\0';
                    client->status = CONNECTED;
                    client->is_connected = 1;
                    client->invalid_message_count = 0;
                    client->socket_fd = client_sock;
                    client->player_id = client_index;
                    generate_token(client->token, 10);
                    
                    char message[40];
                    snprintf(message, sizeof(message), "Vítej ve hře!|%s", client->token);
                    send_message(client->socket_fd, OKAY, message);

                    LOG_INFO("Nový klient '%s' přihlášen (fd=%d, slot=%d, token=%s)\n", 
                           client->nick, client->socket_fd, client_index, client->token);
                    
                } else if(strcmp(header.type_msg, QUIT) == 0) {
                    should_disconnect = 1;
                    
                }
                // Pokud přijde dřív PING než LOGI, je to v pořádku
                else if(strcmp(header.type_msg, PING) == 0){
                    continue;
                } 
                
                // Pokud přijde cokoliv jiného než očekáváno, odpoj klienta
                else {
                    send_error(client->socket_fd, "Očekáván příkaz LOGI");
                    should_disconnect = 1;
                }
                break;
            }

            case CONNECTED: {
                if(strcmp(header.type_msg, QUIT) == 0) {
                    should_disconnect = 1;
                    
                } else if(strcmp(header.type_msg, PONG) == 0) {
                    // Heartbeat aktualizován

                } else if(strcmp(header.type_msg, RLIS) == 0) {
                    char room_list[4096];
                    int count = get_room_list(room_list, sizeof(room_list));

                    if(count > 0){
                        send_message(client->socket_fd, RLIS, room_list);
                    } else{
                        send_message(client->socket_fd, ELIS, "Žádné místnosti");
                    }
                
                } else if(strcmp(header.type_msg, RCRT) == 0) {
                    if(!message_body || strlen(message_body) == 0) {
                        send_message(client->socket_fd, ECRT, "Chybí název");
                        break;
                    }
                    
                    int room_id = create_room(message_body, client->player_id);
                    
                    if(room_id >= 0){
                        char room_id_str[12];
                        snprintf(room_id_str, sizeof(room_id_str), "%d", room_id);
                        
                        client->status = IN_ROOM;
                        client->current_room = find_room(room_id);
                        
                        send_message(client->socket_fd, OCRT, room_id_str);
                        send_message(client->socket_fd, BOSS, "1");
                    } else{
                        send_message(client->socket_fd, ECRT, "Nelze vytvořit");
                    }
                    
                } else if(strcmp(header.type_msg, RCNT) == 0) {
                    if(!message_body || strlen(message_body) == 0){
                        send_message(client->socket_fd, ECNT, "Chybí ID");
                        break;
                    }

                    int room_id = atoi(message_body);

                    if(connect_room(room_id, client->player_id) >= 0){
                        client->status = IN_ROOM;
                        client->current_room = find_room(room_id);

                        send_message(client->socket_fd, OCNT, message_body);
                    } else {
                        send_message(client->socket_fd, ECNT, "Nelze připojit");
                    }
                    
                } else {
                    send_error(client->socket_fd, "Neznámý příkaz (CONNECTED)");
                    client->invalid_message_count++;
                    should_disconnect = 1;
                }
                break;
            }

            case IN_ROOM: {
                GameRoom *room = client->current_room;
                
                if(!room){
                    send_error(client->socket_fd, "Nejsi v místnosti");
                    client->status = CONNECTED;
                    break;
                }
                
                int room_id = room->room_id;
                
                if(strcmp(header.type_msg, RDIS) == 0){
                    pthread_mutex_unlock(&clients_mutex);
                    
                    leave_room(room_id, client_index);
                    if(delete_room(room_id) == -1){
                        broadcast_to_room(room_id, BOSS, "Byl jsi jmenován vlastníkem", -1);
                    }
                    
                    pthread_mutex_lock(&clients_mutex);
                    
                    client->current_room = NULL;
                    client->status = CONNECTED;

                    send_message(client->socket_fd, ODIS, "Opuštěno");
                    
                } else if(strcmp(header.type_msg, PONG) == 0) {
                    // Heartbeat aktualizován

                } else if(strcmp(header.type_msg, REDY) == 0){
                    int ready = (message_body && message_body[0] == '1') ? 1 : 0;

                    if(set_player_ready(room_id, client_index, ready) == 0){
                        char ready_players_str[128];
                        char room_info[1024];
                        
                        room = find_room(room_id);
                        if(room){
                            snprintf(ready_players_str, sizeof(ready_players_str), 
                                    "(%d/%d)", room->ready_count, room->max_players);
                        }

                        pthread_mutex_unlock(&clients_mutex);
                        
                        get_room_info(room_id, room_info, sizeof(room_info));
                        broadcast_to_room(room_id, PRDY, ready_players_str, -1);
                        broadcast_to_room(room_id, RINF, room_info, -1);
                        
                        pthread_mutex_lock(&clients_mutex);
                    } else{
                        send_error(client->socket_fd, "Chyba ready");
                    }
                    
                } else if(strcmp(header.type_msg, STRT) == 0){          
                    if(room->owner_index != client_index){
                        send_message(client->socket_fd, ESTR, "Pouze owner");
                        break;
                    }

                    if(room->ready_count < room->player_count){
                        send_message(client->socket_fd, ESTR, "Ne všichni jsou připraveni");
                        break;
                    }

                    // printf("%d | %d\n", room->ready_count, room->player_count);
                    
                    GameInstance *game = game_create(room);
                    
                    if(!game){
                        send_message(client->socket_fd, ESTR, "Chyba při vytváření");
                        break;
                    }

                    room->game_instance = game;

                    if(game_start(game) != 0){              // <-- tady musi byt ==
                        send_message(client->socket_fd, ESTR, "Chyba při startu");
                        game_destroy(game);
                        room->game_instance = NULL;
                        break;
                    }

                    room->status = ROOM_PLAYING;
                    
                    pthread_mutex_unlock(&clients_mutex);
                    broadcast_to_room(room_id, STRT, "Hra začíná!", -1);
                    pthread_mutex_lock(&clients_mutex);

                    // tady "odpřipravíme" hráče, abychom po hře mohli kontrolovat, zda chtějí pokračovat
                    for(int i = 0; i < room->player_count; i++){
                        int idx = room->player_indexes[i];

                        // unready
                        set_player_ready(room->room_id, idx, 0);
                    }

                    room = find_room(room_id);
                    if(room && room->game_instance){
                        game = (GameInstance*)room->game_instance;

                        for(int i = 0; i < MAX_PLAYERS_PER_ROOM; i++){
                            int idx = room->player_indexes[i];

                            if(idx != -1 && idx < MAX_CLIENTS){
                                int current_player_idx = room->player_indexes[game->current_player_index];
                                
                                if(idx == current_player_idx){
                                    clients[idx].status = ON_TURN;
                                    send_message(clients[idx].socket_fd, TURN, "Jsi na tahu");
                                } else{
                                    clients[idx].status = ON_WAIT;
                                    send_message(clients[idx].socket_fd, WAIT, "Čekej");
                                }

                                char hand_cards[2048];
                                if(game_get_player_cards(game, idx, hand_cards, sizeof(hand_cards)) > 0){
                                    send_message(clients[idx].socket_fd, CRDS, hand_cards);
                                }
                            }
                        }
                    }
                    
                } else if(strcmp(header.type_msg, QUIT) == 0) {
                    leave_room(room_id, client->player_id);
                    delete_room(room_id);
                    should_disconnect = 1;
                    
                } else {
                    send_error(client_sock, "Nejsi v místnosti");
                    close(client_sock);
                }
                break;
            }

            case ON_TURN: {
                GameRoom *room = client->current_room;

                if(!room || !room->game_instance){
                    send_error(client->socket_fd, "Hra neběží");
                    client->status = CONNECTED;
                    break;
                }
                
                GameInstance *game = (GameInstance*)room->game_instance;
                int room_id = room->room_id;

                if(strcmp(header.type_msg, TAKP) == 0){
                    // Lízni z balíčku
                    int result = game_process_move(game, client_index, header.type_msg, message_body);
                    
                    if(result == 0){
                        // Tah byl úspěšný - informuj VŠECHNY hráče v místnosti o změně stavu
                        room = find_room(room_id);
                        if(room && room->game_instance){
                            game = (GameInstance*)room->game_instance;

                            // Projdeme všechny sloty pro hráče v místnosti
                            for(int i = 0; i < MAX_PLAYERS_PER_ROOM; i++){
                                int idx = room->player_indexes[i];
                                
                                // Kontrola, zda je na tomto indexu připojený klient
                                if(idx != -1 && idx < MAX_CLIENTS){
                                    char full_state[4096];
                                    int target_client_index = clients[idx].player_id;

                                    // Vygenerujeme personalizovaný stav pro konkrétního klienta
                                    int written = game_get_full_state(game, target_client_index, full_state, sizeof(full_state));
                                    
                                    if(written > 0){
                                        // Pošleme aktualizovaná data (UPDT) každému hráči
                                        send_message(clients[idx].socket_fd, "STAT", full_state);
                                    }
                                }
                            }
                        }
                    }
                    else if(result == -2){
                        send_error(client->socket_fd, "Již jsi lízl");
                    }
                    else if(result == -3){
                        send_error(client->socket_fd, "Již jsi vyhodil");
                    }
                    else if(result == -4){
                        send_error(client->socket_fd, "První hráč v prvním kole nelíže");
                    }
                    else if(result == -5){
                        send_error(client->socket_fd, "Obracím balíček, zkus to znovu");
                    }
                    else{
                        send_error(client->socket_fd, "Neplatný tah");
                    }
                } else if(strcmp(header.type_msg, PONG) == 0) {
                    // Heartbeat aktualizován

                }
                else if(strcmp(header.type_msg, TAKT) == 0){
                    // Lízni z trash
                    int result = game_process_move(game, client_index, header.type_msg, message_body);
                    
                    if(result == 0){
                        room = find_room(room_id);
                        if(room && room->game_instance){
                            game = (GameInstance *)room->game_instance;
                            for(int i = 0; i < MAX_PLAYERS_PER_ROOM; i++){
                                int idx = room->player_indexes[i];

                                if(idx != -1 && idx < MAX_CLIENTS){
                                    char full_state[4096];
                                    int target_index = clients[idx].player_id;

                                    int written = game_get_full_state(game, target_index, full_state, sizeof(full_state));

                                    if(written > 0){
                                        send_message(clients[idx].socket_fd, STAT, full_state);
                                    }
                                }
                            }
                        }
                    }
                    else if(result == -2){
                        send_error(client->socket_fd, "Již jsi lízl");
                    } else if (result == -3){
                        send_error(client->socket_fd, "Balíček je prázdný");
                    }
                    else{
                        send_error(client->socket_fd, "Neplatný tah");
                    }
                }
                else if(strcmp(header.type_msg, UNLO) == 0){
                    // Vylož karty
                    int result = game_process_move(game, client_index, header.type_msg, message_body);
                    
                    if(result == 0){
                        room = find_room(room_id);
                        if(room && room->game_instance){
                            game = (GameInstance *)room->game_instance;
                            for(int i = 0; i < MAX_PLAYERS_PER_ROOM; i++){
                                int idx = room->player_indexes[i];

                                if(idx != -1 && idx < MAX_CLIENTS){
                                    char full_state[4096];
                                    int target_index = clients[idx].player_id;

                                    int written = game_get_full_state(game, target_index, full_state, sizeof(full_state));

                                    if(written > 0){
                                        send_message(clients[idx].socket_fd, STAT, full_state);
                                    }
                                }
                            }
                        }
                    }else if(result == -69){
                        send_error(client->socket_fd, "Akci nelze provést (neměl bys čím zavřít)");
                    }
                    else{
                        send_error(client->socket_fd, "Neplatná postupka");
                    }
                } else if(strcmp(header.type_msg, ADDC) == 0){
                    // Přilož kartu k existující postupce
                    int result = game_process_move(game, client_index, header.type_msg, message_body);
                    
                    if(result == 0){
                        // Úspěch -> broadcast všem hráčům v místnosti
                        room = find_room(room_id);
                        if(room && room->game_instance){
                            for(int i = 0; i < MAX_PLAYERS_PER_ROOM; i++){
                                int idx = room->player_indexes[i];
                                if(idx != -1 && idx < MAX_CLIENTS){
                                    char full_state[4096];
                                    int target_id = clients[idx].player_id;
                                    
                                    if(game_get_full_state(game, target_id, full_state, sizeof(full_state)) > 0){
                                        send_message(clients[idx].socket_fd, "STAT", full_state);
                                    }
                                }
                            }
                        }
                        send_message(client->socket_fd, OKAY, "Karta přiložena");
                    } else {
                        send_error(client->socket_fd, "Kartu nelze k této postupce přiložit");
                    }
                }
                else if(strcmp(header.type_msg, THRW) == 0){
                    // Vyhoď kartu
                    int result = game_process_move(game, client_index, header.type_msg, message_body);
                    
                    if(result == 0){
                        // Zkontroluj, zda hra neskončila
                        room = find_room(room_id);
                        if(room && room->game_instance){
                            game = (GameInstance*)room->game_instance;
                            
                            if(game->state == GAME_STATE_FINISHED){
                                // Hra skončila!
                                pthread_mutex_unlock(&clients_mutex);
                                broadcast_to_room(room_id, OKAY, client->nick, -1);
                                pthread_mutex_lock(&clients_mutex);
                                
                                // Vrať všechny do IN_ROOM
                                for(int i = 0; i < MAX_PLAYERS_PER_ROOM; i++){
                                    int idx = room->player_indexes[i];
                                    if(idx != -1 && idx < MAX_CLIENTS){
                                        clients[idx].status = GAME_DONE;
                                    }
                                }
                            } else {
                                // 1. Aktualizace statusů v poli clients (pro interní logiku serveru)
                                // Současný hráč (ten, co vyhodil) přechází na čekání
                                client->status = ON_WAIT;

                                // Najdeme index dalšího hráče na řadě
                                int next_idx = room->player_indexes[game->current_player_index];
                                if(next_idx != -1 && next_idx < MAX_CLIENTS){
                                    clients[next_idx].status = ON_TURN;
                                }

                                // 2. BROADCAST: Pošleme zprávu STAT (UPDT) oběma hráčům
                                // Projdeme všechny sloty v místnosti
                                for(int i = 0; i < MAX_PLAYERS_PER_ROOM; i++) {
                                    int idx = room->player_indexes[i];
                                    
                                    if(idx != -1 && idx < MAX_CLIENTS) {
                                        char full_state[4096];
                                        int target_id = clients[idx].player_id;
                                        
                                        // Vygenerujeme personalizovaný stav (obsahuje ruku, postupky, TURN/WAIT i enemy_count)
                                        int written = game_get_full_state(game, target_id, full_state, sizeof(full_state));
                                        
                                        if(written > 0) {
                                            // Pošleme sjednocenou zprávu STAT (místo CRDS, TURN nebo WAIT)
                                            send_message(clients[idx].socket_fd, "STAT", full_state);
                                            
                                            // Volitelně můžeš nechat krátkou textovou zprávu pro konzoli v Pygame
                                            if(clients[idx].status == ON_TURN) {
                                                send_message(clients[idx].socket_fd, TURN, "Jsi na tahu");
                                            } else {
                                                send_message(clients[idx].socket_fd, WAIT, "Čekej, hraje soupeř");
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    else if (result == -2){
                        send_error(client->socket_fd, "Nejdříve musíš líznout.");
                    }
                    else if (result == -3){
                        send_error(client->socket_fd, "Nemůžeš vyhodit, ale můžeš zavřít!");
                    }
                    else{
                        send_error(client->socket_fd, "Nemůžeš vyhodit tuto kartu");
                    }
                }
                else if(strcmp(header.type_msg, CLOS) == 0){
                    // Zavři hru
                    int result = game_process_move(game, client_index, header.type_msg, message_body);
                    
                    if(result == 0){
                        // Hra skončila!
                        char end_report[1024] = {0};
                        int offset = 0;

                        pthread_mutex_unlock(&clients_mutex);
                        game_calculate_scores(game);
                        pthread_mutex_lock(&clients_mutex);

                        offset += snprintf(end_report + offset, sizeof(end_report) - offset, "W:%s", client->nick);

                        for(int i = 0; i < game->player_count; i++){
                            int c_inx = game->players[i].client_index;

                            if(c_inx != -1){
                                offset += snprintf(end_report + offset, sizeof(end_report) - offset, "|P:%s:%d:%d:%d", 
                            clients[c_inx].nick, game->players[i].score, game->players[i].cards_played, game->players[i].turns_played);
                            }
                        }

                        pthread_mutex_unlock(&clients_mutex);
                        broadcast_to_room(room_id, GEND, end_report, -1);
                        pthread_mutex_lock(&clients_mutex);
                        
                        // Vrať všechny do IN_ROOM
                        room = find_room(room_id);
                        if(room){
                            for(int i = 0; i < MAX_PLAYERS_PER_ROOM; i++){
                                int idx = room->player_indexes[i];
                                if(idx != -1 && idx < MAX_CLIENTS){
                                    clients[idx].status = GAME_DONE;
                                    // printf("Status pro index %d = %d\n", idx, clients[idx].status);
                                }
                            }
                        }
                    }
                    else{
                        send_error(client->socket_fd, "Nemůžeš zavřít");
                    }
                }
                else if(strcmp(header.type_msg, QUIT) == 0){
                    game_pause(game, "Hráč se odpojil");
                    should_disconnect = 1;
                }
                else {
                    send_error(client->socket_fd, "Neznámý příkaz (ON_TURN)");
                    close(client_sock);
                }
                break;
            }

            case ON_WAIT: {
                GameRoom *room = client->current_room;

                if(!room || !room->game_instance){
                    send_error(client->socket_fd, "Hra neběží");
                    client->status = CONNECTED;
                    break;
                }

                GameInstance *game = (GameInstance*)room->game_instance;

                if(strcmp(header.type_msg, QUIT) == 0){
                    game_pause(game, "Hráč se odpojil");
                    should_disconnect = 1;
                } else if(strcmp(header.type_msg, PONG) == 0) {
                    // Heartbeat aktualizován

                } else {
                    send_error(client->socket_fd, "Nejsi na tahu");
                }
                break;
            }

            case PAUSED: {
                if(strcmp(header.type_msg, QUIT) == 0) {
                    should_disconnect = 1;
                } else if(strcmp(header.type_msg, PONG) == 0) {
                    // Heartbeat aktualizován

                } else {
                    send_message(client->socket_fd, NOTI, "Hra pozastavena");
                }
                break;
            }

            case GAME_DONE: {
                GameRoom *room = client->current_room;

                if(!room || !room->game_instance){
                    send_error(client->socket_fd, "Hra neběží");
                    client->status = CONNECTED;
                    break;
                }
                
                GameInstance *game = (GameInstance*)room->game_instance;
                int room_id = room->room_id;

                if(strcmp(header.type_msg, QUIT) == 0) {
                    should_disconnect = 1;
                }
                else if (strcmp(header.type_msg, PLAG) == 0){
                    // logika taková, že se čeká na oba hráče
                    for(int i = 0; i < room->player_count; i++){
                        int c_idx = room->player_indexes[i];

                        
                        if(client->player_id == c_idx){
                            if(strcmp(message_body, "1")){
                                set_player_ready(room->room_id, c_idx, 1);
                                LOG_INFO("Hráč idx:%d chce hrát znovu!", c_idx);
                        }
                            else 
                            {
                                set_player_ready(room->room_id, c_idx, 0);
                                LOG_INFO("Hráč idx:%d už nechce hrát znovu!", c_idx);
                            }
                        }
                    }

                    if(room->ready_count != room->player_count){
                        send_message(client->socket_fd, ESTR, "Čekání na protihráče");
                        break;
                    }
                    
                    game_destroy(game);

                    GameInstance *game = game_create(room);
                    
                    if(!game){
                        send_message(client->socket_fd, ESTR, "Chyba při vytváření");
                        break;
                    }

                    room->game_instance = game;

                    if(game_start(game) != 0){
                        send_message(client->socket_fd, ESTR, "Chyba.");
                        game_destroy(game);
                        room->game_instance = NULL;
                        break;
                    }

                    room->status = ROOM_PLAYING;
                    
                    pthread_mutex_unlock(&clients_mutex);
                    broadcast_to_room(room_id, STRT, "Hra začíná!", -1);
                    pthread_mutex_lock(&clients_mutex);

                    room = find_room(room_id);
                    if(room && room->game_instance){
                        game = (GameInstance*)room->game_instance;

                        for(int i = 0; i < MAX_PLAYERS_PER_ROOM; i++){
                            int idx = room->player_indexes[i];

                            if(idx != -1 && idx < MAX_CLIENTS){
                                int current_player_idx = room->player_indexes[game->current_player_index];
                                
                                if(idx == current_player_idx){
                                    clients[idx].status = ON_TURN;
                                    send_message(clients[idx].socket_fd, TURN, "Jsi na tahu");
                                } else{
                                    clients[idx].status = ON_WAIT;
                                    send_message(clients[idx].socket_fd, WAIT, "Čekej");
                                }

                                char hand_cards[2048];
                                if(game_get_player_cards(game, idx, hand_cards, sizeof(hand_cards)) > 0){
                                    send_message(clients[idx].socket_fd, CRDS, hand_cards);
                                }
                            }
                        }
                    }

                    room = find_room(room_id);
                    if(room && room->game_instance){
                        game = (GameInstance*)room->game_instance;

                        // Projdeme všechny sloty pro hráče v místnosti
                        for(int i = 0; i < MAX_PLAYERS_PER_ROOM; i++){
                            int idx = room->player_indexes[i];
                            
                            // Kontrola, zda je na tomto indexu připojený klient
                            if(idx != -1 && idx < MAX_CLIENTS){
                                char full_state[4096];
                                int target_client_index = clients[idx].player_id;
                                set_player_ready(room->room_id, target_client_index, 0);

                                // Vygenerujeme personalizovaný stav pro konkrétního klienta
                                int written = game_get_full_state(game, target_client_index, full_state, sizeof(full_state));
                                
                                if(written > 0){
                                    // Pošleme aktualizovaná data (UPDT) každému hráči
                                    send_message(clients[idx].socket_fd, "STAT", full_state);
                                }
                            }
                        }

                    }
                    LOG_DEBUG("ROOM DEBUG: room_id=%d status=%d player_count=%d ready_count=%d game_instance=%p",
                            room_id,
                            room ? room->status : -1,
                            room ? room->player_count : -1,
                            room ? room->ready_count : -1,
                            room ? room->game_instance : NULL);

                    for(int i = 0; i < MAX_PLAYERS_PER_ROOM; i++){
                        int idx = room->player_indexes[i];

                        if(idx == -1){
                            LOG_DEBUG("ROOM[%d]: slot=%d EMPTY", room_id, i);
                            continue;
                        }

                        if(idx < 0 || idx >= MAX_CLIENTS){
                            LOG_DEBUG("ROOM[%d]: slot=%d INVALID idx=%d", room_id, i, idx);
                            continue;
                        }

                        LOG_DEBUG(
                            "ROOM[%d]: slot=%d idx=%d nick='%s' socket=%d connected=%d active=%d status=%d player_id=%d last_heartbeat=%ld",
                            room_id,
                            i,
                            idx,
                            clients[idx].nick,
                            clients[idx].socket_fd,
                            clients[idx].is_connected,
                            clients[idx].is_active,
                            clients[idx].status,
                            clients[idx].player_id,
                            clients[idx].last_heartbeat
                        );
                    }

                    if(room && room->game_instance){
                        GameInstance *g = (GameInstance*)room->game_instance;

                        LOG_DEBUG(
                            "GAME DEBUG: current_player_index=%d state=%d",
                            g->current_player_index,
                            g->state
                        );
                    }
                } else if(strcmp(header.type_msg, PONG) == 0) {
                    // Heartbeat aktualizován

                }
                else if(strcmp(header.type_msg, LBBY) == 0){
                    // vrať hráče do lobby (oba dva -> druhý nemá na co čekat)
                    // smazaní místnosti
                    pthread_mutex_unlock(&clients_mutex);
                    broadcast_to_room(room_id, LBBY, "", -1);
                    pthread_mutex_lock(&clients_mutex);

                    game_destroy(game);
                    for(int i = 0; i < room->max_players; i++){
                        if(i != -1){
                            int c_idx = room->player_indexes[i];
                            leave_room(room_id, c_idx);
                            clients[c_idx].status = CONNECTED;
                            clients[c_idx].current_room = NULL;
                        }
                    }
                    
                    
                }else if(strcmp(header.type_msg, CNNT) == 0){
                    client->status = CONNECTED;
                    leave_room(client->current_room->room_id, client->player_id);
                }else {
                    send_message(client->socket_fd, NOTI, "Hra skončila");
                    for(int i = 0; i < MAX_CLIENTS; i++){

                    }
                    client->status = IN_ROOM;
                }
                break;
            }
        }

        pthread_mutex_unlock(&clients_mutex);

        if(message_body) {
            free(message_body);
            message_body = NULL;
        }

        if(should_disconnect) {
            break;
        }
    }

    // Cleanup
    LOG_INFO("Klient %s se odpojuje (fd=%d, slot=%d)\n", 
           client->nick[0] ? client->nick : "unknown", client_sock, client_index);

    pthread_mutex_lock(&clients_mutex);

    if (client->current_room) {
        char notify_msg[128];
        snprintf(notify_msg, sizeof(notify_msg), "Hráč %s se odpojil.", client->nick);
        
        pthread_mutex_unlock(&clients_mutex);
        broadcast_to_room(client->current_room->room_id, PAUS, notify_msg, client_index);
        pthread_mutex_lock(&clients_mutex);
        
        if (client->current_room->game_instance) {
            game_pause((GameInstance*)client->current_room->game_instance, "Hra pozastavena - čeká se na reconnect");
        }
    }
    
    // printf("%d | %d | %d\n", client_sock, );
    // int sock = client->socket_fd;
    if(client_sock > 0) {
        close(client_sock);
    }
    
    client->socket_fd = -1;
    client->is_connected = 0;
    client->disconnect_time = time(NULL);
    client->is_active = 0;
    // printf("Status klienta před odpojením: %d (ukládám do last_status)\n", client->status);
    client->last_status = client->status;
    client->status = DISCONNECTED;

    // DEBUG: Výpis stavu po odpojení
    LOG_DEBUG("  -> Po odpojení: nick='%s', socket_fd=%d, is_connected=%d, disconnect_time=%ld, status=%d, last_status=%d\n",
           client->nick, client->socket_fd, client->is_connected, client->disconnect_time, client->status, client->last_status);

    // PONECHÁME: nick, player_id, status, current_room pro reconnect!
    
    pthread_mutex_unlock(&clients_mutex);

    return NULL;
}