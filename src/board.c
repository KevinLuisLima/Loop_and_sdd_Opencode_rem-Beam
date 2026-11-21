#include "board.h"
#include "common.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

void boardInit(Board *b) {
    memset(b, 0, sizeof(*b));
    for (int i=0;i<64;i++) b->pieceOn[i]=-1;
    b->epSquare=-1;
    b->histCount=0;
}

static void setPiece(Board *b, int sq, int color, int pt) {
    b->bb[color][pt] |= (1ULL<<sq);
    b->pieceOn[sq] = color*6 + pt;
}

static void clearPiece(Board *b, int sq) {
    int p = b->pieceOn[sq];
    if (p>=0) {
        int c = p/6, t = p%6;
        b->bb[c][t] &= ~(1ULL<<sq);
    }
    b->pieceOn[sq] = -1;
}

void boardSetStartpos(Board *b) {
    boardInit(b);
    const int backW[8] = {PT_ROOK,PT_KNIGHT,PT_BISHOP,PT_QUEEN,PT_KING,PT_BISHOP,PT_KNIGHT,PT_ROOK};
    for (int f=0; f<8; f++) {
        setPiece(b, squareOf(f,0), WHITE, backW[f]);
        setPiece(b, squareOf(f,1), WHITE, PT_PAWN);
        setPiece(b, squareOf(f,6), BLACK, PT_PAWN);
    }
    const int backB[8] = {PT_ROOK,PT_KNIGHT,PT_BISHOP,PT_QUEEN,PT_KING,PT_BISHOP,PT_KNIGHT,PT_ROOK};
    for (int f=0; f<8; f++) setPiece(b, squareOf(f,7), BLACK, backB[f]);
    b->side = WHITE;
    b->castling = CASTLE_WK|CASTLE_WQ|CASTLE_BK|CASTLE_BQ;
    b->epSquare = -1;
    b->halfmove = 0;
    b->fullmove = 1;
}

int boardFromFEN(Board *b, const char *fen) {
    boardInit(b);
    if (!fen) return 0;
    const char *p = fen;
    int rank = 7, file = 0;
    while (*p && *p != ' ') {
        if (*p == '/') {
            rank--;
            file = 0;
        } else if (isdigit((unsigned char)*p)) {
            int n = *p - '0';
            file += n;
        } else {
            int color = isupper((unsigned char)*p) ? WHITE : BLACK;
            int pt;
            char c = tolower((unsigned char)*p);
            switch (c) {
                case 'p': pt=PT_PAWN; break;
                case 'n': pt=PT_KNIGHT; break;
                case 'b': pt=PT_BISHOP; break;
                case 'r': pt=PT_ROOK; break;
                case 'q': pt=PT_QUEEN; break;
                case 'k': pt=PT_KING; break;
                default: return 0;
            }
            setPiece(b, rank*8+file, color, pt);
            file++;
        }
        p++;
    }
    while (*p == ' ') p++;
    if (*p == 'w') b->side = WHITE;
    else if (*p == 'b') b->side = BLACK;
    else return 0;
    p++;
    while (*p == ' ') p++;
    b->castling = 0;
    if (*p == '-') { p++; }
    else {
        while (*p && *p != ' ') {
            switch (*p) {
                case 'K': b->castling |= CASTLE_WK; break;
                case 'Q': b->castling |= CASTLE_WQ; break;
                case 'k': b->castling |= CASTLE_BK; break;
                case 'q': b->castling |= CASTLE_BQ; break;
            }
            p++;
        }
    }
    while (*p == ' ') p++;
    if (*p == '-') { b->epSquare=-1; p++; }
    else if (isalpha((unsigned char)*p) && isdigit((unsigned char)*(p+1))) {
        int f = tolower((unsigned char)*p)-'a';
        int r = *(p+1)-'1';
        b->epSquare = r*8+f;
        p+=2;
    } else {
        b->epSquare = -1;
    }
    while (*p == ' ') p++;
    if (isdigit((unsigned char)*p)) {
        b->halfmove = atoi(p);
        while (isdigit((unsigned char)*p)) p++;
    }
    while (*p == ' ') p++;
    if (isdigit((unsigned char)*p)) {
        b->fullmove = atoi(p);
    } else b->fullmove = 1;
    if (b->fullmove < 1) b->fullmove = 1;
    return 1;
}

void boardToFEN(const Board *b, char *buf, size_t bufsz) {
    char tmp[128];
    size_t ti=0;
    for (int r=7; r>=0; r--) {
        int empty=0;
        for (int f=0; f<8; f++) {
            int sq=r*8+f;
            int p=b->pieceOn[sq];
            if (p<0) { empty++; }
            else {
                if (empty>0){ tmp[ti++] = '0'+empty; empty=0; }
                char c;
                int t=p%6, col=p/6;
                switch(t){ case PT_PAWN:c='p';break;case PT_KNIGHT:c='n';break;case PT_BISHOP:c='b';break;case PT_ROOK:c='r';break;case PT_QUEEN:c='q';break;default:c='k';break;}
                if (col==WHITE) c=toupper((unsigned char)c);
                tmp[ti++]=c;
            }
        }
        if (empty>0) tmp[ti++]='0'+empty;
        if (r>0) tmp[ti++]='/';
    }
    tmp[ti]='\0';
    char sidestr[4]; sidestr[0]= b->side==WHITE?'w':'b'; sidestr[1]='\0';
    char cast[5]; int ci=0;
    if (b->castling & CASTLE_WK) cast[ci++]='K';
    if (b->castling & CASTLE_WQ) cast[ci++]='Q';
    if (b->castling & CASTLE_BK) cast[ci++]='k';
    if (b->castling & CASTLE_BQ) cast[ci++]='q';
    if (ci==0) { cast[ci++]='-'; }
    cast[ci]='\0';
    char epstr[4];
    if (b->epSquare>=0) {
        int f=sqFile(b->epSquare), r=sqRank(b->epSquare);
        epstr[0]='a'+f; epstr[1]='1'+r; epstr[2]='\0';
    } else { epstr[0]='-'; epstr[1]='\0'; }
    snprintf(buf, bufsz, "%s %s %s %s %d %d", tmp, sidestr, cast, epstr, b->halfmove, b->fullmove);
}

Bitboard boardOccupancy(const Board *b, int color) {
    Bitboard o=0;
    for (int t=0;t<6;t++) o |= b->bb[color][t];
    return o;
}
Bitboard boardAllOccupancy(const Board *b) {
    return boardOccupancy(b,WHITE) | boardOccupancy(b,BLACK);
}

int pieceColor(int p){ return p<0?-1:p/6; }
int pieceType(int p){ return p<0?-1:p%6; }

Bitboard slidingAttacks(int sq, Bitboard occ, int isRook, int isBishop) {
    Bitboard b=0;
    int dirs[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
    for (int d=0; d<8; d++) {
        if (d<4 && !isRook) continue;
        if (d>=4 && !isBishop) continue;
        int f=sqFile(sq), r=sqRank(sq);
        int df=dirs[d][0], dr=dirs[d][1];
        while (1) {
            f+=df; r+=dr;
            if (f<0||f>=8||r<0||r>=8) break;
            int s=r*8+f;
            b |= 1ULL<<s;
            if (occ & (1ULL<<s)) break;
        }
    }
    return b;
}

int isSquareAttacked(const Board *b, int sq, int byColor) {
    /* pawn */
    {
        int r=sqRank(sq), f=sqFile(sq);
        int pr = r - ((byColor==WHITE)?1:-1);
        for (int df=-1;df<=1;df+=2){
            int pf=f+df;
            if (pf>=0&&pf<8&&pr>=0&&pr<8) {
                int origin=pr*8+pf;
                int p=b->pieceOn[origin];
                if (p>=0 && p/6==byColor && p%6==PT_PAWN) return 1;
            }
        }
    }
    /* knights */
    {
        static const int dx[8]={1,2,2,1,-1,-2,-2,-1};
        static const int dy[8]={2,1,-1,-2,-2,-1,1,2};
        for (int i=0;i<8;i++){
            int f=sqFile(sq)-dx[i], r=sqRank(sq)-dy[i];
            if (f>=0&&f<8&&r>=0&&r<8){
                int origin=r*8+f;
                int p=b->pieceOn[origin];
                if (p>=0 && p/6==byColor && p%6==PT_KNIGHT) return 1;
            }
        }
    }
    /* king */
    {
        for (int df=-1;df<=1;df++) for (int dr=-1;dr<=1;dr++){
            if (df==0&&dr==0) continue;
            int f=sqFile(sq)-df, r=sqRank(sq)-dr;
            if (f>=0&&f<8&&r>=0&&r<8){
                int origin=r*8+f;
                int p=b->pieceOn[origin];
                if (p>=0 && p/6==byColor && p%6==PT_KING) return 1;
            }
        }
    }
    /* rooks/queens */
    {
        int dirs[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
        for (int d=0; d<4; d++){
            int f=sqFile(sq), r=sqRank(sq);
            int df=dirs[d][0], dr=dirs[d][1];
            while (1){
                f+=df; r+=dr;
                if (f<0||f>=8||r<0||r>=8) break;
                int origin=r*8+f;
                int p=b->pieceOn[origin];
                if (p>=0){
                    if (p/6==byColor && (p%6==PT_ROOK || p%6==PT_QUEEN)) return 1;
                    break;
                }
            }
        }
    }
    /* bishops/queens */
    {
        int dirs[4][2]={{1,1},{1,-1},{-1,1},{-1,-1}};
        for (int d=0; d<4; d++){
            int f=sqFile(sq), r=sqRank(sq);
            int df=dirs[d][0], dr=dirs[d][1];
            while (1){
                f+=df; r+=dr;
                if (f<0||f>=8||r<0||r>=8) break;
                int origin=r*8+f;
                int p=b->pieceOn[origin];
                if (p>=0){
                    if (p/6==byColor && (p%6==PT_BISHOP || p%6==PT_QUEEN)) return 1;
                    break;
                }
            }
        }
    }
    return 0;
}

void makeMove(Board *b, Move m) {
    int from=m.from, to=m.to;
    int p = b->pieceOn[from];
    int color = p/6, pt = p%6;
    Undo *u = &b->history[b->histCount++];
    u->move = m;
    u->prevCastling = b->castling;
    u->prevEp = b->epSquare;
    u->prevHalfmove = b->halfmove;
    u->prevFullmove = b->fullmove;
    u->prevSide = b->side;
    int capturedSq = to;
    int captured = b->pieceOn[to];
    if (pt==PT_PAWN && b->epSquare>=0 && to == b->epSquare && sqRank(from)!=sqRank(to) ) {
        int epCapSq = (color==WHITE) ? to-8 : to+8;
        capturedSq = epCapSq;
        captured = b->pieceOn[epCapSq];
    }
    u->captured = captured;
    u->capturedSq = capturedSq;

    if (pt==PT_PAWN || captured>=0) b->halfmove = 0;
    else b->halfmove++;

    clearPiece(b, from);
    if (captured>=0) clearPiece(b, capturedSq);
    int promoPt = pt;
    if (m.promo != 0 && pt==PT_PAWN) {
        if (m.promo==1) promoPt=PT_KNIGHT;
        else if (m.promo==2) promoPt=PT_BISHOP;
        else if (m.promo==3) promoPt=PT_ROOK;
        else if (m.promo==4) promoPt=PT_QUEEN;
        setPiece(b, to, color, promoPt);
    } else {
        setPiece(b, to, color, pt);
    }

    if (pt==PT_KING) {
        if (from==4 && to==6) { clearPiece(b, 7); setPiece(b, 5, WHITE, PT_ROOK); }
        else if (from==4 && to==2) { clearPiece(b,0); setPiece(b,3,WHITE,PT_ROOK); }
        else if (from==60 && to==62) { clearPiece(b,63); setPiece(b,61,BLACK,PT_ROOK); }
        else if (from==60 && to==58) { clearPiece(b,56); setPiece(b,59,BLACK,PT_ROOK); }
    }

    if (pt==PT_KING) {
        if (color==WHITE) b->castling &= ~(CASTLE_WK|CASTLE_WQ);
        else b->castling &= ~(CASTLE_BK|CASTLE_BQ);
    }
    if (pt==PT_ROOK) {
        if (from==7) b->castling &= ~CASTLE_WK;
        if (from==0) b->castling &= ~CASTLE_WQ;
        if (from==63) b->castling &= ~CASTLE_BK;
        if (from==56) b->castling &= ~CASTLE_BQ;
    }
    if (captured>=0 && captured%6==PT_ROOK) {
        if (capturedSq==7) b->castling &= ~CASTLE_WK;
        if (capturedSq==0) b->castling &= ~CASTLE_WQ;
        if (capturedSq==63) b->castling &= ~CASTLE_BK;
        if (capturedSq==56) b->castling &= ~CASTLE_BQ;
    }

    b->epSquare = -1;
    if (pt==PT_PAWN) {
        int fromRank = sqRank(from), toRank=sqRank(to);
        if (fromRank==1 && toRank==3) b->epSquare = squareOf(sqFile(from),2);
        if (fromRank==6 && toRank==4) b->epSquare = squareOf(sqFile(from),5);
    }

    b->side = 1 - b->side;
    if (b->side==WHITE) b->fullmove++;
}

void unmakeMove(Board *b) {
    Undo *u = &b->history[--b->histCount];
    Move m = u->move;
    int from=m.from, to=m.to;
    b->side = u->prevSide;
    int color = b->side;
    int pt = b->pieceOn[to]%6;
    clearPiece(b, to);
    if (u->captured>=0) {
        int cc = u->captured/6, ct = u->captured%6;
        setPiece(b, u->capturedSq, cc, ct);
    }
    int origPt = pt;
    if (m.promo != 0) origPt = PT_PAWN;
    setPiece(b, from, color, origPt);

    if (origPt==PT_KING) {
        if (from==4 && to==6) { clearPiece(b,5); setPiece(b,7,WHITE,PT_ROOK);}
        else if (from==4 && to==2){ clearPiece(b,3); setPiece(b,0,WHITE,PT_ROOK);}
        else if (from==60 && to==62){ clearPiece(b,61); setPiece(b,63,BLACK,PT_ROOK);}
        else if (from==60 && to==58){ clearPiece(b,59); setPiece(b,56,BLACK,PT_ROOK);}
    }

    b->castling = u->prevCastling;
    b->epSquare = u->prevEp;
    b->halfmove = u->prevHalfmove;
    b->fullmove = u->prevFullmove;
}
