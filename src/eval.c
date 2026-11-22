#include "eval.h"
#include <math.h>
extern uint64_t KNIGHT_ATK[64];

static int pst_index(int pt, int sq) { return W_PST + pt * 64 + sq; }

int eval_position(const Board *b, const Weights *w) {
    float score = 0.0f;
    /* material + PST from white perspective */
    for (int pt = 0; pt < 6; pt++) {
        if (pt == KING) continue; /* king not material */
        float mv = w->vals[W_MATERIAL + pt]; /* P..Q */
        /* white */
        uint64_t wb = b->pieces[WHITE][pt];
        while (wb) {
            int sq = __builtin_ctzll(wb); wb &= wb - 1;
            score += mv + w->vals[pst_index(pt, sq)];
        }
        /* black mirrored */
        uint64_t bb = b->pieces[BLACK][pt];
        while (bb) {
            int sq = __builtin_ctzll(bb); bb &= bb - 1;
            score -= mv + w->vals[pst_index(pt, sq ^ 56)];
        }
    }
    /* mobility cheap: count attacks? Use simple piece count mobility baseline */
    /* doubled pawns penalty */
    for (int color = 0; color < 2; color++) {
        uint64_t pawns = b->pieces[color][PAWN];
        int sign = (color == WHITE) ? 1 : -1;
        /* doubled */
        int files[8] = {0};
        uint64_t tmp = pawns;
        while (tmp) { int sq = __builtin_ctzll(tmp); tmp &= tmp - 1; files[sq % 8]++; }
        for (int f = 0; f < 8; f++) {
            if (files[f] > 1) score += sign * w->vals[W_DOUBLED] * (files[f] - 1);
        }
        /* isolated */
        tmp = pawns;
        while (tmp) {
            int sq = __builtin_ctzll(tmp); tmp &= tmp - 1;
            int f = sq % 8;
            int adj = 0;
            if (f > 0) {
                /* any pawn in adjacent files */
                uint64_t adjmask = pawns & (0x0101010101010101ULL << (f - 1));
                if (adjmask) adj = 1;
            }
            if (f < 7) {
                uint64_t adjmask = pawns & (0x0101010101010101ULL << (f + 1));
                if (adjmask) adj = 1;
            }
            if (!adj) score += sign * w->vals[W_ISOLATED];
        }
    }
    /* mobility: count legal-ish moves via sliders count bits (popcount attacks of pieces excluding pawns) */
    /* Compute attacked squares by white and black (simplified) */
    int mobW = 0, mobB = 0;
    /* knights */
    uint64_t tmp = b->pieces[WHITE][KNIGHT];
    while (tmp) { int sq = __builtin_ctzll(tmp); tmp &= tmp - 1; mobW += __builtin_popcountll(KNIGHT_ATK[sq] & ~b->occ[0]); }
    tmp = b->pieces[BLACK][KNIGHT];
    while (tmp) { int sq = __builtin_ctzll(tmp); tmp &= tmp - 1; mobB += __builtin_popcountll(KNIGHT_ATK[sq] & ~b->occ[1]); }
    /* king safety (very simple): piece count near enemy king? skip; add king PST already */
    score += (mobW - mobB) * (w->vals[W_MOBILITY] / 100.0f);
    /* king pst included */
    /* if side to move is black, negate */
    if (b->side == BLACK) score = -score;
    /* round */
    return (int)lroundf(score);
}
