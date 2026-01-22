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
#include <ctype.h>
#include <errno.h>
#include <string.h>

void error(const char *msg){
    LOG_ERROR("%s\n", msg);
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

/**
 * @brief Kontroluje vloženou IP adresu - správný formát, počet teček, pouze čísla
 * @param ip Řetězec IP adresy
 */
int is_valid_ip(const char *ip) {
    // Počítadlo tokenů oddělených tečkou (3 tečky)
    int parts = 0;
    char *copy = strdup(ip);
    if (!copy) return 0;

    char *token = strtok(copy, ".");
    // Kontrola jednotlivých čísel
    while (token) {
        for (int i = 0; token[i]; i++) {
            if (!isdigit(token[i])) {
                free(copy);
                return 0;
            }
        }
        // Po cyklu víme, že jde o čísla (beztrestná akce)
        int num = atoi(token);
        if (num < 0 || num > 255) {
            free(copy);
            return 0;
        }

        token = strtok(NULL, ".");
        parts++;
    }
    // Vrať počet teček (resp. jestli se == 4)
    free(copy);
    return parts == 4;
}

void start_server(int argc, char **argv){
    int server_fd, new_socket;          // Proměnné pro server socket a socket klienta
    struct sockaddr_in address;         // Struktura pro síťové nastavení (IPv4, adresy a portu)
    int addrlen = sizeof(address);      // Délka adresy
    int result;                         // Pomocná proměnná pro návratové hodnoty jednotlivých funkcí
    char *act_add;
    int act_port;

    // Přiřazení socketu serveru
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd == 0){
        printf("ERROR: Chyba při vytváření socketu\n");
        exit(EXIT_FAILURE);
    }

    // Nastavení automatického (rychlého) uvolnění portu po ukončení serveru
    int opt = 1;
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
        printf("ERROR: Volání setsockopt bylo chybné\n");
        exit(EXIT_FAILURE);
    }

    // Nastavení adresy a portu serveru. Umožňuje tři možnosti -> příkazová řádka, config.h nebo použije 0.0.0.0
    address.sin_family = AF_INET;

    // Kontrola, zda uživatel zadal adresu a port do příkazové řádky
    if(argv && argc == ARGUMENT_COUNT){
        if(is_valid_ip(argv[1])){
            if(strcmp(argv[1], LOCALHOST) == 0) printf("WARNING: Adresa %s (loopback) nebude dosažitelná z jiných zařízení.\n", argv[1]);
            if(strcmp(argv[1], BROADCAST) == 0) {
                printf("ERROR: Adresa %s (broadcast) není kompatibilní s TCP\n", argv[1]); 
                exit(EXIT_FAILURE);
            }
            // Vložení adresy do struktury
            if(inet_pton(AF_INET, argv[1], &address.sin_addr) <= 0){
                printf("ERROR: Požadovanou adresu nebylo možné přiřadit. <%s>\n", argv[1]); 
                exit(EXIT_FAILURE);
            } else {
                printf("INFO: Používám adresu %s\n", argv[1]);
                act_add = argv[1];
            }
        } else{
            printf("ERROR: Zvolená IP adresa není validní! <%s>\n", argv[1]);
            exit(EXIT_FAILURE);
        }
    } else if(is_valid_ip(SERVER_ADDRESS)){
        if(inet_pton(AF_INET, SERVER_ADDRESS, &address.sin_addr) <= 0){
            printf("ERROR: Požadovanou adresu nebylo možné přiřadit. <%s>\n", argv[1]); 
            exit(EXIT_FAILURE);
        }
        else{
            printf("INFO: Používám adresu %s\n", SERVER_ADDRESS);
            act_add = SERVER_ADDRESS;
        }
    }
    else{
        // Nastavení adresy na 0.0.0.0 -> poslouchá všude
        address.sin_addr.s_addr = INADDR_ANY;
        printf("INFO: Používám adresu 0.0.0.0\n");
        act_add = "0.0.0.0";
    }
    
    // Kontrola, jestli byl zadán port
    if(argv && argc == ARGUMENT_COUNT){
        char *port_str = argv[2];
        char *endptr;
        errno = 0;

        long port_long = strtol(port_str, &endptr, 10);

        // Kontrola, zda došlo k chybě při převodu
        if (errno != 0 || *endptr != '\0' || port_long < 0 || port_long > 65535) {
            printf("Chyba: Neplatný port '%s'\n", port_str);
            exit(EXIT_FAILURE);
        }

        int port = (int)port_long;
        address.sin_port = htons(port);
        act_port = port; 
        
        printf("INFO: Používám port %d\n", port);
    } else {
        address.sin_port = htons(SERVER_PORT);
        act_port = SERVER_PORT;
    }
    

    // Bind
    result = bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    if(result < 0){
        printf("ERROR: Bind (%d)\n", result);
        exit(EXIT_FAILURE);
    }

    // Listen -> fronta 10 klientů
    result = listen(server_fd, MAX_CLIENTS);
    if(result < 0){
        printf("ERROR: Listen (%d)\n", result);
        exit(EXIT_FAILURE);
    }

    // Výpis dosavadního stavu
    printf("Server naslouchá na adrese %s:%d, přijímá %d klientů\n", act_add, act_port, MAX_CLIENTS);


    // Vlákno pro kontrolu timeoutu (start)
    pthread_t timeout_thread;
    if(pthread_create(&timeout_thread, NULL, timeout_checker_thread, NULL) != 0){
        printf("Chyba: Timeout thread\n");
    }
    pthread_detach(timeout_thread);

    // Smyčka přijímající klienty
    for(;;){
        printf("Čekám na klienta...\n");
        // Čekání na klienta
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        DLOG("ACCEPT fd=%d", new_socket);
        if(new_socket < 0){
            printf("ERROR: Accept (%d)\n", new_socket);
            continue; // Jdi čekat na dalšího klienta
        }

        // Struktura klientů připojených k serveru
        pthread_mutex_lock(&clients_mutex);

        int client_index = -1; // Nastav na "neplatný" index
        for(int i = 0; i < MAX_CLIENTS; i++){
            if(clients[i].socket_fd == -1){
                client_index = i;
                break; // Místo nalezeno
            }
        }
        // Volné místo nalezeno -> přiřad klienta do pole
        if(client_index != -1){
            clients[client_index].socket_fd = new_socket;           // Nastav socket z acceptu
            clients[client_index].player_id = client_index + 1;     // Nastav index klienta (zde přičteme jedničku)
            clients[client_index].is_active = 1;                    // Připojil se -> je aktivní
            clients[client_index].status = CONNECTED;               // Nastav serverový stav CONNECTED

            ThreadContext *context = (ThreadContext*)malloc(sizeof(ThreadContext));
            context->socket_fd = new_socket;
            context->client_index = client_index;

            // DLOG("ASSIGN slot=%d fd=%d", client_index, new_socket);  // Debugovací výpis

            pthread_t client_thread;
            if(pthread_create(&client_thread, NULL, client_handler, (void*)context) < 0){
                error("Chyba: pthread_create\n");
                send_error(new_socket, "Cannot connect at the moment (pthread_error)");
                close(new_socket);
                clients[client_index].socket_fd = -1;   // Defaultní hodnota pro nepřipojeného klienta
                clients[client_index].is_active = 0;
            } else{
                pthread_detach(client_thread);  // Po skončení vlákna OS udělá cleanup (uvolní paměť, kterou vlákno drželo)
                LOG_INFO("Novy hrac pripojen (FD: %d, ID: %d)\n", new_socket, clients[client_index].player_id);
            }
        } 
        // Nenalezeno volné místo -> informuj klienta a odpoj ho
        else{
            send_error(new_socket, "Cannot connect at the moment (FULL)");
            close(new_socket); // Zavři klienta
        }
        pthread_mutex_unlock(&clients_mutex);
    }
}