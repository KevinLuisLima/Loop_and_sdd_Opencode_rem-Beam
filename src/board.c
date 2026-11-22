#include "board.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static uint64_t ZOBRIST[12][64];
static uint64_t Z_SIDE, Z_CASTLE[16], Z_EP[64];
static void init_zobrist(void) {
    unsigned int seed = 12345;
#define RN (seed = seed * 1103515245u + 12345u)
    for (int p = 0; p < 12; p++)
        for (int sq = 0; sq < 64; sq++) {
            RN; uint64_t a = seed;
            RN; uint64_t b = seed;
            ZOBRIST[p][sq] = (a << 32) | b;
        }
    RN; uint64_t a = seed; RN; uint64_t b = seed; Z_SIDE = (a<<32)|b;
    for (int i = 0; i < 16; i++) { RN; a=seed; RN; b=seed; Z_CASTLE[i]=(a<<32)|b; }
    for (int sq = 0; sq < 64; sq++) { RN; a=seed; RN; b=seed; Z_EP[sq]=(a<<32)|b; }
#undef RN
}


Bitboard pawn_attacks(int color, int sq) {
    Bitboard b = 0;
    int r = sq / 8, c = sq % 8;
    if (color == WHITE) {
        if (r < 7) {
            if (c > 0) b |= 1ULL << (sq + 7);
            if (c < 7) b |= 1ULL << (sq + 9);
        }
    } else {
        if (r > 0) {
            if (c > 0) b |= 1ULL << (sq - 9);
            if (c < 7) b |= 1ULL << (sq - 7);
        }
    }
    return b;
}

Bitboard knight_attacks(int sq) {
    Bitboard b = 0;
    int r = sq / 8, c = sq % 8;
    static const int kr[8] = { -2,-2,-1,-1,1,1,2,2 };
    static const int kc[8] = { -1,1,-2,2,-2,2,-1,1 };
    for (int i = 0; i < 8; i++) {
        int nr = r + kr[i], nc = c + kc[i];
        if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8) b |= 1ULL << (nr * 8 + nc);
    }
    return b;
}

Bitboard king_attacks(int sq) {
    Bitboard b = 0;
    int r = sq / 8, c = sq % 8;
    static const int kr[8] = { -1,-1,-1,0,0,1,1,1 };
    static const int kc[8] = { -1,0,1,-1,1,-1,0,1 };
    for (int i = 0; i < 8; i++) {
        int nr = r + kr[i], nc = c + kc[i];
        if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8) b |= 1ULL << (nr * 8 + nc);
    }
    return b;
}

Bitboard rook_attacks(int sq, Bitboard occ) {
    Bitboard b = 0;
    int r = sq / 8, c = sq % 8;
    for (int rr = r + 1; rr < 8; rr++) { int s = rr * 8 + c; b |= 1ULL << s; if (occ & (1ULL << s)) break; }
    for (int rr = r - 1; rr >= 0; rr--) { int s = rr * 8 + c; b |= 1ULL << s; if (occ & (1ULL << s)) break; }
    for (int cc = c + 1; cc < 8; cc++) { int s = r * 8 + cc; b |= 1ULL << s; if (occ & (1ULL << s)) break; }
    for (int cc = c - 1; cc >= 0; cc--) { int s = r * 8 + cc; b |= 1ULL << s; if (occ & (1ULL << s)) break; }
    return b;
}

Bitboard bishop_attacks(int sq, Bitboard occ) {
    Bitboard b = 0;
    int r = sq / 8, c = sq % 8;
    for (int i = 1; r + i < 8 && c + i < 8; i++) { int s = (r+i)*8 + (c+i); b |= 1ULL<<s; if (occ&(1ULL<<s)) break; }
    for (int i = 1; r + i < 8 && c - i >= 0; i++) { int s = (r+i)*8 + (c-i); b |= 1ULL<<s; if (occ&(1ULL<<s)) break; }
    for (int i = 1; r - i >= 0 && c + i < 8; i++) { int s = (r-i)*8 + (c+i); b |= 1ULL<<s; if (occ&(1ULL<<s)) break; }
    for (int i = 1; r - i >= 0 && c - i >= 0; i++) { int s = (r-i)*8 + (c-i); b |= 1ULL<<s; if (occ&(1ULL<<s)) break; }
    return b;
}

Bitboard queen_attacks(int sq, Bitboard occ) {
    return rook_attacks(sq, occ) | bishop_attacks(sq, occ);
}

static uint64_t compute_hash(const Position* p) {
    init_zobrist();
    uint64_t h = 0;
    for (int sq = 0; sq < 64; sq++) {
        int pc = p->mailbox[sq];
        if (pc >= 0) h ^= ZOBRIST[pc][sq];
    }
    if (p->side == BLACK) h ^= Z_SIDE;
    h ^= Z_CASTLE[p->castle & 15];
    if (p->ep >= 0) h ^= Z_EP[p->ep];
    return h;
}

void pos_init(Position* p) {
    memset(p, 0, sizeof(Position));
    p->side = WHITE;
    p->castle = 0;
    p->ep = -1;
    p->halfmove = 0;
    p->fullmove = 1;
    p->historyCount = 0;
    for (int i = 0; i < 64; i++) p->mailbox[i] = -1;
    init_zobrist();
}

static int piece_from_char(char c) {
    const char* w = "PNBRQK";
    const char* b = "pnbrqk";
    for (int i = 0; i < 6; i++) if (c == w[i]) return W_PAWN + i;
    for (int i = 0; i < 6; i++) if (c == b[i]) return B_PAWN + i;
    return -1;
}

int pos_from_fen(Position* p, const char* fen) {
    pos_init(p);
    char buf[512];
    strncpy(buf, fen, 511); buf[511] = 0;
    char* parts[6];
    int nump = 0;
    char* tok = strtok(buf, " ");
    while (tok && nump < 6) { parts[nump++] = tok; tok = strtok(NULL, " "); }
    if (nump < 4) return -1;

    int rank = 7, file = 0;
    const char* b = parts[0];
    for (const char* c = b; *c; c++) {
        if (*c == '/') { rank--; file = 0; continue; }
        if (*c >= '1' && *c <= '8') { file += *c - '0'; continue; }
        int pc = piece_from_char(*c);
        if (pc < 0) return -1;
        if (file > 7 || rank < 0) return -1;
        int sq = rank * 8 + file;
        p->bb[pc] |= 1ULL << sq;
        p->mailbox[sq] = pc;
        file++;
    }
    if (parts[1][0] == 'w') p->side = WHITE;
    else if (parts[1][0] == 'b') p->side = BLACK;
    else return -1;

    p->castle = 0;
    if (strcmp(parts[2], "-") != 0) {
        for (const char* c = parts[2]; *c; c++) {
            if (*c == 'K') p->castle |= 1;
            else if (*c == 'Q') p->castle |= 2;
            else if (*c == 'k') p->castle |= 4;
            else if (*c == 'q') p->castle |= 8;
            else return -1;
        }
    }

    p->ep = -1;
    if (strcmp(parts[3], "-") != 0) {
        if (strlen(parts[3]) < 2) return -1;
        int f = parts[3][0] - 'a';
        int r = parts[3][1] - '1';
        if (f < 0 || f > 7 || r < 0 || r > 7) return -1;
        p->ep = r * 8 + f;
    }

    if (nump >= 5) p->halfmove = atoi(parts[4]);
    if (nump >= 6) p->fullmove = atoi(parts[5]);

    int wk = __builtin_popcountll(p->bb[W_KING]);
    int bk = __builtin_popcountll(p->bb[B_KING]);
    if (wk != 1 || bk != 1) return -1;

    p->hash = compute_hash(p);
    p->historyCount = 0;
    p->hashHistory[p->historyCount++] = p->hash;
    return 0;
}

int pos_to_fen(const Position* p, char* buf, size_t sz) {
    char tmp[256];
    int idx = 0;
    for (int r = 7; r >= 0; r--) {
        int empty = 0;
        for (int c = 0; c < 8; c++) {
            int sq = r*8+c;
            int pc = p->mailbox[sq];
            if (pc < 0) { empty++; continue; }
            if (empty) { tmp[idx++] = '0' + empty; empty = 0; }
            const char* pieces = "PNBRQKpnbrqk";
            tmp[idx++] = pieces[pc];
        }
        if (empty) tmp[idx++] = '0' + empty;
        if (r > 0) tmp[idx++] = '/';
    }
    tmp[idx] = 0;
    char cstr[8]; int ci = 0;
    if (p->castle & 1) cstr[ci++] = 'K';
    if (p->castle & 2) cstr[ci++] = 'Q';
    if (p->castle & 4) cstr[ci++] = 'k';
    if (p->castle & 8) cstr[ci++] = 'q';
    if (ci == 0) cstr[ci++] = '-';
    cstr[ci] = 0;
    char epstr[4];
    if (p->ep >= 0) { epstr[0] = 'a' + (p->ep % 8); epstr[1] = '1' + (p->ep / 8); epstr[2] = 0; }
    else { epstr[0] = '-'; epstr[1] = 0; }
    snprintf(buf, sz, "%s %c %s %s %d %d", tmp, p->side == WHITE ? 'w' : 'b', cstr, epstr, p->halfmove, p->fullmove);
    return 0;
}

Bitboard occ_all(const Position* p) {
    Bitboard b = 0;
    for (int i = 0; i < 12; i++) b |= p->bb[i];
    return b;
}

Bitboard occ_color(const Position* p, int color) {
    Bitboard b = 0;
    for (int t = 0; t < 6; t++) b |= p->bb[color*6+t];
    return b;
}

static void remove_piece(Position* p, int sq) {
    int pc = p->mailbox[sq];
    if (pc >= 0) p->bb[pc] &= ~(1ULL << sq);
    p->mailbox[sq] = -1;
}

static void add_piece(Position* p, int sq, int pc) {
    p->bb[pc] |= 1ULL << sq;
    p->mailbox[sq] = pc;
}

int in_check(const Position* p, int color) {
    Bitboard kingbb = p->bb[color*6+KING];
    if (!kingbb) return 1;
    int ksq = __builtin_ctzll(kingbb);
    Bitboard occ = occ_all(p);
    int enemy = 1 - color;
    Bitboard att = 0;
    Bitboard bb;
    bb = p->bb[enemy*6+PAWN]; while (bb) { int s=__builtin_ctzll(bb); bb&=bb-1; att |= pawn_attacks(enemy, s); }
    bb = p->bb[enemy*6+KNIGHT]; while (bb) { int s=__builtin_ctzll(bb); bb&=bb-1; att |= knight_attacks(s); }
    bb = p->bb[enemy*6+BISHOP]; while (bb) { int s=__builtin_ctzll(bb); bb&=bb-1; att |= bishop_attacks(s, occ); }
    bb = p->bb[enemy*6+ROOK]; while (bb) { int s=__builtin_ctzll(bb); bb&=bb-1; att |= rook_attacks(s, occ); }
    bb = p->bb[enemy*6+QUEEN]; while (bb) { int s=__builtin_ctzll(bb); bb&=bb-1; att |= queen_attacks(s, occ); }
    bb = p->bb[enemy*6+KING]; while (bb) { int s=__builtin_ctzll(bb); bb&=bb-1; att |= king_attacks(s); }
    return (att & (1ULL << ksq)) ? 1 : 0;
}

int make_move(Position* p, Move m, Undo* u) {
    memset(u, 0, sizeof(Undo));
    u->move = m;
    u->prevCastle = p->castle;
    u->prevEp = p->ep;
    u->prevHalfmove = p->halfmove;
    u->prevHash = p->hash;
    u->prevHistoryCount = p->historyCount;
    u->captured = -1;
    u->rookFrom = -1; u->rookTo = -1;
    u->movedPiece = 0;

    int from = m.from, to = m.to;
    int piece = p->mailbox[from];
    int color = COLOR_OF(piece);
    init_zobrist();
    u->movedPiece = piece;

    if (m.flags & 2) {
        int capSq = (color == WHITE) ? to - 8 : to + 8;
        u->captured = p->mailbox[capSq];
        remove_piece(p, capSq);
    } else {
        int cap = p->mailbox[to];
        if (cap >= 0) {
            u->captured = cap;
            remove_piece(p, to);
        }
    }

    remove_piece(p, from);
    int newPiece = piece;
    if (m.promo != 0) {
        int pt = m.promo;
        if (pt == 1) newPiece = color*6 + KNIGHT;
        else if (pt == 2) newPiece = color*6 + BISHOP;
        else if (pt == 3) newPiece = color*6 + ROOK;
        else newPiece = color*6 + QUEEN;
    }
    add_piece(p, to, newPiece);

    if (m.flags & 1) {
        if (to == 6) { u->rookFrom = 7; u->rookTo = 5; }
        else if (to == 2) { u->rookFrom = 0; u->rookTo = 3; }
        else if (to == 62) { u->rookFrom = 63; u->rookTo = 61; }
        else if (to == 58) { u->rookFrom = 56; u->rookTo = 59; }
        int rookPiece = p->mailbox[u->rookFrom];
        remove_piece(p, u->rookFrom);
        add_piece(p, u->rookTo, rookPiece);
    }

    if (piece == W_KING) { p->castle &= ~3; }
    if (piece == B_KING) { p->castle &= ~12; }
    if (from == 7 || to == 7) p->castle &= ~1;
    if (from == 0 || to == 0) p->castle &= ~2;
    if (from == 63 || to == 63) p->castle &= ~4;
    if (from == 56 || to == 56) p->castle &= ~8;

    p->ep = -1;
    if (TYPE_OF(piece) == PAWN && abs(from - to) == 16) {
        p->ep = (from + to) / 2;
    }

    p->side = 1 - color;

    if (TYPE_OF(piece) == PAWN || u->captured >= 0) p->halfmove = 0;
    else p->halfmove++;

    if (color == BLACK) p->fullmove++;

    p->hash = compute_hash(p);

    if (p->historyCount < MAX_HISTORY) {
        p->hashHistory[p->historyCount++] = p->hash;
    }
    return 0;
}

void undo_move(Position* p, const Undo* u) {
    Move m = u->move;
    int from = m.from, to = m.to;
    int color = 1 - p->side;

    if (m.flags & 1) {
        int rookPiece = (color == WHITE) ? W_ROOK : B_ROOK;
        remove_piece(p, u->rookTo);
        add_piece(p, u->rookFrom, rookPiece);
    }

    remove_piece(p, to);
    add_piece(p, from, u->movedPiece);

    if (m.flags & 2) {
        int capSq = (color == WHITE) ? to - 8 : to + 8;
        if (u->captured >= 0) add_piece(p, capSq, u->captured);
    } else if (u->captured >= 0) {
        add_piece(p, to, u->captured);
    }

    p->side = color;
    p->castle = u->prevCastle;
    p->ep = u->prevEp;
    p->halfmove = u->prevHalfmove;
    if (color == BLACK) p->fullmove--;
    p->historyCount = u->prevHistoryCount;
    p->hash = u->prevHash;
}

static int square_attacked(const Position* p, int sq, int byColor) {
    Bitboard occ = occ_all(p);
    Bitboard bb;
    bb = p->bb[byColor*6+PAWN]; while (bb) { int s=__builtin_ctzll(bb); bb&=bb-1; if (pawn_attacks(byColor, s) & (1ULL<<sq)) return 1; }
    bb = p->bb[byColor*6+KNIGHT]; while (bb) { int s=__builtin_ctzll(bb); bb&=bb-1; if (knight_attacks(s) & (1ULL<<sq)) return 1; }
    bb = p->bb[byColor*6+BISHOP]; while (bb) { int s=__builtin_ctzll(bb); bb&=bb-1; if (bishop_attacks(s, occ) & (1ULL<<sq)) return 1; }
    bb = p->bb[byColor*6+ROOK]; while (bb) { int s=__builtin_ctzll(bb); bb&=bb-1; if (rook_attacks(s, occ) & (1ULL<<sq)) return 1; }
    bb = p->bb[byColor*6+QUEEN]; while (bb) { int s=__builtin_ctzll(bb); bb&=bb-1; if (queen_attacks(s, occ) & (1ULL<<sq)) return 1; }
    bb = p->bb[byColor*6+KING]; while (bb) { int s=__builtin_ctzll(bb); bb&=bb-1; if (king_attacks(s) & (1ULL<<sq)) return 1; }
    return 0;
}

static void add_move(MoveList* ml, int from, int to, int promo, int captured, int flags) {
    if (ml->count >= MAX_MOVES) return;
    Move m; m.from=from; m.to=to; m.promo=promo; m.captured=captured; m.flags=flags;
    ml->moves[ml->count++] = m;
}

void gen_moves(Position* p, MoveList* ml, bool only_captures) {
    ml->count = 0;
    int color = p->side;
    /* pawns */
    for (int sq = 0; sq < 64; sq++) {
        int pc = p->mailbox[sq];
        if (pc < 0 || COLOR_OF(pc) != color || TYPE_OF(pc) != PAWN) continue;
        if (color == WHITE) {
            int to = sq + 8;
            if (to < 64 && p->mailbox[to] < 0) {
                if (to / 8 == 7) {
                    if (!only_captures) for (int pr = 1; pr <= 4; pr++) add_move(ml, sq, to, pr, 255, 0);
                } else {
                    if (!only_captures) add_move(ml, sq, to, 0, 255, 0);
                    if (sq / 8 == 1) {
                        int to2 = sq + 16;
                        if (p->mailbox[to2] < 0) {
                            if (!only_captures) add_move(ml, sq, to2, 0, 255, 0);
                        }
                    }
                }
            }
            for (int d = -1; d <= 1; d += 2) {
                int c = sq % 8 + d;
                int r = sq / 8 + 1;
                if (c < 0 || c > 7 || r > 7) continue;
                int to2 = r * 8 + c;
                int target = p->mailbox[to2];
                if (target >= 0 && COLOR_OF(target) != color) {
                    if (to2 / 8 == 7) for (int pr = 1; pr <= 4; pr++) add_move(ml, sq, to2, pr, target, 0);
                    else add_move(ml, sq, to2, 0, target, 0);
                }
                if (p->ep == to2 && target < 0) {
                    int capSq = to2 - 8;
                    if (capSq >= 0 && p->mailbox[capSq] >= 0 && COLOR_OF(p->mailbox[capSq]) != color) {
                        add_move(ml, sq, to2, 0, p->mailbox[capSq], 2);
                    }
                }
            }
        } else {
            int to = sq - 8;
            if (to >= 0 && p->mailbox[to] < 0) {
                if (to / 8 == 0) {
                    if (!only_captures) for (int pr = 1; pr <= 4; pr++) add_move(ml, sq, to, pr, 255, 0);
                } else {
                    if (!only_captures) add_move(ml, sq, to, 0, 255, 0);
                    if (sq / 8 == 6) {
                        int to2 = sq - 16;
                        if (p->mailbox[to2] < 0) {
                            if (!only_captures) add_move(ml, sq, to2, 0, 255, 0);
                        }
                    }
                }
            }
            for (int d = -1; d <= 1; d += 2) {
                int c = sq % 8 + d;
                int r = sq / 8 - 1;
                if (c < 0 || c > 7 || r < 0) continue;
                int to2 = r * 8 + c;
                int target = p->mailbox[to2];
                if (target >= 0 && COLOR_OF(target) != color) {
                    if (to2 / 8 == 0) for (int pr = 1; pr <= 4; pr++) add_move(ml, sq, to2, pr, target, 0);
                    else add_move(ml, sq, to2, 0, target, 0);
                }
                if (p->ep == to2 && target < 0) {
                    int capSq = to2 + 8;
                    if (capSq < 64 && p->mailbox[capSq] >= 0 && COLOR_OF(p->mailbox[capSq]) != color) {
                        add_move(ml, sq, to2, 0, p->mailbox[capSq], 2);
                    }
                }
            }
        }
    }

    /* knights */
    int nd[8][2] = {{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
    for (int sq = 0; sq < 64; sq++) {
        int pc = p->mailbox[sq];
        if (pc < 0 || COLOR_OF(pc) != color || TYPE_OF(pc) != KNIGHT) continue;
        int r = sq / 8, c = sq % 8;
        for (int i = 0; i < 8; i++) {
            int nr = r + nd[i][0], nc = c + nd[i][1];
            if (nr < 0 || nr > 7 || nc < 0 || nc > 7) continue;
            int to = nr*8+nc;
            int target = p->mailbox[to];
            if (target >= 0 && COLOR_OF(target) == color) continue;
            if (only_captures && !(target >= 0)) continue;
            add_move(ml, sq, to, 0, (target>=0?target:255), 0);
        }
    }
    /* bishops */
    {
        int d[4][2] = {{-1,-1},{-1,1},{1,-1},{1,1}};
        for (int sq = 0; sq < 64; sq++) {
            int pc = p->mailbox[sq];
            if (pc < 0 || COLOR_OF(pc) != color) continue;
            if (TYPE_OF(pc) != BISHOP && TYPE_OF(pc) != QUEEN) continue;
            int r = sq/8, c = sq%8;
            for (int di = 0; di < 4; di++) {
                int rr = r + d[di][0], cc = c + d[di][1];
                while (rr >= 0 && rr < 8 && cc >= 0 && cc < 8) {
                    int to = rr*8+cc;
                    int target = p->mailbox[to];
                    if (target >= 0 && COLOR_OF(target) == color) break;
                    if (!only_captures || target >= 0) add_move(ml, sq, to, 0, (target>=0?target:255), 0);
                    if (target >= 0) break;
                    rr += d[di][0]; cc += d[di][1];
                }
            }
        }
    }
    /* rooks */
    {
        int d[4][2] = {{-1,0},{1,0},{0,-1},{0,1}};
        for (int sq = 0; sq < 64; sq++) {
            int pc = p->mailbox[sq];
            if (pc < 0 || COLOR_OF(pc) != color) continue;
            if (TYPE_OF(pc) != ROOK && TYPE_OF(pc) != QUEEN) continue;
            int r = sq/8, c = sq%8;
            for (int di = 0; di < 4; di++) {
                int rr = r + d[di][0], cc = c + d[di][1];
                while (rr >= 0 && rr < 8 && cc >= 0 && cc < 8) {
                    int to = rr*8+cc;
                    int target = p->mailbox[to];
                    if (target >= 0 && COLOR_OF(target) == color) break;
                    if (!only_captures || target >= 0) add_move(ml, sq, to, 0, (target>=0?target:255), 0);
                    if (target >= 0) break;
                    rr += d[di][0]; cc += d[di][1];
                }
            }
        }
    }
    /* king */
    int kd[8][2] = {{-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,-1},{1,0},{1,1}};
    for (int sq = 0; sq < 64; sq++) {
        int pc = p->mailbox[sq];
        if (pc < 0 || COLOR_OF(pc) != color || TYPE_OF(pc) != KING) continue;
        int r = sq/8, c = sq%8;
        for (int i = 0; i < 8; i++) {
            int nr = r + kd[i][0], nc = c + kd[i][1];
            if (nr < 0 || nr > 7 || nc < 0 || nc > 7) continue;
            int to = nr*8+nc;
            int target = p->mailbox[to];
            if (target >= 0 && COLOR_OF(target) == color) continue;
            if (only_captures && !(target >= 0)) continue;
            add_move(ml, sq, to, 0, (target>=0?target:255), 0);
        }
        if (color == WHITE && sq == 4 && !only_captures) {
            if ((p->castle & 1) && p->mailbox[5] < 0 && p->mailbox[6] < 0) {
                if (!square_attacked(p, 4, BLACK) && !square_attacked(p, 5, BLACK) && !square_attacked(p, 6, BLACK)) {
                    add_move(ml, 4, 6, 0, 255, 1);
                }
            }
            if ((p->castle & 2) && p->mailbox[3] < 0 && p->mailbox[2] < 0 && p->mailbox[1] < 0) {
                if (!square_attacked(p, 4, BLACK) && !square_attacked(p, 3, BLACK) && !square_attacked(p, 2, BLACK)) {
                    add_move(ml, 4, 2, 0, 255, 1);
                }
            }
        }
        if (color == BLACK && sq == 60 && !only_captures) {
            if ((p->castle & 4) && p->mailbox[61] < 0 && p->mailbox[62] < 0) {
                if (!square_attacked(p, 60, WHITE) && !square_attacked(p, 61, WHITE) && !square_attacked(p, 62, WHITE)) {
                    add_move(ml, 60, 62, 0, 255, 1);
                }
            }
            if ((p->castle & 8) && p->mailbox[59] < 0 && p->mailbox[58] < 0 && p->mailbox[57] < 0) {
                if (!square_attacked(p, 60, WHITE) && !square_attacked(p, 59, WHITE) && !square_attacked(p, 58, WHITE)) {
                    add_move(ml, 60, 58, 0, 255, 1);
                }
            }
        }
    }
}

bool move_is_legal(Position* p, Move m) {
    Undo u;
    int color = p->side;
    make_move(p, m, &u);
    int illegal = in_check(p, color);
    undo_move(p, &u);
    return !illegal;
}

int is_checkmate(const Position* pp) {
    Position p = *pp;
    MoveList ml;
    gen_moves(&p, &ml, false);
    for (int i = 0; i < ml.count; i++) {
        if (move_is_legal(&p, ml.moves[i])) return 0;
    }
    return in_check(pp, pp->side);
}

int is_stalemate(const Position* pp) {
    Position p = *pp;
    MoveList ml;
    gen_moves(&p, &ml, false);
    for (int i = 0; i < ml.count; i++) {
        if (move_is_legal(&p, ml.moves[i])) return 0;
    }
    return !in_check(pp, pp->side);
}

int insufficient_material(const Position* p) {
    int pieces = 0;
    for (int i = 0; i < 12; i++) {
        if (i == W_KING || i == B_KING) continue;
        pieces += __builtin_popcountll(p->bb[i]);
    }
    if (pieces == 0) return 1;
    if (pieces == 1) {
        for (int i = 0; i < 12; i++) {
            if (i == W_KING || i == B_KING) continue;
            if (TYPE_OF(i) == BISHOP || TYPE_OF(i) == KNIGHT) return 1;
        }
    }
    return 0;
}

int square_ok(int sq) { return sq >= 0 && sq < 64; }

const char* move_to_uci(Move m, char* buf) {
    buf[0] = 'a' + (m.from % 8);
    buf[1] = '1' + (m.from / 8);
    buf[2] = 'a' + (m.to % 8);
    buf[3] = '1' + (m.to / 8);
    if (m.promo) {
        char pc = (m.promo == 1) ? 'n' : (m.promo == 2) ? 'b' : (m.promo == 3) ? 'r' : 'q';
        buf[4] = pc; buf[5] = 0;
    } else {
        buf[4] = 0;
    }
    return buf;
}

uint64_t perft(Position* p, int depth) {
    if (depth == 0) return 1;
    MoveList ml;
    gen_moves(p, &ml, false);
    Move legal[MAX_MOVES]; int nl = 0;
    for (int i = 0; i < ml.count; i++) {
        if (move_is_legal(p, ml.moves[i])) legal[nl++] = ml.moves[i];
    }
    if (depth == 1) return nl;
    uint64_t nodes = 0;
    for (int i = 0; i < nl; i++) {
        Undo u;
        make_move(p, legal[i], &u);
        nodes += perft(p, depth - 1);
        undo_move(p, &u);
    }
    return nodes;
}
