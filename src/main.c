#include "board.h"
#include "search.h"
#include "eval.h"
#include "weights.h"
#include "movegen.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sqToStr(int sq, char *buf) {
    buf[0] = 'a' + (sq%8);
    buf[1] = '1' + (sq/8);
    buf[2] = '\0';
}

static void moveToStr(Move m, char *buf) {
    char a[4], b[4];
    sqToStr(m.from, a);
    sqToStr(m.to, b);
    snprintf(buf, 16, "%s%s", a, b);
    if (m.promo != 0) {
        char p = (m.promo==1)?'n':(m.promo==2)?'b':(m.promo==3)?'r':'q';
        size_t l = strlen(buf);
        buf[l]=p; buf[l+1]='\0';
    }
}

static long perft(Board *b, int depth) {
    if (depth == 0) return 1;
    MoveList ml; ml.count=0;
    generateLegalMoves(b,&ml);
    if (depth == 1) return ml.count;
    long nodes=0;
    for (int i=0;i<ml.count;i++) {
        makeMove(b, ml.moves[i]);
        nodes += perft(b, depth-1);
        unmakeMove(b);
    }
    return nodes;
}

static Weights loadWeightsOrZero(const char *path) {
    Weights w; memset(&w,0,sizeof(w));
    if (path) {
        char err[256]; err[0]='\0';
        if (weightsLoad(path,&w,err,sizeof(err))) return w;
        /* try zero-filled if load fails */
    }
    /* default zeros (with a sane material scale fallback) */
    /* If all zero, fill minimal material values */
    int allzero=1;
    for (int i=0;i<EXPECTED_WEIGHTS;i++) if (w.w[i]!=0.0f) allzero=0;
    if (allzero) {
        w.w[0]=100; w.w[1]=320; w.w[2]=330; w.w[3]=500; w.w[4]=900; w.w[6]=1;
        /* default PSQT zeros */
        w.count=EXPECTED_WEIGHTS;
    }
    return w;
}

static Move parseUciMove(const char *s) {
    Move m; m.from=0; m.to=0; m.promo=0; m.flags=0;
    if (!s || strlen(s) < 4) return m;
    m.from = (s[1]-'1')*8 + (s[0]-'a');
    m.to = (s[3]-'1')*8 + (s[2]-'a');
    if (strlen(s) >= 5) {
        char p = s[4];
        if (p=='n') m.promo=1;
        else if (p=='b') m.promo=2;
        else if (p=='r') m.promo=3;
        else if (p=='q') m.promo=4;
    }
    return m;
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1],"eval")==0) {
        Board b;
        if (argc<3 || !boardFromFEN(&b, argv[2])) boardSetStartpos(&b);
        Weights w = loadWeightsOrZero((argc>=4)?argv[3]:NULL);
        printf("%d\n", evalPosition(&b,&w));
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1],"moves")==0) {
        Board b; 
        if (argc>=3 && !boardFromFEN(&b, argv[2])) boardSetStartpos(&b);
        else if (argc<3) boardSetStartpos(&b);
        MoveList ml; ml.count=0; generateLegalMoves(&b,&ml);
        for (int i=0;i<ml.count;i++) {
            char s[16]; moveToStr(ml.moves[i], s); printf("%s\n", s);
        }
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1],"apply")==0) {
        Board b;
        if (argc<4 || !boardFromFEN(&b, argv[2])) { printf("ERROR bad fen\n"); return 1; }
        Move m = parseUciMove(argv[3]);
        makeMove(&b, m);
        char fen[256]; boardToFEN(&b, fen, sizeof(fen));
        MoveList ml; ml.count=0; generateLegalMoves(&b,&ml);
        const char *st = "continue";
        if (ml.count==0) {
            if (isInCheck(&b, b.side)) st="mate";
            else st="stalemate";
        } else if (b.halfmove >= 100) {
            st="draw50";
        }
        printf("FEN=%s STATUS=%s\n", fen, st);
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1],"perft")==0) {
        int depth = (argc>=3)?atoi(argv[2]):5;
        Board b; boardSetStartpos(&b);
        if (argc>=4) boardFromFEN(&b, argv[3]);
        /* optional fen with spaces? join args? skip */
        long n = perft(&b, depth);
        printf("%ld\n", n);
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1],"--perft-position")==0) {
        /* argv[2]=depth, argv[3]=fen string maybe quoted (single arg) */
        int depth = (argc>=3)?atoi(argv[2]):5;
        Board b; boardSetStartpos(&b);
        if (argc>=4) boardFromFEN(&b, argv[3]);
        long n = perft(&b, depth);
        printf("%ld\n", n);
        return 0;
    }
    const char *fen = NULL;
    const char *wpath = NULL;
    int timeMs = 1000;
    /* parse: first non-option arg fen, then weights, then time */
    /* support flags */
    /* Simple positional */
    if (argc > 1) fen = argv[1];
    if (argc > 2) wpath = argv[2];
    if (argc > 3) timeMs = atoi(argv[3]);
    Board b;
    if (fen && strlen(fen)>0) {
        if (!boardFromFEN(&b, fen)) boardSetStartpos(&b);
    } else {
        boardSetStartpos(&b);
    }
    Weights w = loadWeightsOrZero(wpath);
    SearchStats st; st.nodes=0;
    int budget = timeMs;
    if (budget > 7000) budget = 7000;
    if (budget < 50) budget = 50;
    /* sanity: ensure legal moves; if none, return empty? */
    MoveList ml; ml.count=0; generateLegalMoves(&b,&ml);
    if (ml.count == 0) {
        printf("(none)\n");
        return 0;
    }
    if (ml.count == 1) {
        char s[16]; moveToStr(ml.moves[0], s);
        printf("%s\n", s);
        return 0;
    }
    Move m = searchBestMove(&b, &w, budget, &st);
    char s[16]; moveToStr(m, s);
    printf("%s\n", s);
    return 0;
}
