#ifndef CONFIG_H
#define CONFIG_H

// _______________________________
// ________ SÍŤOVÝ CONFIG ________
// _______________________________

// Adresa serveru -> bez údaje bere localhost "192.168.208.195"
#define SERVER_ADDRESS ""       
// Port serveru, na kterém bude naslouchat
#define SERVER_PORT 10000
// Maximální množství klientů k obsloužení
#define MAX_CLIENTS 10
// Magic pro protokolové zprávy
#define MAGIC "JOKE"
// Interval PING zprávy
#define PING_INTERVAL 15
// Maximální délka timeoutu klienta na odpověď
#define PONG_TIMEOUT 35
// Definice adresy localhostu
#define LOCALHOST "127.0.0.1"
// Definice adresy broadcastu
#define BROADCAST "255.255.255.255"
// _________________________________




// _____________________________
// ________ HERNÍ SETUP ________
// _____________________________

// Maximální počet karet pro hru -> 4 žolíci
#define DECK_C_COUNT 108
// Maximální počet klientů v místnosti
#define MAX_ROOM_LIMIT 2
// Maximální počet disconnectů
#define MAX_DISCONNECT_COUNT 3
// Definice délky tokenu pro reconnect
#define TOKEN_LEN 3
// Definice počtu přijatelných argumentů z cmd line
#define ARGUMENT_COUNT 3
// ______________________________





// ________ NASTAVENÍ HERNÍ MÍSTNOSTI (game_manager.h) ________
// Maximální délka názvu místnosti
#define MAX_ROOM_NAME 10
// 
#define MAX_ROOM_PLAYERS 2
#define MAX_HAND_CARD 15
#define DECK_CARDS_COUNT 108
#define MAX_SEQUENCE_CARDS 15
#define MAX_SEQUENCES 50

// Nastavení místnosti (room_manager.h)
#define MAX_ROOMS 7
#define MAX_PLAYERS_PER_ROOM 2
#define ROOM_NAME_LEN 15
// ___________________________________________________________




// ________ TIMEOUT INTERVAL (server_manager.h) ________
#define TIMEOUT_CHECK_INTERVAL 3
// _____________________________________________________


#define MAX_GARBAGE 16




// Test správně zadaného portu
#if SERVER_PORT < 0 || SERVER_PORT > 65535
#error "SERVER_PORT must be between 1 and 65535"
#endif

#endif