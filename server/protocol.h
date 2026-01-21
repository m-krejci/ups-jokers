#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdio.h>
#include <sys/types.h>
#include "config.h"

#define MAGIC_LEN 4
#define MSG_TYPE_LEN 4
#define LENGTH_LEN 4
#define HEADER_LEN (MAGIC_LEN + MSG_TYPE_LEN + LENGTH_LEN)
#define NICK_LEN 31

// definice zprav
#define LOGI "LOGI"         // Zpráva o přihlášení uživatele (klient si sám hlídá, aby nebylo prázdné) -- LOGIn
#define LOGO "LOGO"         // Zpráva o odhlášení uživatele (například z důvodu změny jména) -- LOGOut
#define OKAY "OKAY"         // Potvrzovací zpráva -- OKAY
#define QUIT "QUIT"         // Zpráva přicházející od klienta o odhlášení ze serveru -- QUIT
#define RCRT "RCRT"         // Žádost o vytvoření místnosti -- Room CReaTe
#define RDIS "RDIS"         // Zpráva o odpojení klienta z místnosti, ve které se nacházel -- Room DISconnect
#define RCNT "RCNT"         // Žádost klienta o připojení do jedné z existujících místností -- Room CoNnecT
#define RLIS "RLIS"         // Žádost klienta o vypsání dostupných místností -- Room LISt
#define OCRT "OCRT"         // Serverová potvrzovací zpráva o vytvoření místnosti -- Okay CReaTe
#define ECRT "ECRT"         // Serverová chybová zpráva o vytvoření místnosti (dosažen maximální počet) -- Error CReaTe
#define OCNT "OCNT"         // Serverová potvrzovací zpráva o připojení do místnosti -- Okay CoNnecT
#define ECNT "ECNT"         // Serverová chybová zpráva o připojení do místnosti -- Error CoNnecT
#define ELIS "ELIS"         // Serverová chybová zpráva - žádné nalezené místnosti -- Error LISt rooms
#define ERRR "ERRR"         // Obecná serverová chyba
#define ODIS "ODIS"         // Serverová potvrzovací zpráva o opuštění místnosti
#define EDIS "EDIS"         // Serverová chybová zpráva o opuštění místností
#define REDY "REDY"         // Klientská zpráva o připravenosti klienta začít hrát
#define EEDY "EEDY"         // Serverová chybová zpráva o připravenosti hráče
#define OEDY "OEDY"         // Serverová potvrzovací zpráva o připravenosti hráče
#define STRT "STRT"         // Klientská zpráva o tom, že hra může začít
#define ESTR "ESTR"         // Serverová zpráva o chybě při pokusu o start hry
#define BOSS "BOSS"         // Serverová zpráva pro klienta, že je vlastníkem místnosti a jen on může hru začít
#define TURN "TURN"
#define WAIT "WAIT"
#define PRDY "PRDY"         // Players ReaDY
#define TAKT "TAKT"
#define TAKP "TAKP"
#define THRW "THRW"
#define UNLO "UNLO"
#define CLOS "CLOS"
#define CRDS "CRDS"
#define RINF "RINF"
#define NOTI "NOTI"
#define ADDC "ADDC"
#define CSEQ "CSEQ"
#define STAT "STAT"
#define GEND "GEND"
#define PLAG "PLAG"
#define LBBY "LBBY"
#define PING "PING"
#define PONG "PONG"
#define PAUS "PAUS"
#define RESU "RESU"
#define RECO "RECO"
#define CNNT "CNNT"


#define MAX_MESSAGE_LEN 9999

typedef struct{
    char magic[MAGIC_LEN + 1];
    char type_msg[MSG_TYPE_LEN + 1];
    int message_len;
    //char message_len[LENGTH_LEN + 1];
} ProtocolHeader;

static const char* const VALID_MESSAGES[] = {
    "LOGI",
    "LOGO",
    "OKAY",
    "QUIT",
    "RCRT",
    "RDIS",
    "RCNT",
    "RLIS",
    "ERRR",
    "OCRT",
    "OCNT",
    "ECRT",
    "ECNT",
    "ODIS",
    "EDIS",
    "REDY",
    "EEDY",
    "OEDY",
    "BOSS",
    "TURN",
    "WAIT",
    "PRDY",
    "STRT",
    "CRDS",
    "RINF",
    "NOTI",
    "TAKP",
    "THRW",
    "TAKT",
    "UNLO",
    "ADDC",
    "CSEQ",
    "STAT",
    "GEND",
    "CLOS",
    "PLAG",
    "LBBY",
    "PING",
    "PONG",
    "PAUS",
    "RESU",
    "RECO",
    "CNNT"
};

static const size_t VM_COUNT = sizeof(VALID_MESSAGES) / sizeof(VALID_MESSAGES[0]);

/**
 * Přijímání zpráv, které se stará o plné přijetí zprávy. Cyklem přijímá zprávy, dokud zpráva není celá.
 * @param sock socket klienta, od kterého přijímá zprávu
 * @param buf buffer pro ukládání zprávy
 * @param count délka přijímané zprávy
 */
ssize_t custom_receive(int sock, void* buf, size_t count);

/**
 * 
 */
ssize_t custom_send(int sock, const void* buf, size_t count);

/**
 * 
 */
int validate_message(const char* message);

/**
 * 
 */
int validate_message_len(const char* message);

/**
 * @returns -1: Pokud je formát hlavičky v nesprávném formátu (délka je nesprávná)
 * @returns -2: Pokud je hlavička nesprávná (!= JOKE)
 * @returns -3: Pokud přijatá zpráva není známá (neexistuje v VALID_MESSAGES)
 */
int read_full_message(int client_sock, ProtocolHeader* header_out, char** message_out);

/**
 * 
 */
int send_message(int client_sock, const char* type_msg, const char* message);

/**
 *
 */
int send_error(int client_sock, const char* err_msg);

#endif