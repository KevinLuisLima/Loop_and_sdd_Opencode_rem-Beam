#include "eval.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int load_weights(const char* path, double* w, int n) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "weights file open failed: %s\n", path); return -1; }
    /* header: magic uint32, version uint32, num_weights uint32, dtype uint8 */
    unsigned char hdr[13];
    if (fread(hdr, 1, 13, f) != 13) { fprintf(stderr, "weights header truncated\n"); fclose(f); return -1; }
    /* magic little endian */
    unsigned int magic = (unsigned int)hdr[0] | ((unsigned int)hdr[1]<<8) | ((unsigned int)hdr[2]<<16) | ((unsigned int)hdr[3]<<24);
    if (magic != 0x43485731) { fprintf(stderr, "bad magic\n"); fclose(f); return -1; }
    unsigned int version = (unsigned int)hdr[4] | ((unsigned int)hdr[5]<<8) | ((unsigned int)hdr[6]<<16) | ((unsigned int)hdr[7]<<24);
    if (version != 1) { fprintf(stderr, "bad version %u\n", version); fclose(f); return -1; }
    unsigned int num = (unsigned int)hdr[8] | ((unsigned int)hdr[9]<<8) | ((unsigned int)hdr[10]<<16) | ((unsigned int)hdr[11]<<24);
    if ((int)num != n) { fprintf(stderr, "num weights %u != %d\n", num, n); fclose(f); return -1; }
    unsigned char dtype = hdr[12];
    if (dtype != 0) { fprintf(stderr, "unsupported dtype %u\n", dtype); fclose(f); return -1; }
    /* read floats */
    for (int i = 0; i < n; i++) {
        unsigned char b[4];
        if (fread(b, 1, 4, f) != 4) { fprintf(stderr, "weights truncated\n"); fclose(f); return -1; }
        unsigned int u = (unsigned int)b[0] | ((unsigned int)b[1]<<8) | ((unsigned int)b[2]<<16) | ((unsigned int)b[3]<<24);
        /* float32 to double */
        /* interpret as float */
        float* fp = (float*)&u;
        w[i] = *fp;
        if (!isfinite(w[i])) { fprintf(stderr, "NaN/Inf weight at %d\n", i); fclose(f); return -1; }
    }
    /* ensure no trailing junk? allow */
    fclose(f);
    return 0;
}

int evaluate(const Position* p, const double* w) {
    int score = 0; /* white perspective centipawns, but w in cp */
    /* material counts */
    int countsW[5] = {0}, countsB[5] = {0};
    for (int t = 0; t < 5; t++) {
        countsW[t] = __builtin_popcountll(p->bb[W_PAWN + t]);
        countsB[t] = __builtin_popcountll(p->bb[B_PAWN + t]);
    }
    for (int t = 0; t < 5; t++) {
        int diff = countsW[t] - countsB[t];
        score += (int)((double)diff * w[t]);
    }
    /* PST */
    for (int sq = 0; sq < 64; sq++) {
        int pc = p->mailbox[sq];
        if (pc < 0) continue;
        int color = COLOR_OF(pc);
        int type = TYPE_OF(pc);
        if (type == KING) continue;
        int idx = W_PST + type * 64 + (color == WHITE ? sq : (sq ^ 56));
        int val = (int)w[idx];
        if (color == WHITE) score += val; else score -= val;
    }
    /* mobility: attack counts popcount (rough), include pieces */
    Bitboard wAtt = 0, bAtt = 0;
    Bitboard occ = occ_all(p);
    /* knights etc */
    // white
    for (int i = 0; i < 12; i++) {
        Bitboard bb = p->bb[i];
        int color = COLOR_OF(i);
        int type = TYPE_OF(i);
        while (bb) {
            int s = __builtin_ctzll(bb); bb &= bb - 1;
            Bitboard a = 0;
            if (type == KNIGHT) a = knight_attacks(s);
            else if (type == BISHOP) a = bishop_attacks(s, occ);
            else if (type == ROOK) a = rook_attacks(s, occ);
            else if (type == QUEEN) a = queen_attacks(s, occ);
            else if (type == KING) a = king_attacks(s);
            else if (type == PAWN) a = pawn_attacks(color, s);
            if (color == WHITE) wAtt |= a; else bAtt |= a;
        }
    }
    int mobDiff = __builtin_popcountll(wAtt) - __builtin_popcountll(bAtt);
    score += (int)((double)mobDiff * w[W_MOB] / 10.0); /* scale down */

    /* doubled pawns */
    for (int color = 0; color < 2; color++) {
        int pen = 0;
        for (int f = 0; f < 8; f++) {
            int cnt = 0;
            for (int r = 0; r < 8; r++) {
                int sq = r*8+f;
                int pc = p->mailbox[sq];
                if (pc >= 0 && COLOR_OF(pc) == color && TYPE_OF(pc) == PAWN) cnt++;
            }
            if (cnt > 1) pen += (cnt - 1);
        }
        if (color == WHITE) score -= (int)((double)pen * w[W_DOUBLED]);
        else score += (int)((double)pen * w[W_DOUBLED]);
    }
    /* isolated pawns */
    for (int color = 0; color < 2; color++) {
        int cntIso = 0;
        for (int sq = 0; sq < 64; sq++) {
            int pc = p->mailbox[sq];
            if (pc < 0 || COLOR_OF(pc) != color || TYPE_OF(pc) != PAWN) continue;
            int f = sq % 8;
            int hasAdj = 0;
            for (int ff = f - 1; ff <= f + 1; ff += 2) {
                if (ff < 0 || ff > 7) continue;
                for (int r = 0; r < 8; r++) {
                    int s2 = r*8+ff;
                    int pc2 = p->mailbox[s2];
                    if (pc2 >= 0 && COLOR_OF(pc2) == color && TYPE_OF(pc2) == PAWN) { hasAdj = 1; break; }
                }
                if (hasAdj) break;
            }
            if (!hasAdj) cntIso++;
        }
        if (color == WHITE) score -= (int)((double)cntIso * w[W_ISOLATED]);
        else score += (int)((double)cntIso * w[W_ISOLATED]);
    }
    /* passed pawns */
    for (int sq = 0; sq < 64; sq++) {
        int pc = p->mailbox[sq];
        if (pc < 0 || TYPE_OF(pc) != PAWN) continue;
        int color = COLOR_OF(pc);
        int f = sq % 8;
        int r = sq / 8;
        /* enemy pawns ahead on f-1,f,f+1 */
        int blocked = 0;
        for (int ff = f - 1; ff <= f + 1; ff++) {
            if (ff < 0 || ff > 7) continue;
            int rstart = (color == WHITE) ? r + 1 : r - 1;
            int rend = (color == WHITE) ? 8 : -1;
            int step = (color == WHITE) ? 1 : -1;
            for (int rr = rstart; rr != rend; rr += step) {
                int s2 = rr*8+ff;
                int pc2 = p->mailbox[s2];
                if (pc2 >= 0 && COLOR_OF(pc2) != color && TYPE_OF(pc2) == PAWN) { blocked = 1; break; }
            }
            if (blocked) break;
        }
        if (!blocked) {
            int bonus = (int)w[W_PASSED];
            /* advance bonus: deeper = more */
            int advancement = (color == WHITE) ? r : 7 - r;
            bonus += advancement * (int)(w[W_PASSED] / 4.0);
            if (color == WHITE) score += bonus; else score -= bonus;
        }
    }
    /* bishop pair */
    for (int color = 0; color < 2; color++) {
        int bishops = 0;
        for (int t = 0; t < 64; t++) {
            int pc = p->mailbox[t];
            if (pc >= 0 && COLOR_OF(pc) == color && TYPE_OF(pc) == BISHOP) bishops++;
        }
        if (bishops >= 2) {
            if (color == WHITE) score += (int)w[W_BISHOP_PAIR];
            else score -= (int)w[W_BISHOP_PAIR];
        }
    }
    /* king safety: count enemy attackers in adjacent squares around king */
    for (int color = 0; color < 2; color++) {
        Bitboard kbb = p->bb[color*6+KING];
        if (!kbb) continue;
        int ksq = __builtin_ctzll(kbb);
        /* adjacent squares */
        Bitboard ring = king_attacks(ksq);
        ring |= 1ULL << ksq;
        /* enemy attacks on ring */
        /* compute by iterating enemy pieces */
        // simplify: count enemy pieces attacking any ring square
        int attackers = 0;
        for (int sq2 = 0; sq2 < 64; sq2++) {
            int pc = p->mailbox[sq2];
            if (pc < 0 || COLOR_OF(pc) == color) continue;
            /* does this piece attack a ring square? */
            Bitboard a = 0;
            int type = TYPE_OF(pc);
            if (type == KNIGHT) a = knight_attacks(sq2);
            else if (type == BISHOP) a = bishop_attacks(sq2, occ);
            else if (type == ROOK) a = rook_attacks(sq2, occ);
            else if (type == QUEEN) a = queen_attacks(sq2, occ);
            else if (type == PAWN) a = pawn_attacks(COLOR_OF(pc), sq2);
            else if (type == KING) a = king_attacks(sq2);
            if (a & ring) attackers++;
        }
        if (color == WHITE) score -= attackers * (int)(w[W_KING_SAFETY] / 4.0);
        else score += attackers * (int)(w[W_KING_SAFETY] / 4.0);
    }

    if (p->side == BLACK) score = -score;
    return score;
}
