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
    // Inicializace loggeru
    log_init("server.log", LOG_DEBUG);

    // Vymazání dat
    log_delete();
    LOG_INFO("Server startuje");

    // Základní inicializace klientů, místností a hry
    initialize_clients();
    initialize_rooms();
    game_init();

    // Start serveru
    start_server(argc, argv);

    // Řádné uzavření souboru
    LOG_INFO("Server se ukončuje");
    log_close();

    return 0;
}

// ================== SPUŠTĚNÍ ==================
// ./zolik_server <adresa:Optional> <port:Optional>
// ==============================================