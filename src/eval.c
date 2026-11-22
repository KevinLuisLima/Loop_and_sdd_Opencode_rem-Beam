#include "eval.h"
#include <math.h>

static int color_piece_value_idx(int pt){
    switch(pt){ case 1:return W_MATERIAL_PAWN; case 2:return W_MATERIAL_KNIGHT; case 3:return W_MATERIAL_BISHOP; case 4:return W_MATERIAL_ROOK; case 5:return W_MATERIAL_QUEEN; default:return -1;}
}

static int pst_index(int pt, int sq, int color){
    int base = W_PST_BASE + (pt-1)*64;
    if (color==COLOR_BLACK) sq ^= 56;
    return base + sq;
}

static int eval_color(const Position *p, const Weights *w, int color){
    attacks_init();
    int score=0;
    /* material + PST */
    for(int pc=1; pc<=12; pc++){
        if (piece_color(pc)!=color) continue;
        int pt=piece_type(pc);
        Bitboard bb = p->pieces[pc];
        /* material */
        int mi=color_piece_value_idx(pt);
        if (mi>=0){
            float mv=w->data[mi];
            int cnt=__builtin_popcountll(bb);
            score += (int)(mv*cnt);
        }
        /* PST */
        while (bb){ int sq=__builtin_ctzll(bb); bb&=bb-1; int idx=pst_index(pt, sq, color==COLOR_BLACK? COLOR_BLACK : COLOR_WHITE); /* color perspective */
            /* Note: eval_color receives color (absolute). For white color, white perspective; black color, black perspective (flip sq). */
            /* pst_index already flips if color param black; pass color */
            idx = W_PST_BASE + (pt-1)*64 + ( (color==COLOR_BLACK) ? (sq^56) : sq );
            score += (int)w->data[idx];
        }
    }
    /* occupancy own for files */
    Bitboard own_pawns = (color==COLOR_WHITE)? p->pieces[WP] : p->pieces[BP];
    int filecount[8]={0};
    for (Bitboard bb=own_pawns; bb; bb&=bb-1){ int sq=__builtin_ctzll(bb); filecount[sq&7]++; }
    /* doubled */
    for(int f=0;f<8;f++){
        if (filecount[f]>1) score -= (int)(w->data[W_TERM_DOUBLED]*(filecount[f]-1));
    }
    /* isolated */
    for(int f=0; f<8; f++){
        if (filecount[f]==0) continue;
        int left = (f>0)? filecount[f-1] : 0;
        int right = (f<7)? filecount[f+1] : 0;
        if (left==0 && right==0){
            score -= (int)(w->data[W_TERM_ISOLATED]*filecount[f]);
        }
    }
    /* mobility: pieces (knight,bishop,rook,queen) attacks */
    Bitboard att=0;
    int pieces[4] = { (color==COLOR_WHITE)?WN:BN, (color==COLOR_WHITE)?WB:BB, (color==COLOR_WHITE)?WR:BR, (color==COLOR_WHITE)?WQ:BQ};
    for (int i=0;i<4;i++){
        Bitboard bb=p->pieces[pieces[i]];
        while (bb){ int sq=__builtin_ctzll(bb); bb&=bb-1;
            if (pieces[i]==(color==COLOR_WHITE?WN:BN)) att |= knight_attacks(sq);
            else if (pieces[i]==(color==COLOR_WHITE?WB:BB)) att |= bishop_attacks(sq, p->occ[2]);
            else if (pieces[i]==(color==COLOR_WHITE?WR:BR)) att |= rook_attacks(sq, p->occ[2]);
            else att |= queen_attacks(sq, p->occ[2]);
        }
    }
    /* mobility counts intersections? Count attacked squares not occupied by own */
    Bitboard mob = att & ~((color==COLOR_WHITE)? p->occ[0] : p->occ[1]);
    score += (int)(w->data[W_TERM_MOBILITY]*__builtin_popcountll(mob));
    return score;
}

int eval_position(const Position *p, const Weights *w){
    if (!w || !w->data) return 0;
    int wv = eval_color(p,w,COLOR_WHITE);
    int bv = eval_color(p,w,COLOR_BLACK);
    int score = wv - bv;
    if (p->side==COLOR_BLACK) score = -score;
    /* clamp */
    if (score > 20000) score=20000;
    if (score < -20000) score=-20000;
    return score;
}