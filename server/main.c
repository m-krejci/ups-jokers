#include "server_manager.h"
#include "room_manager.h"
#include "game_manager.h"
#include "client_manager.h"
#include "logger.h"
#include <stdlib.h>
#include <stdio.h>

/**
 * Vstupní bod programu, startuje server.
 */
int main(int argc, char** argv){
    
    log_init("server.log", LOG_DEBUG);
    log_delete();
    LOG_INFO("Server startuje");
    // printf("Starting JOKERS! server...\n");

    initialize_clients();
    initialize_rooms();
    game_init();

    start_server();

    LOG_INFO("Server se ukončuje");
    log_close();



    return 0;
}