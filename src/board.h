#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef uint64_t Bitboard;

typedef enum {
    PAWN = 0,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING
} PieceType;

typedef enum {
    WHITE = 0,
    BLACK = 1
} Color;

#define W_PAWN   0
#define W_KNIGHT 1
#define W_BISHOP 2
#define W_ROOK   3
#define W_QUEEN  4
#define W_KING   5
#define B_PAWN   6
#define B_KNIGHT 7
#define B_BISHOP 8
#define B_ROOK   9
#define B_QUEEN  10
#define B_KING   11

#define PIECE(c, t) ((c) * 6 + (t))
#define COLOR_OF(p) ((p) / 6)
#define TYPE_OF(p)  ((p) % 6)
#define EMPTY       (-1)

#define MAX_MOVES 256
#define MAX_HISTORY 4096

typedef struct {
    uint8_t from;
    uint8_t to;
    uint8_t promo;
    uint8_t captured;
    uint8_t flags;
} Move;

typedef struct {
    Move moves[MAX_MOVES];
    int count;
} MoveList;

typedef struct {
    uint64_t bb[12];
    int8_t mailbox[64];
    int side;
    int castle;
    int ep;
    int halfmove;
    int fullmove;
    uint64_t hash;
    uint64_t hashHistory[MAX_HISTORY];
    int historyCount;
} Position;

typedef struct {
    Move move;
    int movedPiece;
    int captured;
    int prevCastle;
    int prevEp;
    int prevHalfmove;
    uint64_t prevHash;
    int prevHistoryCount;
    int rookFrom;
    int rookTo;
} Undo;

Bitboard pawn_attacks(int color, int sq);
Bitboard knight_attacks(int sq);
Bitboard king_attacks(int sq);
Bitboard rook_attacks(int sq, Bitboard occ);
Bitboard bishop_attacks(int sq, Bitboard occ);
Bitboard queen_attacks(int sq, Bitboard occ);

void pos_init(Position* p);
int  pos_from_fen(Position* p, const char* fen);
int  pos_to_fen(const Position* p, char* buf, size_t sz);

Bitboard occ_all(const Position* p);
Bitboard occ_color(const Position* p, int color);

int make_move(Position* p, Move m, Undo* u);
void undo_move(Position* p, const Undo* u);

void gen_moves(Position* p, MoveList* ml, bool only_captures);
bool move_is_legal(Position* p, Move m);

int in_check(const Position* p, int color);
int is_checkmate(const Position* p);
int is_stalemate(const Position* p);
int insufficient_material(const Position* p);

uint64_t perft(Position* p, int depth);

int square_ok(int sq);
const char* move_to_uci(Move m, char* buf);

#endif
