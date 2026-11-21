#include "eval.h"
#include "movegen.h"
#include "common.h"
#include <string.h>

#define PSQT_BASE 12

static int countMoves(Board *b) {
    MoveList ml; ml.count=0;
    generateLegalMoves(b,&ml);
    return ml.count;
}

int evalPosition(const Board *b, const Weights *w) {
    /* work on copy for move count */
    Board bb = *b;
    const float *W = w->w;
    float score = 0.0f;
    /* material + psqt */
    for (int sq=0; sq<64; sq++) {
        int p = b->pieceOn[sq];
        if (p<0) continue;
        int pt = p%6;
        int col = p/6;
        float mat = W[pt]; /* material value for piece type */
        if (pt == PT_KING) mat = 0.0f; /* king no material */
        float psqt = W[PSQT_BASE + pt*64 + (col==WHITE ? sq : (63-sq))];
        if (col == WHITE) { score += mat + psqt; }
        else { score -= mat + psqt; }
    }
    /* mobility */
    {
        int usMoves = countMoves(&bb);
        /* opponent moves: flip side */
        Board b2 = *b;
        b2.side = 1 - b2.side;
        int oppMoves = countMoves(&b2);
        /* relative to side to move? score currently white perspective. Use global: white mobility - black mobility approximated by moves when side white? simpler: add coefficient * (whiteMoves?) Need count per color. We'll approximate with side to move bonus: */
        if (b->side == WHITE) score += W[6] * (usMoves - oppMoves);
        else score += W[6] * (oppMoves - usMoves);
    }
    /* pawn structure for white and black */
    {
        int filesW[8]={0}, filesB[8]={0};
        for (int sq=0; sq<64; sq++) {
            int p=b->pieceOn[sq];
            if (p<0 || p%6 != PT_PAWN) continue;
            int f=sqFile(sq);
            if (p/6==WHITE) filesW[f]++;
            else filesB[f]++;
        }
        /* doubled */
        for (int f=0; f<8; f++) {
            if (filesW[f] > 1) score += W[7] * (filesW[f]-1);
            if (filesB[f] > 1) score -= W[7] * (filesB[f]-1);
        }
        /* isolated */
        for (int f=0; f<8; f++) {
            if (filesW[f]>0) {
                int neighbors = (f>0?filesW[f-1]:0) + (f<7?filesW[f+1]:0);
                if (neighbors==0) score += W[8]; /* penalty expected negative? w may be negative; just add */
            }
            if (filesB[f]>0) {
                int neighbors = (f>0?filesB[f-1]:0) + (f<7?filesB[f+1]:0);
                if (neighbors==0) score -= W[8];
            }
        }
    }
    /* bishop pair */
    {
        int bw=0, bbp=0;
        for (int sq=0;sq<64;sq++){int p=b->pieceOn[sq]; if(p<0||p%6!=PT_BISHOP)continue; if(p/6==WHITE)bw++;else bbp++;}
        if (bw>=2) score += W[10];
        if (bbp>=2) score -= W[10];
    }
    /* make relative to side to move */
    int result = (int)score;
    if (b->side == BLACK) result = -result;
    return result;
}