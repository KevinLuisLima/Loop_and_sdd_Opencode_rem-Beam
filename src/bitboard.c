#include "bitboard.h"
#include "types.h"
#include <assert.h>

static const int rook_dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
static const int bishop_dirs[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};

uint64_t bb_set(int sq) { return 1ULL << sq; }

int bb_popcount(uint64_t x) { return __builtin_popcountll(x); }

int bb_lsb(uint64_t x) { return __builtin_ctzll(x); }

uint64_t bb_knight(int sq) {
    static const int offsets[8] = {17,15,10,6,-6,-10,-15,-17};
    uint64_t mask = 0;
    int r = sq / 8, c = sq % 8;
    for (int i=0;i<8;i++) {
        int o = offsets[i];
        int nr = r + (o > 0 ? (o+7)/8 : o/8); // not exact, use deltas below
        (void)nr;
    }
    /* simpler direct */
    mask = 0;
    static const int d[8][2] = {{2,1},{2,-1},{-2,1},{-2,-1},{1,2},{1,-2},{-1,2},{-1,-2}};
    for (int i=0;i<8;i++) {
        int nr = r + d[i][0];
        int nc = c + d[i][1];
        if (nr>=0 && nr<8 && nc>=0 && nc<8) mask |= 1ULL << (nr*8+nc);
    }
    return mask;
}

uint64_t bb_king(int sq) {
    int r = sq/8, c = sq%8;
    uint64_t mask = 0;
    for (int dr=-1; dr<=1; dr++)
        for (int dc=-1; dc<=1; dc++) {
            if (dr==0 && dc==0) continue;
            int nr=r+dr, nc=c+dc;
            if (nr>=0 && nr<8 && nc>=0 && nc<8) mask |= 1ULL << (nr*8+nc);
        }
    return mask;
}

uint64_t bb_pawn_attacks(int side, int sq) {
    int r = sq/8, c = sq%8;
    uint64_t mask = 0;
    if (side == WT) {
        if (r < 7) {
            if (c > 0) mask |= 1ULL << ((r+1)*8 + (c-1));
            if (c < 7) mask |= 1ULL << ((r+1)*8 + (c+1));
        }
    } else {
        if (r > 0) {
            if (c > 0) mask |= 1ULL << ((r-1)*8 + (c-1));
            if (c < 7) mask |= 1ULL << ((r-1)*8 + (c+1));
        }
    }
    return mask;
}

static uint64_t sliding(int sq, uint64_t occ, int dirs[][2], int ndir) {
    int r = sq/8, c = sq%8;
    uint64_t mask = 0;
    for (int d=0; d<ndir; d++) {
        int dr = dirs[d][0], dc = dirs[d][1];
        int nr = r + dr, nc = c + dc;
        while (nr>=0 && nr<8 && nc>=0 && nc<8) {
            int s = nr*8+nc;
            mask |= 1ULL << s;
            if (occ & (1ULL<<s)) break;
            nr += dr; nc += dc;
        }
    }
    return mask;
}

uint64_t bb_rook(int sq, uint64_t occ) {
    return sliding(sq, occ, (int(*)[2])rook_dirs, 4);
}
uint64_t bb_bishop(int sq, uint64_t occ) {
    return sliding(sq, occ, (int(*)[2])bishop_dirs, 4);
}
uint64_t bb_queen(int sq, uint64_t occ) {
    return bb_rook(sq, occ) | bb_bishop(sq, occ);
}

void bb_init(void) { }