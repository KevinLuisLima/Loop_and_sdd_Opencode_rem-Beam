#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "moves.h"
#include "position.h"

int gen_pseudo_moves(const Position *p, Move *out, int captures_only);
int gen_legal_moves(Position *p, Move *out);
int move_is_legal(Position *p, Move m);
Move parse_move(const char *s);
void move_to_uci(Move m, char *buf);

#endif