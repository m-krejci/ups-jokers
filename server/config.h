#ifndef CONFIG_H
#define CONFIG_H

#define SERVER_ADDRESS "192.168.208.195"
#define SERVER_PORT 10000
#define MAX_CLIENTS 10
#define MAX_ROOM_LIMIT 2
#define MAGIC "JOKE"

#define DECK_C_COUNT 108
#define PING_INTERVAL 15
#define PONG_TIMEOUT 35
#define MAX_DISCONNECT_COUNT 3
#define TOKEN_LEN 10


#if SERVER_PORT < 1 || SERVER_PORT > 65535
#error "SERVER_PORT must be between 1 and 65535"
#endif

#endif