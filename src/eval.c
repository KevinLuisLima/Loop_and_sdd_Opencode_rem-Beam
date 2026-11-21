#include "eval.h"
#include "movegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MATERIAL_BASE_PAWN 100.0f
#define MATERIAL_BASE_KNIGHT 320.0f
#define MATERIAL_BASE_BISHOP 330.0f
#define MATERIAL_BASE_ROOK 500.0f
#define MATERIAL_BASE_QUEEN 900.0f

/* weight indices */
#define W_PAWN 0
#define W_KNIGHT 1
#define W_BISHOP 2
#define W_ROOK 3
#define W_QUEEN 4
#define W_PST(p) (5 + (p) * 64)
#define W_MOB (5 + 6*64)
#define W_DOUBLED (W_MOB + 1)
#define W_ISOLATED (W_MOB + 2)
#define W_KINGSAFE (W_MOB + 3)

static const char *MAGIC = "CHE";

void weights_defaults(WeightSet *ws) {
    if (!ws) return;
    memset(ws, 0, sizeof(*ws));
    ws->version = 1;
    ws->num_weights = EVAL_NUM_WEIGHTS;
    ws->w[W_PAWN] = MATERIAL_BASE_PAWN;
    ws->w[W_KNIGHT] = MATERIAL_BASE_KNIGHT;
    ws->w[W_BISHOP] = MATERIAL_BASE_BISHOP;
    ws->w[W_ROOK] = MATERIAL_BASE_ROOK;
    ws->w[W_QUEEN] = MATERIAL_BASE_QUEEN;
    for (int p = 0; p < 6; p++) {
        for (int sq = 0; sq < 64; sq++) {
            ws->w[W_PST(p)+sq] = 0.0f; /* flat */
        }
    }
    /* king central penalty? default zeros */
    ws->w[W_MOB] = 0.1f;
    ws->w[W_DOUBLED] = -20.0f;
    ws->w[W_ISOLATED] = -15.0f;
    ws->w[W_KINGSAFE] = -3.0f;
    /* simple pst tables */
    /* pawns: favor center/advanced */
    for (int sq = 0; sq < 64; sq++) {
        int r = sq/8, f = sq%8;
        float v = (r==3||r==4? 5.0f:0.0f) + (f>=2 && f<=5 ? 3.0f : 0.0f);
        ws->w[W_PST(0)+sq] = v;
    }
    /* knights: center */
    for (int sq = 0; sq < 64; sq++) {
        int r=sq/8,c=sq%8;
        float center = 4.0f - (fabsf(r-3.5f)+fabsf(c-3.5f));
        ws->w[W_PST(1)+sq] = center*5.0f;
    }
    /* bishops similar */
    for (int sq = 0; sq < 64; sq++) {
        int r=sq/8,c=sq%8;
        float center = 4.0f - (fabsf(r-3.5f)+fabsf(c-3.5f));
        ws->w[W_PST(2)+sq] = center*3.0f;
    }
    /* rooks on open files (approx) */
    /* queens */
    /* king: back rank/others negative */
    for (int sq = 0; sq < 64; sq++) {
        int r=sq/8;
        if (r <= 1) ws->w[W_PST(5)+sq] = 20.0f; /* keep king back */
        else if (r == 2) ws->w[W_PST(5)+sq] = 10.0f;
        else if (r >= 5) ws->w[W_PST(5)+sq] = -10.0f;
    }
    ws->loaded = 1;
}

int weights_load(WeightSet *ws, const char *path) {
    if (!ws || !path) return -1;
    memset(ws, 0, sizeof(*ws));
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    char magic[4];
    if (fread(magic, 1, 4, f) != 4) { fclose(f); return -1; }
    if (memcmp(magic, MAGIC, 3) != 0) { fclose(f); return -1; }
    /* read version */
    unsigned char hdr[8];
    if (fread(hdr, 1, 8, f) != 8) { fclose(f); return -1; }
    uint32_t version = hdr[0] | (hdr[1]<<8) | (hdr[2]<<16) | (hdr[3]<<24);
    uint32_t num = hdr[4] | (hdr[5]<<8) | (hdr[6]<<16) | (hdr[7]<<24);
    if (version == 0) { fclose(f); return -1; }
    if (num != EVAL_NUM_WEIGHTS) { fclose(f); return -1; }
    size_t got = fread(ws->w, sizeof(float), num, f);
    /* check no extra/truncated? require exactly num */
    if (got != num) { fclose(f); return -1; }
    /* check NaN/Inf */
    for (unsigned i = 0; i < num; i++) {
        if (!isfinite(ws->w[i])) { fclose(f); return -1; }
    }
    ws->version = (int)version;
    ws->num_weights = (int)num;
    ws->loaded = 1;
    fclose(f);
    return 0;
}

/* helper counts */
static int popcount64(unsigned long long x) { return __builtin_popcountll(x); }

static int count_doubled(const Position *p, int color) {
    int doubled = 0;
    for (int f = 0; f < 8; f++) {
        int cnt = 0;
        for (int r = 0; r < 8; r++) {
            int sq = r*8+f;
            if (pos_piece_at(p, sq, NULL) == PT_PAWN) {
                int col; pos_piece_at(p, sq, &col);
                if (col == color) cnt++;
            }
        }
        (void)cnt;
    }
    /* count files with >1 pawn */
    for (int f = 0; f < 8; f++) {
        int cnt = 0;
        for (int r = 0; r < 8; r++) {
            int sq = r*8+f;
            int col = -1;
            if (pos_piece_at(p, sq, &col) == PT_PAWN && col == color) cnt++;
        }
        if (cnt > 1) doubled += cnt - 1;
    }
    return doubled;
}

static int count_isolated(const Position *p, int color) {
    int iso = 0;
    for (int f = 0; f < 8; f++) {
        /* has pawn on file? */
        int has = 0;
        for (int r = 0; r < 8; r++) {
            int sq = r*8+f;
            int col = -1;
            if (pos_piece_at(p, sq, &col) == PT_PAWN && col == color) { has = 1; break; }
        }
        if (!has) continue;
        int left = 0, right = 0;
        if (f > 0) for (int r = 0; r < 8; r++){int col=-1;if(pos_piece_at(p,r*8+(f-1),&col)==PT_PAWN && col==color)left=1;}
        if (f < 7) for (int r = 0; r < 8; r++){int col=-1;if(pos_piece_at(p,r*8+(f+1),&col)==PT_PAWN && col==color)right=1;}
        if (!left && !right) {
            for (int r = 0; r < 8; r++) {
                int sq = r*8+f;
                int col=-1;
                if (pos_piece_at(p, sq, &col) == PT_PAWN && col == color) iso++;
            }
        }
    }
    return iso;
}

static int king_safety(const Position *p, int color, const WeightSet *ws) {
    uint64_t kingbb = p->bb[color][PT_KING];
    if (!kingbb) return 0;
    int ksq = __builtin_ctzll(kingbb);
    int kr = ksq/8, kc = ksq%8;
    int opp = color ^ 1;
    int attacks = 0;
    /* count opponent pieces adjacent to king (simple) */
    for (int dr = -2; dr <= 2; dr++) for (int dc = -2; dc <= 2; dc++) {
        int rr = kr+dr, cc = kc+dc;
        if (rr<0||rr>7||cc<0||cc>7) continue;
        int sq = rr*8+cc;
        uint64_t bit = 1ULL<<sq;
        /* if opponent piece attacks approximated by presence in adjacent? Just count pieces */
        for (int pt = 0; pt < 6; pt++) if (p->bb[opp][pt]&bit) attacks++;
    }
    (void)ws;
    return attacks;
}

int eval_position(const Position *p, const WeightSet *ws) {
    if (!p) return 0;
    WeightSet def;
    const WeightSet *W = ws;
    if (!W || !W->loaded) {
        weights_defaults(&def);
        W = &def;
    }
    /* material + PST */
    double score = 0.0; /* white - black */
    for (int pt = 0; pt < 6; pt++) {
        /* material for pt except king weight 0-ish until used? weight pawn..queen only; king material index not available, use 0 */
        uint64_t wbb = p->bb[COLOR_WHITE][pt];
        uint64_t bbb = p->bb[COLOR_BLACK][pt];
        /* count pieces */
        int wc = popcount64(wbb);
        int bc = popcount64(bbb);
        float matval = 0.0f;
        if (pt == PT_PAWN) matval = W->w[W_PAWN];
        else if (pt == PT_KNIGHT) matval = W->w[W_KNIGHT];
        else if (pt == PT_BISHOP) matval = W->w[W_BISHOP];
        else if (pt == PT_ROOK) matval = W->w[W_ROOK];
        else if (pt == PT_QUEEN) matval = W->w[W_QUEEN];
        else matval = 0.0f;
        score += (wc - bc) * matval;
        /* PST */
        uint64_t t = wbb;
        while (t) {
            int sq = __builtin_ctzll(t); t &= t-1;
            score += W->w[W_PST(pt)+sq];
        }
        t = bbb;
        while (t) {
            int sq = __builtin_ctzll(t); t &= t-1;
            /* mirror square for black */
            int msq = sq ^ 56;
            score -= W->w[W_PST(pt)+msq];
        }
    }

    /* mobility: pseudo move count difference */
    static int buf[512];
    Position pcw = *p; pcw.side = COLOR_WHITE; int wmob = movegen_pseudo(&pcw, buf, 512);
    Position pcb = *p; pcb.side = COLOR_BLACK; int bmob = movegen_pseudo(&pcb, buf, 512);
    score += (wmob - bmob) * W->w[W_MOB];

    /* pawn structure */
    int wdoubled = count_doubled(p, COLOR_WHITE);
    int bdoubled = count_doubled(p, COLOR_BLACK);
    score += (wdoubled - bdoubled) * W->w[W_DOUBLED];
    int wiso = count_isolated(p, COLOR_WHITE);
    int biso = count_isolated(p, COLOR_BLACK);
    score += (wiso - biso) * W->w[W_ISOLATED];

    /* king safety */
    int wks = king_safety(p, COLOR_WHITE, W);
    int bks = king_safety(p, COLOR_BLACK, W);
    score += (bks - wks) * W->w[W_KINGSAFE]; /* more attacks on own king worse */

    /* relative to side to move */
    if (p->side == COLOR_BLACK) score = -score;
    return (int)lroundf(score);
}
