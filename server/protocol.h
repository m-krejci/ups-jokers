#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdio.h>
#include <sys/types.h>
#include "config.h"

#define MAGIC_LEN 4                                             // Délka magicu ("JOKE")
#define MSG_TYPE_LEN 4                                          // Délka typu zprávy
#define LENGTH_LEN 4                                            // Délka délky zprávy
#define HEADER_LEN (MAGIC_LEN + MSG_TYPE_LEN + LENGTH_LEN)      // Délka hlavičky
#define NICK_LEN 31                                             // Maximální délka nicknamu
#define MAX_MESSAGE_LEN 9999                                    // Maximální délka zprávy

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
#define TURN "TURN"         // Serverová zpráva informující klienta, že je na tahu
#define WAIT "WAIT"         // Serverová zpráva informující klienta, že čeká na tah protihráče
#define PRDY "PRDY"         // Players ReaDY - počet hráčů ve stavu ready
#define TAKT "TAKT"         // TAKe Thrown - klient chce vzít vyhozenou kartu
#define TAKP "TAKP"         // TAKe Packafe - klient chce vzít kartu z balíčku
#define THRW "THRW"         // THRoW - klient chce vyhodit kartu
#define UNLO "UNLO"         // UNLOad - klient chce vyložit karty (postupky a sety)
#define CLOS "CLOS"         // CLOSe - klient se pokouší zavřít hru
#define CRDS "CRDS"         // CaRDS - server odesílá formátovanou zprávu o kartách hráče
#define RINF "RINF"         // Room INFo - server odesílá informace o místnosti
#define NOTI "NOTI"         // NOTIfication - obecná oznamovací zpráva
#define ADDC "ADDC"         // ADD Card - klient se pokouší přidat kartu k postupkám
#define CSEQ "CSEQ"         // Create SEQuence - klient chce vyložit postupky
#define STAT "STAT"         // STATistics - Informace o stavu hry při každém tahu
#define GEND "GEND"         // Game END - server informuje, že hra skončila zavřením
#define PLAG "PLAG"         // PLay AGain - hráč chce pokračovat po dokončené hře se stejným protivníkem
#define LBBY "LBBY"         // LoBBY - server posílá klienta do lobby
#define PING "PING"         // PING - PING/PONG zpráva / heartbeat
#define PONG "PONG"         // PONG - PING/PONG zpráva / heartbeat
#define PAUS "PAUS"         // PAUSe - server informuje o pozastavené hře z důvodu nedostupnosti jednoho z hráčů
#define RESU "RESU"         // RESUme - server informuje o znovuobnovení hry po opětovném připojení
#define RECO "RECO"         // RECOnnect - server informuje klienta, že reconnect byl úspěšný
#define CNNT "CNNT"         

// Struktura pro hlavičku zprávy
typedef struct{
    char magic[MAGIC_LEN + 1];
    char type_msg[MSG_TYPE_LEN + 1];
    int message_len;
} ProtocolHeader;

// Pole všech zpráv
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
 * @brief Odesílá na socket do té doby, dokud není vše odesláno
 * @param sock Klientský socket
 * @param buf Buffer
 * @param count Délka zprávy
 */
ssize_t custom_send(int sock, const void* buf, size_t count);

/**
 * @brief Kontroluje, jestli přijatá zpráva je mezi definovanými zprávami (typ)
 * @param message Testovaná zpráva
 */
int validate_message(const char* message);

/**
 * @brief Kontroluje délku zprávy a že je každý znak číslo
 * @param message Testovaná zpráva
 * @return 0: ERROR, 1: SUCCESS
 */
int validate_message_len(const char* message);

/**
 * @brief Čte zprávu po částech, parsuje na části hlavičky a validuje pomocí funkcí
 * @param clinet_sock Klientský socket
 * @param header_out Hlavička zprávy (buffer)
 * @param message_out Zpráva (buffer)
 * @returns -1: Pokud je formát hlavičky v nesprávném formátu (délka je nesprávná)
 * @returns -2: Pokud je hlavička nesprávná (!= JOKE)
 * @returns -3: Pokud přijatá zpráva není známá (neexistuje v VALID_MESSAGES)
 */
int read_full_message(int client_sock, ProtocolHeader* header_out, char** message_out);

/**
 * @brief Stará se o build a odesílání zpráv
 * @param client_sock Klientský socket
 * @param type_msg Typ zprávy
 * @param message Skutečná zpráva
 * @return -1: přiliš dlouhá zpráva/chyba, -2 malloc error, -3 odeslání menšího množství dat, 0: SUCCESS
 */
int send_message(int client_sock, const char* type_msg, const char* message);

/**
 * @brief Odesílá zprávu, ale rovnou s errorem
 * @param client_sock Socket klienta
 * @param err_msg Chybová zpráva
 * @return send_message() returns
 */
int send_error(int client_sock, const char* err_msg);

#endif