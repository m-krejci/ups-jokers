#include "game_manager.h"
#include "room_manager.h"
#include "client_manager.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static GameInstance *active_games[MAX_ROOMS];
static pthread_mutex_t games_mutex = PTHREAD_MUTEX_INITIALIZER;

void game_init(){
    pthread_mutex_lock(&games_mutex);

    for(int i = 0; i < MAX_ROOMS; i++){
        active_games[i] = NULL;
    }

    pthread_mutex_unlock(&games_mutex);

    srand(time(NULL));
    LOG_INFO("Herní systém nainicializován\n");
}

GameInstance* game_create(struct GameRoom *room){
    if(!room){
        LOG_ERROR("Chyba: neplatné parametry pro game_create\n");
        return NULL;
    }

    GameInstance *game = (GameInstance*)malloc(sizeof(GameInstance));
    if(!game){
        LOG_ERROR("Chyba: Malloc selhal (game_create)\n");
        return NULL;
    }

    memset(game, 0, sizeof(GameInstance));

    game->room_id = room->room_id;
    game->state = GAME_STATE_LOBBY;
    game->current_player_index = 0;
    game->turn_timeout_seconds = 60;
    game->sequence_count = 0;



    game->player_count = 0;
    for(int i = 0; i < MAX_PLAYERS_PER_ROOM; i++){
        int client_index = room->player_indexes[i];
        if(client_index != -1){
            PlayerGameState *player = &game->players[game->player_count];

            player->client_index = client_index;
            player->score = 0;
            player->position = game->player_count;
            player->is_active = 1;
            player->hand_count = 0;
            player->turns_played = 0;
            player->cards_played = 0;
            player->is_ready_for_next_round = 0;
            player->did_closed = 0;
            player->did_thrown = 0;
            player->took_card = 0;

            if(game->player_count == 0){
                player->takes_15 = 1;
            }else{
                player->takes_15 = 0;
            }

            game->player_count++;
        }
    }
    LOG_INFO("Hra vytvořena s %d hráči\n", game->player_count);

    pthread_mutex_lock(&games_mutex);
    active_games[room->room_id] = game;
    pthread_mutex_unlock(&games_mutex);

    return game;
}

void game_destroy(GameInstance *game){
    if(!game){
        return;
    }

    LOG_INFO("Ničím hru pro místnost %d\n", game->room_id);

    pthread_mutex_lock(&games_mutex);
    active_games[game->room_id] = NULL;
    pthread_mutex_unlock(&games_mutex);

    if(game->event_log){
        free(game->event_log);
    }

    free(game);
}

int game_start(GameInstance *game){
    if(!game){
        return -1;
    }

    if(game->state != GAME_STATE_LOBBY){
        LOG_ERROR("Chyba: Hra už běží nebo skončila\n");
        return -1;
    }

    LOG_INFO("Spouštím hru v místnosti %d\n", game->room_id);

    game->state = GAME_STATE_STARTING;

    game_init_deck(game);
    game_deal_cards(game);

    if(game->deck_count > 0){
        game->deck_count--;
        game->discard_deck[0] = game->deck[game->deck_count];
        game->discard_count = 1;
    }

    for(int i = 0; i < MAX_PLAYERS_PER_ROOM; i++){
        PlayerGameState *player = &game->players[i];

        if(player->takes_15){
            game->current_player_index = i;
            LOG_INFO("Hru zahajuje hráč: s ID %d\n", player->client_index);
            
            game->turn_start_time = time(NULL);  // ← PŘIDEJ
            game->state = GAME_STATE_PLAYING;   // ← PŘIDEJ
            
            return 0;  // ← OPRAV z "return;"
}
    }

    game->turn_start_time = time(NULL);
    game->state = GAME_STATE_PLAYING;

    LOG_INFO("Hra spuštěna, první hráč %d\n", game->players[game->current_player_index].client_index);
    return 0;
}

int game_process_move(GameInstance *game, int client_index, const char* action, const char* message_body){
    if(!game || !action){
        return -1;
    }

    if(game->state != GAME_STATE_PLAYING){
        LOG_ERROR("Hra neběží\n");
        return -1;
    }

    // Najdi hráče
    PlayerGameState *player = NULL;
    for(int i = 0; i < game->player_count; i++){
        if(game->players[i].client_index == client_index){
            player = &game->players[i];
            break;
        }
    }

    if(!player){
        LOG_ERROR("Hráč nenalezen\n");
        return -1;
    }

    // ========================================
    // TAKP - Lízni z balíčku
    // ========================================
    if(strcmp(action, "TAKP") == 0){
        // Kontrola, zda už nelízal
        if(player->took_card == 1){
            LOG_INFO("Hráč %d už lízl\n", client_index);
            return -2;
        }
        
        // Kontrola, zda už nevyhodil
        if(player->did_thrown == 1){
            LOG_INFO("Hráč %d už vyhodil\n", client_index);
            return -3;
        }

        // První hráč v prvním kole nelíže (má 15 karet)
        if(player->takes_15 && player->turns_played == 0){
            LOG_INFO("První hráč v prvním kole nelíže\n");
            return -4;
        }

        // Lízni kartu
        if(game->deck_count > 0){
            game->deck_count--;
            player->hand[player->hand_count++] = game->deck[game->deck_count];

            LOG_INFO("Hráč %d si lízl kartu z balíčku\n", client_index);
            player->turns_played++;
            player->took_card = 1;
            
            return 0;
        } else{
            LOG_INFO("Chyba: Prázdný balíček (vytvářím nový)\n");
            game_init_deck(game);
            return -5;
        }
    }
    
    // ========================================
    // TAKT - Lízni z vyhozeného balíčku
    // ========================================
    else if(strcmp(action, "TAKT") == 0){
        // Kontrola, zda už nelízal
        if(player->took_card == 1){
            LOG_INFO("Hráč %d už lízl\n", client_index);
            return -2;
        }
        // printf("%d\n", game->discard_count);
        if(game->discard_count == 0){
            LOG_INFO("V balíčku nejsou žádné karty\n");
            return -3;
        }

        if(player->takes_15 == 1 && player->turns_played == 0){
            return -3;
        }

        // Lízni vrchní kartu z discard pile
        if(game->discard_count > 0){
            game->discard_count--;
            player->hand[player->hand_count++] = game->discard_deck[game->discard_count];

            LOG_INFO("Hráč %d si lízl kartu z odhazovacího balíčku\n", client_index);
            player->turns_played++;
            player->took_card = 1;

            return 0;
        } else{
            LOG_INFO("Chyba: Prázdný odhazovací balíček\n");
            return -1;
        }
    }
    
    // ========================================
    // UNLO - Vyložit karty (postupku)
    // ========================================
    else if (strcmp(action, "UNLO") == 0) {
    if (!message_body) {
        LOG_ERROR("Chybí data karet\n");
        return -1;
    }

    int len = strlen(message_body);

    // každá karta má 2 znaky, min. 3 karty
    if (len < 6 || (len % 2) != 0) {
        LOG_ERROR("Neplatný formát karet\n");
        return -1;
    }

    int parsed_count = len / 2;
    if (parsed_count < 3) {
        LOG_INFO("Kombinace musí mít minimálně 3 karty\n");
        return -1;
    }

    if(parsed_count >= player->hand_count){
        return -69;
    }

    // rozparsování karet
    char card_codes[15][3];
    for (int i = 0; i < parsed_count; i++) {
        card_codes[i][0] = message_body[i * 2];
        card_codes[i][1] = message_body[i * 2 + 1];
        card_codes[i][2] = '\0';
    }

    // ověř, že hráč má všechny karty v ruce
    Card cards_to_unload[15];
    int indices_in_hand[15];
    int used_in_hand[100] = {0};

    for (int i = 0; i < parsed_count; i++) {
        int found = 0;
        for (int j = 0; j < player->hand_count; j++) {
            // Karta musí souhlasit kódem a nesmí být již vybraná pro tento UNLO
            if (!used_in_hand[j] && strcmp(player->hand[j].code, card_codes[i]) == 0) {
                cards_to_unload[i] = player->hand[j];
                indices_in_hand[i] = j;
                used_in_hand[j] = 1; 
                found = 1;
                break;
            }
        }

        if (!found) {
            LOG_INFO("Karta %s není v ruce hráče\n", card_codes[i]);
            return -1;
        }
    }

    // ===== VALIDACE KOMBINACÍ =====
    
    // 1. SET - stejná hodnota, každá barva maximálně jednou
    int is_set = 1;
    
    // Kontrola max 4 karty (4 barvy)
    if (parsed_count > 4) {
        is_set = 0;
    }
    
    if (is_set) {
        // printf("[DEBUG] Zahajuji kontrolu SETu (pocet karet: %d)\n", parsed_count);
        
        // Najdi referenční hodnotu setu (první karta, co není žolík)
        int first_value = -1;
        for (int i = 0; i < parsed_count; i++) {
            if (!cards_to_unload[i].is_joker) {
                first_value = cards_to_unload[i].value;
                break;
            }
        }

        // musí existovat alespoň jedna normální karta
        if (first_value == -1) {
            // printf("[DEBUG] SET neplatny: Nenalezena zadna normalni karta (pouze jokery).\n");
            is_set = 0;
        } //else printf("[DEBUG] Referencni hodnota SETu: %d\n", first_value);

        // kontrola stejné hodnoty
        for (int i = 0; i < parsed_count && is_set; i++) {
            if (!cards_to_unload[i].is_joker && cards_to_unload[i].value != first_value) {
                // printf("[DEBUG] SET neplatny: Karta %s ma hodnotu %d (ocekavano %d).\n", 
                        // cards_to_unload[i].code, cards_to_unload[i].value, first_value);
                is_set = 0;
            }
        }

        // kontrola unikátních barev
        if (is_set) {
            char suits_used[4];
            int suits_count = 0;

            for (int i = 0; i < parsed_count; i++) {
                if (cards_to_unload[i].is_joker){
                    // printf("[DEBUG] Karta %d je JOKER - preskakuji kontrolu barvy.\n", i);
                    continue;
                }

                char current_suit = cards_to_unload[i].suit[0];

                for (int j = 0; j < suits_count; j++) {
                    if (suits_used[j] == current_suit) {
                        // printf("[DEBUG] SET neplatny: Duplicitni barva '%c' u karty %s.\n", 
                                // current_suit, cards_to_unload[i].code);
                        is_set = 0;
                        break;
                    }
                }

                if (!is_set) break;
                
                suits_used[suits_count++] = current_suit;
                // printf("[DEBUG] Pridana barva: %c (celkem unikatnich barev: %d)\n", 
                    // cards_to_unload[i].suit, suits_count);
                // printf("[DEBUG] Pridana barva: %c (HEX: 0x%02x) u karty %d\n", 
                    // cards_to_unload[i].suit, (unsigned char)cards_to_unload[i].suit, i);
            }
        }

        // max 4 barvy
        if (is_set && parsed_count > 4)
            is_set = 0;
    }

    // 2. POSTUPKA (sequence) - stejná barva, rostoucí hodnoty
    int is_sequence = 0;

    if (!is_set) {
        LOG_DEBUG("Zahajuji kontrolu POSTUPKY\n");
        char first_suit = '\0';
        int same_suit = 1;
        
        for (int i = 0; i < parsed_count; i++) {
            if (!cards_to_unload[i].is_joker) {
                if (first_suit == '\0') {
                    first_suit = cards_to_unload[i].suit[0];
                } else if (cards_to_unload[i].suit[0] != first_suit) {
                    LOG_DEBUG("Rozdilne barvy: %c vs %c\n", first_suit, cards_to_unload[i].suit[0]);
                    same_suit = 0;
                    break;
                }
            }
        }

        if (same_suit && first_suit != '\0') {
            Card sorted[15];
            
            for (int attempt = 0; attempt < 2; attempt++) {
                LOG_DEBUG("Pokus %d (0=Eso nizke, 1=Eso vysoke)\n", attempt);
                
                // Naplnění a případná změna Esa
                for (int i = 0; i < parsed_count; i++) {
                    sorted[i] = cards_to_unload[i];
                    if (attempt == 1 && !sorted[i].is_joker && sorted[i].value == 1) {
                        sorted[i].value = 14;
                        LOG_DEBUG("Menim Eso na hodnotu 14\n");
                    }
                }

                // Seřazení (Bubble sort)
                for (int i = 0; i < parsed_count - 1; i++) {
                    for (int j = i + 1; j < parsed_count; j++) {
                        if (sorted[i].is_joker) {
                            // Žolíka vždy posuneme dozadu
                            Card tmp = sorted[i]; sorted[i] = sorted[j]; sorted[j] = tmp;
                        } 
                        else if (!sorted[j].is_joker && sorted[i].value > sorted[j].value) {
                            Card tmp = sorted[i]; sorted[i] = sorted[j]; sorted[j] = tmp;
                        }
                    }
                }

                // Kontrola posloupnosti
                is_sequence = 1;
                int jokers_avail = 0;
                for(int i=0; i<parsed_count; i++) if(sorted[i].is_joker) jokers_avail++;

                int start_idx = 0;
                while(start_idx < parsed_count && sorted[start_idx].is_joker) start_idx++;
                
                if(start_idx >= parsed_count) {
                    is_sequence = 1; // Samí žolíci
                } else {
                    int current_val = sorted[start_idx].value;
                    for (int i = start_idx + 1; i < parsed_count; i++) {
                        if (sorted[i].is_joker) continue;

                        int gap = sorted[i].value - current_val;
                        if (gap == 1) {
                            current_val = sorted[i].value;
                        } else if (gap > 1 && (gap - 1) <= jokers_avail) {
                            LOG_DEBUG("Mezera %d zaplnena zolikem\n", gap);
                            jokers_avail -= (gap - 1);
                            current_val = sorted[i].value;
                        } else {
                            LOG_DEBUG("Prerusena posloupnost: %d -> %d (gap %d)\n", current_val, sorted[i].value, gap);
                            is_sequence = 0;
                            break;
                        }
                    }
                }

                if (is_sequence) {
                    LOG_DEBUG("Postupka potvrzena v pokusu %d!\n", attempt);
                    break;
                }

                // Pokud nemáme eso, druhý pokus nedává smysl
                int has_ace = 0;
                for(int i=0; i<parsed_count; i++) if(!cards_to_unload[i].is_joker && cards_to_unload[i].value == 1) has_ace = 1;
                if(!has_ace) {
                    LOG_DEBUG("Eso nenalezeno, koncim pokusy.\n");
                    break;
                }
            }
        } else {
            LOG_DEBUG("Neplatna barva nebo zadne normalni karty.\n");
        }
    }

    // return -1;
    // 3. POSTUPKA různých barev (run) - rostoucí hodnoty, mohou být různé barvy
    int is_run = 0;
    
    // if (!is_set && !is_sequence) {
    //     // Seřaď podle hodnoty (jokery na konec)
    //     Card sorted[15];
    //     int sorted_count = 0;
        
    //     // Nejdřív přidej normální karty
    //     for (int i = 0; i < parsed_count; i++) {
    //         if (!cards_to_unload[i].is_joker) {
    //             sorted[sorted_count++] = cards_to_unload[i];
    //         }
    //     }
        
    //     // Seřaď normální karty podle hodnoty
    //     for (int i = 0; i < sorted_count - 1; i++) {
    //         for (int j = i + 1; j < sorted_count; j++) {
    //             if (sorted[i].value > sorted[j].value) {
    //                 Card tmp = sorted[i];
    //                 sorted[i] = sorted[j];
    //                 sorted[j] = tmp;
    //             }
    //         }
    //     }
        
    //     // Přidej jokery na konec
    //     for (int i = 0; i < parsed_count; i++) {
    //         if (cards_to_unload[i].is_joker) {
    //             sorted[sorted_count++] = cards_to_unload[i];
    //         }
    //     }

    //     // Ověř posloupnost s jokery
    //     is_run = 1;
    //     int expected = sorted[0].value;
    //     int jokers_used = 0;
        
    //     for (int i = 1; i < parsed_count; i++) {
    //         if (sorted[i].is_joker) {
    //             expected++;
    //             jokers_used++;
    //             continue;
    //         }

    //         int gap = sorted[i].value - expected;
    //         if (gap == 1) {
    //             expected = sorted[i].value;
    //         } else if (gap > 1 && jokers_used > 0) {
    //             is_run = 0;
    //             break;
    //         } else if (gap != 1) {
    //             is_run = 0;
    //             break;
    //         }
    //         expected = sorted[i].value;
    //     }

    //     // Pokud to nefunguje a máme A na začátku, zkus A jako vysoké (14)
    //     if (!is_run) {
    //         int has_ace_at_start = 0;
            
    //         if (sorted[0].value == 1 && !sorted[0].is_joker) {
    //             has_ace_at_start = 1;
    //         }
            
    //         if (has_ace_at_start) {
    //             // Přesuň A za ostatní karty (ale před jokery)
    //             Card ace = sorted[0];
    //             int non_joker_count = 0;
    //             for (int i = 0; i < parsed_count; i++) {
    //                 if (!sorted[i].is_joker) non_joker_count++;
    //             }
                
    //             for (int i = 0; i < non_joker_count - 1; i++) {
    //                 sorted[i] = sorted[i + 1];
    //             }
    //             sorted[non_joker_count - 1] = ace;
    //             sorted[non_joker_count - 1].value = 14;
                
    //             // Znovu ověř posloupnost
    //             is_run = 1;
    //             expected = sorted[0].value;
    //             jokers_used = 0;
                
    //             for (int i = 1; i < parsed_count; i++) {
    //                 if (sorted[i].is_joker) {
    //                     expected++;
    //                     jokers_used++;
    //                     continue;
    //                 }

    //                 int gap = sorted[i].value - expected;
    //                 if (gap == 1) {
    //                     expected = sorted[i].value;
    //                 } else if (gap > 1 && jokers_used > 0) {
    //                     is_run = 0;
    //                     break;
    //                 } else if (gap != 1) {
    //                     is_run = 0;
    //                     break;
    //                 }
    //                 expected = sorted[i].value;
    //             }
    //         }
    //     }
    // }

    // Kontrola platnosti
    if (!is_set && !is_sequence && !is_run) {
        LOG_INFO("Karty netvoří platnou kombinaci (set, postupka nebo run)\n");
        return -1;
    }

    // Výpis typu kombinace (debug)
    if (is_set) {
        LOG_INFO("Kombinace: SET (stejná hodnota)\n");
    } else if (is_sequence) {
        LOG_INFO("Kombinace: POSTUPKA (stejná barva)\n");
    } else if (is_run) {
        LOG_INFO("Kombinace: RUN (různé barvy)\n");
    }

    // Seřaď indexy sestupně (aby se při mazání neposunuly)
    for (int i = 0; i < parsed_count - 1; i++) {
        for (int j = i + 1; j < parsed_count; j++) {
            if (indices_in_hand[i] < indices_in_hand[j]) {
                int tmp = indices_in_hand[i];
                indices_in_hand[i] = indices_in_hand[j];
                indices_in_hand[j] = tmp;
            }
        }
    }

    // Odebrání karet z ruky
    for (int i = 0; i < parsed_count; i++) {
        int idx = indices_in_hand[i];
        for (int j = idx; j < player->hand_count - 1; j++) {
            player->hand[j] = player->hand[j + 1];
        }
        player->hand_count--;
    }

    player->cards_played += parsed_count;

    LOG_INFO("Hráč %d vyložil %d karet\n", client_index, parsed_count);
    
    if (game->sequence_count >= MAX_SEQUENCES) {
        LOG_INFO("Nelze přidat další sekvenci\n");
        return -1;
    }

    CardSequence *seq = &game->sequences[game->sequence_count++];
    seq->count = parsed_count;
    seq->owner_client_index = player->client_index;

    for (int i = 0; i < parsed_count; i++) {
        seq->cards[i] = cards_to_unload[i];
    }

    return 0;
}
    // ADD CARD TO SEQUENCE
   else if (strcmp(action, "ADDC") == 0) {
    if (!message_body) return -1;

    // Rozdělení zprávy podle '|'
    char *pipe = strchr(message_body, '|');
    if (!pipe) return -1;

    *pipe = '\0';
    const char *target_seq_str = message_body; // "AH2H3H"
    char *new_card_code = pipe + 1;      // "4H"

    // 1. Ověření, že hráč má kartu v ruce
    int card_in_hand_idx = -1;
    for (int i = 0; i < player->hand_count; i++) {
        if (strcmp(player->hand[i].code, new_card_code) == 0) {
            card_in_hand_idx = i;
            break;
        }
    }
    if (card_in_hand_idx == -1) return -1; // Karta není v ruce

    // 2. Najití cílové sekvence v game->sequences
    CardSequence *target_seq = NULL;
    for (int i = 0; i < game->sequence_count; i++) {
        // Vytvoříme si dočasný string z kódů karet v sekvenci pro porovnání
        char current_seq_str[MAX_SEQUENCE_CARDS * 3] = "";
        for (int j = 0; j < game->sequences[i].count; j++) {
            strcat(current_seq_str, game->sequences[i].cards[j].code);
        }

        if (strcmp(current_seq_str, target_seq_str) == 0) {
            target_seq = &game->sequences[i];
            break;
        }
    }

    if (!target_seq) return -1; // Sekvence nenalezena
    if (target_seq->count >= MAX_SEQUENCE_CARDS) return -1; // Plno

    // 3. Validace: Pasuje karta do sekvence?
    Card new_card = player->hand[card_in_hand_idx];
    int can_add = 0;
    int add_at_start = 0; // 1 = přidat na začátek, 0 = na konec

    // Zjistíme, zda jde o set (stejné hodnoty) nebo postupku
    int is_set = 1;
    int first_val = target_seq->cards[0].value;
    for(int i = 0; i < target_seq->count; i++) {
        if(!target_seq->cards[i].is_joker && target_seq->cards[i].value != first_val) {
            is_set = 0; 
            break;
        }
    }

    if (is_set) {
        // U setu kontroluj:
        // 1. Stejná hodnota nebo joker
        // 2. Barva ještě není použita (max 4 karty v setu)
        if (new_card.is_joker || new_card.value == first_val) {
            if (target_seq->count >= 4) {
                can_add = 0; // Set může mít max 4 karty
            } else {
                // Kontrola, že barva ještě není použita
                int suit_used = 0;
                if (!new_card.is_joker) {
                    for (int i = 0; i < target_seq->count; i++) {
                        if (!target_seq->cards[i].is_joker && 
                            target_seq->cards[i].suit[0] == new_card.suit[0]) {
                            suit_used = 1;
                            break;
                        }
                    }
                }
                
                if (!suit_used) {
                    can_add = 1;
                }
            }
        }
    } else {
        // POSTUPKA - musí být stejná barva a hodnota navazující
        
        // Najdi první a poslední ne-joker kartu
        // Card first_card = target_seq->cards[0];
        // Card last_card = target_seq->cards[target_seq->count - 1];
        
        // Určení barvy postupky (najdi první ne-joker)
        char seq_suit = '\0';
        for (int i = 0; i < target_seq->count; i++) {
            if (!target_seq->cards[i].is_joker) {
                seq_suit = target_seq->cards[i].suit[0];
                break;
            }
        }
        
        if (seq_suit == '\0') {
            // Všechny karty jsou jokery - přijmi jakoukoli kartu
            can_add = 1;
        } else if (new_card.is_joker) {
            // Joker lze přidat vždy, ale musíme určit stranu
            can_add = 1;

            // 1. Zjistíme, zda je v postupce vysoké Eso (A jako 14)
            int has_high_ace = 0;
            int has_king = 0;
            for (int i = 0; i < target_seq->count; i++) {
                if (!target_seq->cards[i].is_joker) {
                    if (target_seq->cards[i].value == 1) has_high_ace = 1;
                    if (target_seq->cards[i].value == 13) has_king = 1;
                }
            }

            // 2. Pokud je tam A i K, považujeme Eso za vysoké (konec postupky)
            if (has_high_ace && has_king) {
                // Napravo už není místo (za A=14 nic nejde), přidáme ho doleva
                LOG_DEBUG("Detekována vysoká postupka končící Esem, přidávám žolíka doleva.\n");
                add_at_start = 1;
            } else {
                // Standardně přidáváme doprava (na konec)
                add_at_start = 0;
            }
        } else if (new_card.suit[0] == seq_suit) {
            // Stejná barva - zkontroluj hodnotu
            
            // Spočítej skutečnou první a poslední hodnotu v posloupnosti (včetně jokerů)
            int first_real_value = -1;
            int last_real_value = -1;
            int first_real_idx = -1;
            int last_real_idx = -1;
            
            // Najdi první ne-joker
            for (int i = 0; i < target_seq->count; i++) {
                if (!target_seq->cards[i].is_joker) {
                    first_real_value = target_seq->cards[i].value;
                    first_real_idx = i;
                    break;
                }
            }
            
            // Najdi poslední ne-joker
            for (int i = target_seq->count - 1; i >= 0; i--) {
                if (!target_seq->cards[i].is_joker) {
                    last_real_value = target_seq->cards[i].value;
                    last_real_idx = i;
                    break;
                }
            }
            
            if (first_real_value == -1) {
                // Všechny karty jsou jokery - přijmi jakoukoli
                can_add = 1;
            } else {
                // Spočítej, kolik jokerů je před první kartou
                int jokers_before = first_real_idx;
                
                // Spočítej, kolik jokerů je za poslední kartou
                int jokers_after = target_seq->count - last_real_idx - 1;
                
                // Vypočítej skutečnou první hodnotu posloupnosti
                int sequence_start = first_real_value - jokers_before;
                
                // Vypočítej skutečnou poslední hodnotu posloupnosti
                int sequence_end = last_real_value + jokers_after;
                
                int new_value = new_card.value;
                
                // Přidání na konec
                if (new_value == sequence_end + 1) {
                    can_add = 1;
                    add_at_start = 0;
                }
                // Eso za králem (pokud poslední hodnota je 13 a máme joker za ním)
                else if (new_value == 1 && sequence_end == 13) {
                    can_add = 1;
                    add_at_start = 0;
                }
                // Eso za posledním (pokud sequence_end je 13 nebo vyšší díky jokerům)
                else if (new_value == 1 && sequence_end >= 13) {
                    can_add = 1;
                    add_at_start = 0;
                }
                // Přidání na začátek
                else if (new_value == sequence_start - 1) {
                    can_add = 1;
                    add_at_start = 1;
                }
                // Král před esem (A jako 1)
                else if (new_value == 13 && sequence_start == 1) {
                    can_add = 1;
                    add_at_start = 1;
                }
                // Král před začátkem (pokud sequence_start je 1 nebo nižší díky jokerům)
                else if (new_value == 13 && sequence_start <= 1) {
                    can_add = 1;
                    add_at_start = 1;
                }
            }
        }
    }

    if (!can_add) {
        LOG_INFO("Karta %s nejde přiložit k sekvenci\n", new_card_code);
        return -1;
    }

    // 4. Provedení akce
    if (add_at_start) {
        // Přidání na začátek - posuň všechny karty doprava
        for (int i = target_seq->count; i > 0; i--) {
            target_seq->cards[i] = target_seq->cards[i - 1];
        }
        target_seq->cards[0] = new_card;
        target_seq->count++;
    } else {
        // Přidání na konec
        target_seq->cards[target_seq->count++] = new_card;
    }

    // Odebrání karty z ruky hráče
    for (int i = card_in_hand_idx; i < player->hand_count - 1; i++) {
        player->hand[i] = player->hand[i + 1];
    }
    player->hand_count--;
    player->cards_played++;

    LOG_INFO("Hráč %d přiložil kartu %s k sekvenci\n", client_index, new_card_code);
    
    return 0;
}

    
    // ========================================
    // THRW - Vyhodit kartu
    // ========================================
    else if(strcmp(action, "THRW") == 0){
        if(!message_body || strlen(message_body) == 0){
            LOG_ERROR("Chybí kód karty\n");
            return -1;
        }

        // Kontrola, zda už nevyhodil
        if(player->did_thrown == 1){
            LOG_INFO("Hráč %d už vyhodil\n", client_index);
            return -1;
        }

        if(player->took_card == 0){
            if(player->takes_15 && player->turns_played == 0){
                // toto je okey -> prvni kolo s 15 kartami musi vyhodit
            }
            else{
                return -2;
            }
            
        }

        if(player->hand_count == 1){
            return -3;
        }

        // Najdi kartu v ruce
        int card_index = -1;
        
        for(int i = 0; i < player->hand_count; i++){
            char card_code[4];
            snprintf(card_code, sizeof(card_code), "%s%s", 
                    player->hand[i].name, player->hand[i].suit);
            
            // printf("Porovnávám: '%s' vs '%s'\n", message_body, card_code);
            
            if(strcmp(message_body, card_code) == 0){
                card_index = i;
                break;
            }
        }

        if(card_index == -1){
            LOG_INFO("Karta '%s' nenalezena v ruce hráče\n", message_body);
            return -1;
        }

        // Přidej kartu na odhazovací hromádku
        game->discard_deck[game->discard_count++] = player->hand[card_index];

        // Odeber kartu z ruky (shift)
        for(int i = card_index; i < player->hand_count - 1; i++){
            player->hand[i] = player->hand[i + 1];
        }
        player->hand_count--;

        LOG_INFO("Hráč %d vyhodil kartu %s\n", client_index, message_body);

        // Reset flagů pro tohoto hráče
        player->took_card = 0;
        player->did_thrown = 0;
        player->turns_played++;

        // Zkontroluj, zda hráč nevyhrál (prázdná ruka)
        if(player->hand_count == 0){
            LOG_INFO("Hráč %d vyhrál!\n", client_index);
            game->state = GAME_STATE_FINISHED;
            return 0;
        }

        // Přejdi na dalšího hráče
        game_next_player(game);

        return 0;
    }
    
    // ========================================
    // CLOS - Zavřít poslední kartou
    // ========================================
    else if(strcmp(action, "CLOS") == 0){
        if(!message_body || strlen(message_body) == 0){
            LOG_ERROR("Chybí kód karty\n");
            return -1;
        }

        // Kontrola, zda má pouze 1 kartu
        if(player->hand_count != 1){
            LOG_INFO("Pro zavření musíš mít přesně 1 kartu (máš %d)\n", player->hand_count);
            return -1;
        }

        // Najdi kartu v ruce
        char card_code[4];
        snprintf(card_code, sizeof(card_code), "%s%s", 
                player->hand[0].name, player->hand[0].suit);

        if(strcmp(message_body, card_code) != 0){
            LOG_INFO("Karta '%s' nesedí s kartou v ruce '%s'\n", message_body, card_code);
            return -1;
        }

        // Přidej kartu na odhazovací hromádku
        game->discard_deck[game->discard_count++] = player->hand[0];

        // Odeber z ruky
        player->hand_count = 0;

        LOG_INFO("Hráč %d zavřel hru!\n", client_index);

        // Hra končí
        game->state = GAME_STATE_FINISHED;

        return 0;
    }
    
    else{
        LOG_ERROR("Neznámá akce: %s\n", action);
        return -7;
    }
}

int game_end_round(GameInstance *game){
    return 0;
}

int game_end(GameInstance *game){
    if(!game){
        return -1;
    }

    LOG_INFO("Ukončuji hru v místnosti %d\n", game->room_id);

    game->state = GAME_STATE_FINISHED;

    int winner_idx = 0;
    int lowest_score = game->players[0].score;

    for(int i = 1; i < game->player_count; i++){
        if(game->players[i].score < lowest_score){
            lowest_score = game->players[i].score;
            winner_idx = i;
        }
    }

    LOG_INFO("Vítež: hráč %d se skóre %d\n", game->players[winner_idx].client_index, lowest_score);
    return 0;
}

int game_pause(GameInstance *game, const char* reason){
    if(!game){
        return -1;
    }

    LOG_INFO("Pozastavuji hru v místnosti %d: %s\n", game->room_id, reason);

    game->state = GAME_STATE_PAUSED;
    return 0;
}

int game_resume(GameInstance *game){
    if(!game){
        return -1;
    }

    if(game->state != GAME_STATE_PAUSED){
        return -1;
    }

    LOG_INFO("Obnovuji hru v místnosti %d\n", game->room_id);

    game->state = GAME_STATE_PLAYING;
    game->turn_start_time = time(NULL);

    return 0;
}

int game_get_player_cards(GameInstance *game, int client_index, char* buffer, size_t buffer_size){
    if(!game || !buffer || buffer_size == 0){
        return -1;
    }

    PlayerGameState *player = NULL;
    for(int i = 0; i < game->player_count; i++){
        if(game->players[i].client_index == client_index){
            player = &game->players[i];
            break;
        }
    }

    if(!player){
        return -1;
    }

    char *current_pos = buffer;
    size_t remaining_size = buffer_size;
    int written = 0;

    for(int i = 0; i < player->hand_count; i++){
        if (i > 0) {
            written = snprintf(current_pos, remaining_size, "|");
            if (written < 0 || written >= remaining_size) {
                break;
            }
            remaining_size -= written;
            current_pos += written;
        }

        written = snprintf(
            current_pos,
            remaining_size,
            "%s%s",
            player->hand[i].name,
            player->hand[i].suit
        );

        if (written < 0 || written >= remaining_size) {
            break;
        }

        remaining_size -= written;
        current_pos += written;
    }

    // Návratová hodnota: počet zapsaných znaků, nebo -1 v případě chyby
    return buffer_size - remaining_size;
}

int game_get_player_state(GameInstance *game, int client_index, char* buffer, size_t buffer_size){
    if(!game || !buffer){
        return -1;
    }

    PlayerGameState *player = NULL;
    for(int i = 0; i < game->player_count; i++){
        if(game->players[i].client_index == client_index){
            player = &game->players[i];
            break;
        }
    }

    if(!player){
        return -1;
    }
    return 1;
}

int game_get_full_state(GameInstance *game, int client_index, char *buffer, size_t buffer_size){
    if(!game || !buffer || buffer_size == 0) return -1;

    PlayerGameState *player = NULL;
    int enemy_card_count = 0;

    // POZOR: Kontroluj game->player_count místo MAX, nebo ověřuj index
    for(int i = 0; i < game->player_count; i++){
        if(game->players[i].client_index == client_index){
            player = &game->players[i];
        } else {
            // Přičítáme jen pokud je slot platný (např. index není -1)
            if(game->players[i].client_index != -1) {
                enemy_card_count += game->players[i].hand_count;
            }
        }
    }
    if(!player) return -1;

    char *ptr = buffer;
    size_t rem = buffer_size;
    int written;

    // 1. Ruka
    for(int i = 0; i < player->hand_count; i++){
        written = snprintf(ptr, rem, "%s", player->hand[i].code);
        if(written < 0 || (size_t)written >= rem) return -2;
        ptr += written; rem -= written;
    }

    // ODDĚLOVAČ 1
    written = snprintf(ptr, rem, "|");
    if(written < 0 || (size_t)written >= rem) return -2;
    ptr += written; rem -= written; // Tady jsi v původním kódu aktualizaci měl

    // 2. Discard
    if(game->discard_count > 0){
        written = snprintf(ptr, rem, "%s", game->discard_deck[game->discard_count-1].code);
    } else {
        written = 0; // Nic se nepíše
    }
    if(written < 0 || (size_t)written >= rem) return -2;
    ptr += written; rem -= written;

    // ODDĚLOVAČ 2 - TADY CHYBĚLY ptr A rem AKTUALIZACE!
    written = snprintf(ptr, rem, "|");
    if(written < 0 || (size_t)written >= rem) return -2;
    ptr += written; rem -= written; // PŘIDÁNO

    // 3. Postupky
    for(int i = 0; i < game->sequence_count; i++){
        if(i > 0){
            written = snprintf(ptr, rem, ",");
            if(written < 0 || (size_t)written >= rem) return -2;
            ptr += written; rem -= written;
        }
        for(int j = 0; j < game->sequences[i].count; j++){
            written = snprintf(ptr, rem, "%s", game->sequences[i].cards[j].code);
            if(written < 0 || (size_t)written >= rem) return -2;
            ptr += written; rem -= written;
        }
    }

    // ODDĚLOVAČ 3
    written = snprintf(ptr, rem, "|");
    if(written < 0 || (size_t)written >= rem) return -2;
    ptr += written; rem -= written;

    int active_client_index = game->players[game->current_player_index].client_index;

    if(active_client_index == client_index) {
        written = snprintf(ptr, rem, "TURN");
    } else {
        written = snprintf(ptr, rem, "WAIT");
    }

    if(written < 0 || (size_t)written >= rem) return -2;
    ptr += written; rem -= written; // Aktualizace musí být pro obě větve

    // ODDĚLOVAČ 4
    written = snprintf(ptr, rem, "|");
    if(written < 0 || (size_t)written >= rem) return -2;
    ptr += written; rem -= written;

    // 4. Enemy count
    written = snprintf(ptr, rem, "%d", enemy_card_count);
    if(written < 0 || (size_t)written >= rem) return -2;
    ptr += written; rem -= written;

    return (int)(buffer_size - rem);
}

int game_validate_move(GameInstance *game, int client_index, const char* action){
    return 0;
}

int game_check_timeout(GameInstance *game){
    return 0;
}

int game_disconnect_handle(GameInstance *game, int client_index){
    return 0;
}

int game_reconnect_handle(GameInstance *game, int client_index){
    if(!game){
        return -1;
    }

    LOG_INFO("Hráč %d se reconnectoval do hry\n", client_index);

    for(int i = 0; i < game->player_count; i++){
        if(game->players[i].client_index == client_index){
            game->players[i].is_active = 1;

            if(game->state == GAME_STATE_PAUSED){
                game_resume(game);
            }
            return 0;
        }
    }
    return -1;
}

void game_init_deck(GameInstance *game){
    if(!game){
        return;
    }

    const char *suits[] = {"H", "D", "C", "S", "Y"};
    const char *names[] = {"A", "2", "3", "4", "5", "6", "7", "8", "9", "X", "J", "Q", "K", "Y"};
    int values[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 50};

    game->deck_count = 0;
    for(int i = 0; i < 2; i++){
        for(int j = 0; j < 4; j++){
            for(int k = 0; k < 13; k++){
                Card *card = &game->deck[game->deck_count];
                card->id = game->deck_count;
                strncpy(card->name, names[k], sizeof(card->name)-1);
                card->name[sizeof(card->name)-1] = '\0';
                strncpy(card->suit, suits[j], sizeof(card->suit)-1);
                card->suit[sizeof(card->suit)-1] = '\0';
                card->value = values[k];
                card->is_joker = 0;
                game->deck_count++;
                char tmp[3];
                snprintf(tmp, sizeof(tmp), "%s%s", card->name, card->suit);
                strcpy(card->code, tmp);
            }
        }
    }

    for(int i = 0; i < 4; i++){
        Card *card = &game->deck[game->deck_count];
        card->id = game->deck_count;
        strncpy(card->name, names[13], sizeof(card->name)-1);
        card->name[sizeof(card->name)-1] = '\0';
        strncpy(card->suit, suits[4], sizeof(card->suit) - 1);
        card->suit[sizeof(card->suit)-1] = '\0';
        card->value = values[13];
        card->is_joker = 1;
        game->deck_count++;
        char tmp[3];
        snprintf(tmp, sizeof(tmp), "%s%s", card->name, card->suit);
        strcpy(card->code, tmp);
    }

    // for(int i = game->deck_count-1; i >0; i--){
    //     int j = rand() % (i+1);
    //     Card temp = game->deck[i];
    //     game->deck[i] = game->deck[j];
    //     game->deck[j] = temp;
    // }

    LOG_INFO("Balíček inicializován a zamíchán (%d karet)\n", game->deck_count);

}

void game_deal_cards(GameInstance *game){
    if(!game){
        return;
    }

    for(int i = 0; i < game->player_count; i++){
        PlayerGameState *player = &game->players[i];
        int cards_per_player = player->takes_15 ? 15 : 14;
        player->hand_count = 0;

        for(int j = 0; j < cards_per_player && game->deck_count > 0; j++){
            game->deck_count--;
            player->hand[player->hand_count++] = game->deck[game->deck_count];
        }

        LOG_INFO("Hráč %d dostal %d karet\n", player->client_index, player->hand_count);
    }
}

void game_next_player(GameInstance *game){
    if(!game){
        return;
    }

    int start = game->current_player_index;

    do{
        game->current_player_index = (game->current_player_index + 1) % game->player_count;

        if(game->players[game->current_player_index].is_active){
            game->turn_start_time = time(NULL);
            LOG_INFO("Na tahu je hráč %d\n", game->players[game->current_player_index].client_index);
        }
        return;
    } while(game->current_player_index != start);

    LOG_INFO("Žádní aktivní hráči, ukončuji hru\n");
    game->state = GAME_STATE_FINISHED;
}

void game_calculate_scores(GameInstance *game) {
    const char *names[] = {"A", "2", "3", "4", "5", "6", "7", "8", "9", "X", "J", "Q", "K", "Y"};
    int values[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 50};
    int num_variants = sizeof(names) / sizeof(names[0]);

    printf("\n[SCORE_CALC] Zahajuji vypocet skore pro hru.\n");

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < game->player_count; i++) {
        int previous_score = game->players[i].score;
        game->players[i].score = 0; 
        
        printf("  [PLAYER %d] Karty v ruce (%d): ", i, game->players[i].hand_count);

        for (int j = 0; j < game->players[i].hand_count; j++) {
            int found = 0;
            for (int k = 0; k < num_variants; k++) {
                if (strcmp(game->players[i].hand[j].name, names[k]) == 0) {
                    game->players[i].score += values[k];
                    printf("%s(+%d) ", game->players[i].hand[j].name, values[k]);
                    found = 1;
                    break;
                }
            }
            if (!found) {
                printf(" ERR(%s) ", game->players[i].hand[j].name);
            }
        }
        printf("\n  [PLAYER %d] Vysledne skore: %d (predchozi bylo: %d)\n", 
               i, game->players[i].score, previous_score);
    }
    pthread_mutex_unlock(&clients_mutex);
    
    printf("[SCORE_CALC] Vypocet dokoncen.\n\n");
}

int game_is_finished(GameInstance *game){
    if(!game){
        return 0;
    }

    for(int i = 0; i < game->player_count; i++){
        if(game->players[i].hand_count == 0){
            return 1;
        }
    }
    return 0;
}