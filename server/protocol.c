#include "protocol.h"
#include "logger.h"
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

    // přečtení hlavičky
    if(custom_receive(client_sock, header_buffer, HEADER_LEN) != HEADER_LEN){
        // printf("Chyba: došlo k chybě při čtení hlavičky\n");
        return -1;
    }

    // ********** ROZPARSOVÁNÍ HLAVIČKY **********
    // magic
    strncpy(header_out->magic, header_buffer, MAGIC_LEN);
    header_out->magic[MAGIC_LEN] = '\0';

    // message type
    strncpy(header_out->type_msg, header_buffer + MAGIC_LEN, MSG_TYPE_LEN);
    header_out->type_msg[MSG_TYPE_LEN] = '\0';

    // length (ASCII text z Python klienta)
    strncpy(len_str, header_buffer + MAGIC_LEN + MSG_TYPE_LEN, LENGTH_LEN);
    len_str[LENGTH_LEN] = '\0';

    // 1. test: Kontrola, jestli je magic korektní
    if(strcmp(header_out->magic, "JOKE") != 0){
        send_error(client_sock, "Chyba: Špatný magic");
        LOG_ERROR("Chyba: Špatný magic: %s\n", header_out->magic);
        return -2;
    }

    // 2. test: Kontrola, zda-li existuje přijatá zpráva v množině akceptovaných zpráv
    if(!validate_message(header_out->type_msg)){
        send_message(client_sock, ERRR, "Chyba: Neznámá zpráva");
        LOG_ERROR("Chyba: Neznámá zpráva: %s\n", header_out->type_msg);
        return -3;
    }

    // 3. test: Kontrola, jestli formát délky zprávy je korektní (ASCII čísla)
    if(!validate_message_len(len_str)){
        send_error(client_sock, "Chyba: Nesprávný formát délky zprávy");
        LOG_ERROR("Chyba: Nesprávný formát délky zprávy %s\n", len_str);  // ********** OPRAVENO: místo header_out->message_len **********
        return -4;
    }

    // ********** PŘEVOD ASCII délky na int **********
    message_len = atoi(len_str);  // ********** OPRAVENO: správný int **********
    header_out->message_len = message_len;

    *message_out = (char *)malloc(message_len + 1);
    if(!*message_out){
        return -5;
    }

    // dopřečti zbytek zprávy
    if(custom_receive(client_sock, *message_out, message_len) != message_len){
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