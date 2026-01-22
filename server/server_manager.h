#ifndef SERVER_MANAGER_H
#define SERVER_MANAGER_H

#include "config.h"

/**
 * @brief Provádí základní síťovou inicializaci (socket, bind, listen, accept, [send, receive], close)
 * @param argc Počet argumentů předaných přes argc
 * @param argv Pole argumentů z cmd
 */
void start_server(int argc, char **argv);

#endif
