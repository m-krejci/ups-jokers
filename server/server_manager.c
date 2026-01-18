#include "server_manager.h"
#include "config.h"
#include "client_manager.h"
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

void error(const char *msg){
    LOG_ERROR("%s", msg);
    exit(EXIT_FAILURE);
}

void* timeout_checker_thread(void* arg){
    LOG_INFO("Timeout checker vlákno spuštěno (interval %ds)\n", TIMEOUT_CHECK_INTERVAL);

    while(1){
        sleep(TIMEOUT_CHECK_INTERVAL);
        check_client_timeouts();
    }

    return NULL;
}

void start_server(){
    int server_fd, new_socket;          // proměnné pro server socket a socket klienta
    struct sockaddr_in address;         // promenná pro nastavení IPv4, adresy a portu
    int addrlen = sizeof(address); 
    int result;                         // pomocná proměnná pro návratové hodnoty jednotlivých funkcí

    // inicializace herního manažera (game_manager.c)
    // initialize_game_manager();

    // přiřazení socketu serveru
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd == 0){
        error("Chyba: socket\n");
    }

    // nastavení automatického (rychlého) uvolnění portu po ukončení serveru
    int opt = 1;
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
        error("Chyba: setsockopt\n");
    }

    // nastavení adresy a portu
    address.sin_family = AF_INET;
    // if(SERVER_ADDRESS){
    //     inet_pton(AF_INET, SERVER_ADDRESS, &address.sin_addr);
    // }
    // else{
    //     address.sin_addr.s_addr = INADDR_ANY;
    // }
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SERVER_PORT);

    // bind
    result = bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    if(result < 0){
        error("Chyba: bind\n");
    }

    // listen
    result = listen(server_fd, MAX_CLIENTS);
    if(result < 0){
        error("Chyba: listen\n");
    }

    // výpis dosavadního stavu
    printf("Server naslouchá na adrese %s:%d, přijímá %d klientů\n", SERVER_ADDRESS, SERVER_PORT, MAX_CLIENTS);

    /**
     * Zde bude inicializace HB vlákna
     */
    // pthread_t hb_thread;
    // if(pthread_create(&hb_thread, NULL, heartbeat_thread, NULL)!=0){
    //  error("Chyba: hb_thread");
    //}
    //pthread_detach(hb_thread);

    pthread_t timeout_thread;
    if(pthread_create(&timeout_thread, NULL, timeout_checker_thread, NULL) != 0){
        error("Chyba: timeout thread\n");
    }
    pthread_detach(timeout_thread);

    for(;;){
        printf("Cekam na klienta...\n");
        // cekani na socket klienta
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if(new_socket < 0){
            error("Chyba: accept\n");
        }

        // struktura klientů připojených k serveru
        pthread_mutex_lock(&clients_mutex);

        int client_index = -1;

        for(int i = 0; i < MAX_CLIENTS; i++){
            if(clients[i].socket_fd == -1){
                client_index = i;
                break;
            }
        }
        // Volné místo nalezeno -> přiřad klienta do pole
        if(client_index != -1){
            clients[client_index].socket_fd = new_socket;
            clients[client_index].player_id = client_index + 1;
            clients[client_index].is_active = 1;
            clients[client_index].status = CONNECTED;

            ThreadContext *context = (ThreadContext*)malloc(sizeof(ThreadContext));
            context->socket_fd = new_socket;
            context->client_index = client_index;

            pthread_t client_thread;

            if(pthread_create(&client_thread, NULL, client_handler, (void*)context) < 0){
                error("Chyba: pthread_create\n");
                send_error(new_socket, "Cannot connect at the moment (pthread_error)");
                close(new_socket);
                clients[client_index].socket_fd = 0;
                clients[client_index].is_active = 0;
            } else{
                pthread_detach(client_thread);
                LOG_INFO("Novy hrac pripojen (FD: %d, ID: %d)\n", new_socket, clients[client_index].player_id);
            }
        } 
        // Nenalezeno volné místo -> informuj klienta a odpoj ho
        else{
            send_error(new_socket, "Cannot connect at the moment (FULL)");
            close(new_socket);
        }
        pthread_mutex_unlock(&clients_mutex);
    }
}