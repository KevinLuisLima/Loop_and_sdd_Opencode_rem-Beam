#include "movegen.h"
#include "common.h"
#include <stdlib.h>

static void addMove(MoveList *ml, int from, int to, int promo) {
    if (ml->count < MAX_MOVES) {
        ml->moves[ml->count].from=from;
        ml->moves[ml->count].to=to;
        ml->moves[ml->count].promo=promo;
        ml->moves[ml->count].flags=0;
        ml->count++;
    }
}

static int onBoard(int f,int r){ return f>=0&&f<8&&r>=0&&r<8; }

int isInCheck(const Board *b, int color) {
    int kingSq=-1;
    for (int i=0;i<64;i++) {
        int p=b->pieceOn[i];
        if (p>=0 && p/6==color && p%6==PT_KING) { kingSq=i; break; }
    }
    if (kingSq<0) return 1;
    return isSquareAttacked(b, kingSq, 1-color);
}

static void genPseudo(Board *b, MoveList *ml) {
    int us=b->side;
    int them=1-us;
    for (int sq=0; sq<64; sq++) {
        int p=b->pieceOn[sq];
        if (p<0 || p/6 != us) continue;
        int pt=p%6;
        int f=sqFile(sq), r=sqRank(sq);
        switch(pt) {
            case PT_PAWN: {
                int dir = (us==WHITE)?1:-1;
                int startRank = (us==WHITE)?1:6;
                int promoRank = (us==WHITE)?7:0;
                int toSq = sq + dir*8;
                if (onBoard(f, r+dir) && b->pieceOn[toSq]<0) {
                    if (sqRank(toSq)==promoRank) {
                        for (int pr=1; pr<=4; pr++) addMove(ml, sq, toSq, pr);
                    } else addMove(ml, sq, toSq, 0);
                    if (r==startRank) {
                        int to2 = sq + dir*16;
                        if (b->pieceOn[to2]<0) addMove(ml, sq, to2, 0);
                    }
                }
                for (int df=-1; df<=1; df+=2) {
                    int nf=f+df, nr=r+dir;
                    if (!onBoard(nf,nr)) continue;
                    int ts=nr*8+nf;
                    int tp=b->pieceOn[ts];
                    if (tp>=0 && tp/6==them) {
                        if (nr==promoRank) for (int pr=1;pr<=4;pr++) addMove(ml,sq,ts,pr);
                        else addMove(ml,sq,ts,0);
                    }
                    if (b->epSquare>=0 && b->epSquare==ts) {
                        addMove(ml, sq, ts, 0);
                    }
                }
                break;
            }
            case PT_KNIGHT: {
                static const int dx[8]={1,2,2,1,-1,-2,-2,-1};
                static const int dy[8]={2,1,-1,-2,-2,-1,1,2};
                for (int i=0;i<8;i++) {
                    int nf=f+dx[i], nr=r+dy[i];
                    if (!onBoard(nf,nr)) continue;
                    int ts=nr*8+nf;
                    int tp=b->pieceOn[ts];
                    if (tp>=0 && tp/6==us) continue;
                    addMove(ml, sq, ts, 0);
                }
                break;
            }
            case PT_BISHOP:
            case PT_ROOK:
            case PT_QUEEN: {
                int isB = (pt==PT_BISHOP||pt==PT_QUEEN);
                int isR = (pt==PT_ROOK||pt==PT_QUEEN);
                int dirs[8][2]={{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
                for (int d=0; d<8; d++) {
                    if (d<4 && !isR) continue;
                    if (d>=4 && !isB) continue;
                    int cf=f, cr=r;
                    while (1) {
                        cf += dirs[d][0]; cr += dirs[d][1];
                        if (!onBoard(cf,cr)) break;
                        int ts=cr*8+cf;
                        int tp=b->pieceOn[ts];
                        if (tp>=0 && tp/6==us) break;
                        addMove(ml,sq,ts,0);
                        if (tp>=0 && tp/6==them) break;
                    }
                }
                break;
            }
            case PT_KING: {
                for (int df=-1; df<=1; df++) for (int dr=-1; dr<=1; dr++) {
                    if (df==0&&dr==0) continue;
                    int nf=f+df, nr=r+dr;
                    if (!onBoard(nf,nr)) continue;
                    int ts=nr*8+nf;
                    int tp=b->pieceOn[ts];
                    if (tp>=0 && tp/6==us) continue;
                    addMove(ml,sq,ts,0);
                }
                if (us==WHITE) {
                    if (sq==4) {
                        if ((b->castling & CASTLE_WK) && b->pieceOn[5]<0 && b->pieceOn[6]<0) {
                            addMove(ml,4,6,0);
                        }
                        if ((b->castling & CASTLE_WQ) && b->pieceOn[3]<0 && b->pieceOn[2]<0 && b->pieceOn[1]<0) {
                            addMove(ml,4,2,0);
                        }
                    }
                } else {
                    if (sq==60) {
                        if ((b->castling & CASTLE_BK) && b->pieceOn[61]<0 && b->pieceOn[62]<0) {
                            addMove(ml,60,62,0);
                        }
                        if ((b->castling & CASTLE_BQ) && b->pieceOn[59]<0 && b->pieceOn[58]<0 && b->pieceOn[57]<0) {
                            addMove(ml,60,58,0);
                        }
                    }
                }
                break;
            }
        }
    }
}

static int isLegal(Board *b, Move m) {
    makeMove(b, m);
    int mover = 1 - b->side;
    int inChk = isInCheck(b, mover);
    unmakeMove(b);
    return !inChk;
}

static int castlingLegal(Board *b, Move m) {
    int us=b->side;
    if (isInCheck(b, us)) return 0;
    int them=1-us;
    if (m.to == m.from+2) {
        if (isSquareAttacked(b, m.from+1, them)) return 0;
        if (isSquareAttacked(b, m.to, them)) return 0;
    } else if (m.to == m.from-2) {
        if (isSquareAttacked(b, m.from-1, them)) return 0;
        if (isSquareAttacked(b, m.to, them)) return 0;
    }
    return 1;
}

void generateLegalMoves(Board *b, MoveList *ml) {
    ml->count=0;
    MoveList pseudo; pseudo.count=0;
    genPseudo(b,&pseudo);
    for (int i=0;i<pseudo.count;i++) {
        Move m=pseudo.moves[i];
        int pt = b->pieceOn[m.from]%6;
        int castling = (pt==PT_KING && (m.to==m.from+2 || m.to==m.from-2));
        if (castling) {
            if (!castlingLegal(b,m)) continue;
        }
        if (!isLegal(b,m)) continue;
        addMove(ml, m.from, m.to, m.promo);
    }
}
