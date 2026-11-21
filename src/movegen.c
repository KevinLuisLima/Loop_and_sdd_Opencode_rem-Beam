#include "movegen.h"
#include <string.h>

#define MAXM 512

static int add_move(int *moves, int *n, int max, int from, int to, int promo) {
    if (*n >= max) return -1;
    moves[*n] = from | (to << 6) | (promo << 12);
    (*n)++;
    return 0;
}

static void gen_pawn(const Position *p, int *moves, int *n, int max) {
    int color = p->side;
    int opp = color ^ 1;
    uint64_t pawns = p->bb[color][PT_PAWN];
    int dir = (color == COLOR_WHITE) ? 8 : -8;
    int startRank = (color == COLOR_WHITE) ? 1 : 6;
    int promoRank = (color == COLOR_WHITE) ? 7 : 0;
    uint64_t occAll = pos_all_occupancy(p);
    uint64_t bb = pawns;
    while (bb) {
        int from = __builtin_ctzll(bb);
        bb &= bb - 1;
        int to = from + dir;
        if (to < 0 || to >= 64) continue;
        if (!(occAll & (1ULL << to))) {
            int tr = to / 8;
            if (tr == promoRank) {
                add_move(moves, n, max, from, to, PT_KNIGHT);
                add_move(moves, n, max, from, to, PT_BISHOP);
                add_move(moves, n, max, from, to, PT_ROOK);
                add_move(moves, n, max, from, to, PT_QUEEN);
            } else {
                add_move(moves, n, max, from, to, 0);
                int fr = from / 8;
                if (fr == startRank) {
                    int to2 = to + dir;
                    if (to2 >= 0 && to2 < 64 && !(occAll & (1ULL << to2))) {
                        add_move(moves, n, max, from, to2, 0);
                    }
                }
            }
        }
    }

    bb = pawns;
    while (bb) {
        int from = __builtin_ctzll(bb);
        bb &= bb - 1;
        int r = from / 8, c = from % 8;
        for (int dc = -1; dc <= 1; dc += 2) {
            int nc = c + dc;
            if (nc < 0 || nc > 7) continue;
            int nr = r + (color == COLOR_WHITE ? 1 : -1);
            if (nr < 0 || nr > 7) continue;
            int to = nr * 8 + nc;
            uint64_t bit = 1ULL << to;
            bool isCap = false;
            for (int pt = 0; pt < 6; pt++) if (p->bb[opp][pt] & bit) { isCap = true; break; }
            if (isCap) {
                int tr = to / 8;
                if (tr == promoRank) {
                    add_move(moves, n, max, from, to, PT_KNIGHT);
                    add_move(moves, n, max, from, to, PT_BISHOP);
                    add_move(moves, n, max, from, to, PT_ROOK);
                    add_move(moves, n, max, from, to, PT_QUEEN);
                } else {
                    add_move(moves, n, max, from, to, 0);
                }
            }
            if (p->ep_square == to) {
                add_move(moves, n, max, from, to, 0);
            }
        }
    }
}

static const int knightOff[8][2] = {{2,1},{2,-1},{-2,1},{-2,-1},{1,2},{1,-2},{-1,2},{-1,-2}};
static void gen_knight(const Position *p, int *moves, int *n, int max) {
    int color = p->side;
    uint64_t bb = p->bb[color][PT_KNIGHT];
    uint64_t occOwn = pos_occupancy(p, color);
    while (bb) {
        int from = __builtin_ctzll(bb);
        bb &= bb - 1;
        int r = from / 8, c = from % 8;
        for (int i = 0; i < 8; i++) {
            int rr = r + knightOff[i][0];
            int cc = c + knightOff[i][1];
            if (rr < 0 || rr > 7 || cc < 0 || cc > 7) continue;
            int to = rr * 8 + cc;
            if (occOwn & (1ULL << to)) continue;
            add_move(moves, n, max, from, to, 0);
        }
    }
}

static void gen_king(const Position *p, int *moves, int *n, int max) {
    int color = p->side;
    uint64_t bb = p->bb[color][PT_KING];
    if (!bb) return;
    int from = __builtin_ctzll(bb);
    uint64_t occOwn = pos_occupancy(p, color);
    int r = from / 8, c = from % 8;
    static const int kOff[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
    for (int i = 0; i < 8; i++) {
        int rr = r + kOff[i][0];
        int cc = c + kOff[i][1];
        if (rr < 0 || rr > 7 || cc < 0 || cc > 7) continue;
        int to = rr * 8 + cc;
        if (occOwn & (1ULL << to)) continue;
        add_move(moves, n, max, from, to, 0);
    }
    int castle = p->castle;
    uint64_t all = pos_all_occupancy(p);
    if (color == COLOR_WHITE) {
        if (castle & 1) {
            if (!((all) & ((1ULL<<5)|(1ULL<<6)))) add_move(moves, n, max, 4, 6, 0);
        }
        if (castle & 2) {
            if (!((all) & ((1ULL<<1)|(1ULL<<2)|(1ULL<<3)))) add_move(moves, n, max, 4, 2, 0);
        }
    } else {
        if (castle & 4) {
            if (!((all) & ((1ULL<<61)|(1ULL<<62)))) add_move(moves, n, max, 60, 62, 0);
        }
        if (castle & 8) {
            if (!((all) & ((1ULL<<57)|(1ULL<<58)|(1ULL<<59)))) add_move(moves, n, max, 60, 58, 0);
        }
    }
}

static void gen_sliding(const Position *p, int *moves, int *n, int max) {
    int color = p->side;
    uint64_t occAll = pos_all_occupancy(p);
    uint64_t occOwn = pos_occupancy(p, color);
    uint64_t bb;

    /* rooks */
    bb = p->bb[color][PT_ROOK];
    static const int rDirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    while (bb) {
        int from = __builtin_ctzll(bb); bb &= bb - 1;
        int r = from / 8, c = from % 8;
        for (int d = 0; d < 4; d++) {
            int rr = r, cc = c;
            for (;;) {
                rr += rDirs[d][0]; cc += rDirs[d][1];
                if (rr < 0 || rr > 7 || cc < 0 || cc > 7) break;
                int to = rr * 8 + cc;
                uint64_t bit = 1ULL << to;
                if (occOwn & bit) break;
                add_move(moves, n, max, from, to, 0);
                if (occAll & bit) break;
            }
        }
    }
    /* bishops */
    bb = p->bb[color][PT_BISHOP];
    static const int bDirs[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
    while (bb) {
        int from = __builtin_ctzll(bb); bb &= bb - 1;
        int r = from / 8, c = from % 8;
        for (int d = 0; d < 4; d++) {
            int rr = r, cc = c;
            for (;;) {
                rr += bDirs[d][0]; cc += bDirs[d][1];
                if (rr < 0 || rr > 7 || cc < 0 || cc > 7) break;
                int to = rr * 8 + cc;
                uint64_t bit = 1ULL << to;
                if (occOwn & bit) break;
                add_move(moves, n, max, from, to, 0);
                if (occAll & bit) break;
            }
        }
    }
    /* queens */
    bb = p->bb[color][PT_QUEEN];
    static const int qDirs[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
    while (bb) {
        int from = __builtin_ctzll(bb); bb &= bb - 1;
        int r = from / 8, c = from % 8;
        for (int d = 0; d < 8; d++) {
            int rr = r, cc = c;
            for (;;) {
                rr += qDirs[d][0]; cc += qDirs[d][1];
                if (rr < 0 || rr > 7 || cc < 0 || cc > 7) break;
                int to = rr * 8 + cc;
                uint64_t bit = 1ULL << to;
                if (occOwn & bit) break;
                add_move(moves, n, max, from, to, 0);
                if (occAll & bit) break;
            }
        }
    }
}

int movegen_pseudo(const Position *p, int *moves, int max) {
    int n = 0;
    gen_pawn(p, moves, &n, max);
    gen_knight(p, moves, &n, max);
    gen_sliding(p, moves, &n, max);
    gen_king(p, moves, &n, max);
    return n;
}

int movegen_legal_moves(Position *p, int *moves, int max) {
    static int pseudo[MAXM];
    int pn = movegen_pseudo(p, pseudo, MAXM);
    int n = 0;
    for (int i = 0; i < pn; i++) {
        int m = pseudo[i];
        int from = m & 63;
        int to = (m >> 6) & 63;
        int promo = (m >> 12) & 7;
        if (!pos_square_ok(from) || !pos_square_ok(to)) continue;

        /* castling through check validation */
        int mover = p->side;
        if (pos_piece_at(p,from,NULL) == PT_KING && (from-to == 2 || to-from == 2)) {
            int opp = mover ^ 1;
            int step = (to > from) ? 1 : -1;
            int mid = from + step;
            if (pos_in_check(p, mover)) continue;
            /* check mid and destination not attacked */
            Position tmp1 = *p;
            /* fake king on mid */
            tmp1.bb[mover][PT_KING] &= ~(1ULL<<from);
            tmp1.bb[mover][PT_KING] |= (1ULL<<mid);
            if (pos_in_check(&tmp1, mover)) continue;
            Position tmp2 = *p;
            tmp2.bb[mover][PT_KING] &= ~(1ULL<<from);
            tmp2.bb[mover][PT_KING] |= (1ULL<<to);
            if (pos_in_check(&tmp2, mover)) continue;
            (void)opp;
        }
        Undo u;
        pos_make_move(p, from, to, promo, &u);
        mover = p->side ^ 1;
        bool bad = pos_in_check(p, mover);
        pos_unmake_move(p, &u);
        if (!bad) {
            if (n < max) moves[n++] = m;
        }
    }
    return n;
}

int movegen_count_legal(Position *p) {
    int moves[512];
    return movegen_legal_moves(p, moves, 512);
}
