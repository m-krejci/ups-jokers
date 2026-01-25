#include "protocol.h"
#include "logger.h"
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <ctype.h>

ssize_t custom_receive(int sock, void* buf, size_t count){
    // celkové množství přečtených
    size_t total_read = 0;

    char* buffer = (char *)buf;

    // dokud délka přečtené zprávy není dlouhá požadované délce, čti
    while(total_read < count){
        ssize_t bytes_read = recv(sock, buffer + total_read, count - total_read, 0);
        if(bytes_read <= 0){
            return bytes_read;
        }
        total_read += bytes_read;
    }
    return total_read;
}

ssize_t custom_send(int sock, const void* buf, size_t count){
    // celkové množství k odeslání
    size_t total_sent = 0;

    const char* buffer = (const char *)buf;

    // dokud délka odeslané zprávy není dlouhá požadované délce, odesílej
    while(total_sent < count){
        ssize_t bytes_sent = send(sock, buffer + total_sent, count - total_sent, 0);
        if(bytes_sent <= 0){
            return bytes_sent;
        }
        total_sent += bytes_sent;
    }
    return total_sent;
}

int validate_message(const char* message){
    size_t len = strlen(message);
    if(len != MSG_TYPE_LEN || message == NULL){
        return 0;
    }

    // kontrola, jestli zpráva existuje v množině zpráv
    for(size_t i = 0; i < VM_COUNT; i++){
        // printf("%s ? %s\n", VALID_MESSAGES[i], message);
        if(strcmp(VALID_MESSAGES[i], message) == 0){
            return 1;
        }
    }
    
    // zpráva nenalezena
    return 0;
}

int validate_message_len(const char* message){
    // Kdyby se stalo, že délka není 4, vrať false
    size_t len = strlen(message);
    if(len != LENGTH_LEN){
        return 0;
    }

    for(size_t i = 0; i < len; i++){
        // pokud některý Bajt ze slova není číslo, vrať false
        if(!isdigit(message[i])){
            return 0;
        }
    }
    // all good
    return 1;
}

int read_full_message(int client_sock, ProtocolHeader* header_out, char** message_out){
    char header_buffer[HEADER_LEN + 1];
    char len_str[LENGTH_LEN + 1];
    int message_len;
    char c;

    // const char *MAG = "JOKE";
    char win[MAGIC_LEN] = {0};
    int filled = 0;

    int garbage = 0;

    // najdi MAGIC "JOKE"
    while (1) {
        ssize_t r = recv(client_sock, &c, 1, 0);
        if (r <= 0) return -1;

        if (filled < MAGIC_LEN) win[filled++] = c;
        else {
            memmove(win, win + 1, MAGIC_LEN - 1);
            win[MAGIC_LEN - 1] = c;
        }

        if (filled == MAGIC_LEN && memcmp(win, MAGIC, MAGIC_LEN) == 0) {
            memcpy(header_buffer, MAGIC, MAGIC_LEN);
            break;
        }

        if (++garbage >= MAX_GARBAGE){
            send_error(client_sock, "Invalid data");
             return -2; // moc bordelu
        }
    }

    // dočti zbytek hlavičky po MAGIC
    int rest = HEADER_LEN - MAGIC_LEN;
    if (custom_receive(client_sock, header_buffer + MAGIC_LEN, rest) != rest) return -1;
    header_buffer[HEADER_LEN] = '\0';

    // parse hlavičky (tohle máš OK)
    strncpy(header_out->magic, header_buffer, MAGIC_LEN);
    header_out->magic[MAGIC_LEN] = '\0';

    strncpy(header_out->type_msg, header_buffer + MAGIC_LEN, MSG_TYPE_LEN);
    header_out->type_msg[MSG_TYPE_LEN] = '\0';

    strncpy(len_str, header_buffer + MAGIC_LEN + MSG_TYPE_LEN, LENGTH_LEN);
    len_str[LENGTH_LEN] = '\0';

    if (strcmp(header_out->magic, MAGIC) != 0) return -2;
    if (!validate_message(header_out->type_msg)) return -3;
    if (!validate_message_len(len_str)) return -4;

    message_len = atoi(len_str);
    header_out->message_len = message_len;

    *message_out = malloc(message_len + 1);
    if (!*message_out) return -5;

    if (custom_receive(client_sock, *message_out, message_len) != message_len) {
        free(*message_out);
        *message_out = NULL;
        return -6;
    }
    (*message_out)[message_len] = '\0';
    return 0;
}



int send_message(int client_sock, const char* type_msg, const char* message){
    int msg_len = strlen(message);
    if(msg_len > MAX_MESSAGE_LEN){
        return -1;
    }

    char header_buffer[HEADER_LEN + 1];
    if (msg_len < 0 || msg_len > 9999) {
    // Chyba nebo logování, protože zpráva je příliš dlouhá/krátká
        return -1; 
    }

    // zapíšeme do bufferu magic + type_msg + msg_len
    snprintf(header_buffer, HEADER_LEN + 1, "%s%-4s%04d", MAGIC, type_msg, msg_len);

    int total_len = HEADER_LEN + msg_len;
    char* packet = malloc(total_len+1);
    if(!packet){
        return -2;
    }

    packet[total_len] = '\0';
    memcpy(packet, header_buffer, HEADER_LEN);
    memcpy(packet + HEADER_LEN, message, msg_len);

    int sent = custom_send(client_sock, packet, total_len);
    LOG_INFO("Sending to client socket %d: %s\n", client_sock, packet);
    free(packet);

    if(sent != total_len){
        return -3;
    }

    
    return 0;
}

int send_error(int client_sock, const char* err_msg){
    return send_message(client_sock, "ERRR", err_msg);
}