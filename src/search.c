#include "search.h"
#include "movegen.h"
#include "eval.h"
#include "common.h"
#include <time.h>
#include <stdlib.h>

static volatile int stopSearch = 0;
static struct timespec deadlineTS;
static int timeLimitMs = 0;
static SearchStats *gstats = NULL;

static long nowMs(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)ts.tv_sec*1000 + (long)ts.tv_nsec/1000000;
}

static void setDeadline(int ms) {
    long n = nowMs() + ms;
    deadlineTS.tv_sec = n/1000;
    deadlineTS.tv_nsec = (n%1000)*1000000;
}

static int timeUp(void) {
    if (timeLimitMs <= 0) return 0;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    if (ts.tv_sec > deadlineTS.tv_sec) return 1;
    if (ts.tv_sec == deadlineTS.tv_sec && ts.tv_nsec >= deadlineTS.tv_nsec) return 1;
    return 0;
}

static const int pieceValue[6] = {100,320,330,500,900,0};

static int moveScore(const Board *b, Move m, const Weights *w) {
    (void)w;
    int from=m.from, to=m.to;
    int victim = b->pieceOn[to];
    int attacker = b->pieceOn[from];
    int score = 0;
    if (victim >= 0) {
        int vv = pieceValue[victim%6];
        int av = pieceValue[attacker%6];
        score += 10000 + vv - av/10;
    }
    if (m.promo != 0) score += 9000 + m.promo*100;
    /* castling */
    int pt = attacker%6;
    if (pt==PT_KING && (to==from+2 || to==from-2)) score += 500;
    return score;
}

static void orderMoves(MoveList *ml, const Board *b, const Weights *w) {
    /* simple insertion sort by score desc */
    int scores[MAX_MOVES];
    for (int i=0;i<ml->count;i++) scores[i]=moveScore(b, ml->moves[i], w);
    for (int i=1;i<ml->count;i++) {
        Move km = ml->moves[i];
        int ks = scores[i];
        int j=i-1;
        while (j>=0 && scores[j] < ks) {
            ml->moves[j+1]=ml->moves[j];
            scores[j+1]=scores[j];
            j--;
        }
        ml->moves[j+1]=km;
        scores[j+1]=ks;
    }
}

static int quiesce(Board *b, const Weights *w, int alpha, int beta, int depth) {
    if (gstats) gstats->nodes++;
    if (timeUp()) { stopSearch=1; return 0; }
    int stand = evalPosition(b,w);
    if (stand >= beta) return beta;
    if (stand > alpha) alpha = stand;
    /* only captures */
    MoveList ml; ml.count=0;
    generateLegalMoves(b,&ml);
    /* filter captures */
    MoveList caps; caps.count=0;
    for (int i=0;i<ml.count;i++) {
        Move m=ml.moves[i];
        if (b->pieceOn[m.to] >= 0 || (b->pieceOn[m.from]%6==PT_PAWN && b->epSquare==m.to)) {
            caps.moves[caps.count++]=m;
        }
    }
    orderMoves(&caps,b,w);
    for (int i=0;i<caps.count;i++) {
        if (stopSearch) return 0;
        makeMove(b, caps.moves[i]);
        int score = -quiesce(b,w,-beta,-alpha,depth+1);
        unmakeMove(b);
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

static int alphaBeta(Board *b, const Weights *w, int depth, int alpha, int beta, int isRoot) {
    (void)isRoot;
    if (gstats) gstats->nodes++;
    if (stopSearch) return 0;
    if (timeUp()) { stopSearch=1; return 0; }
    if (depth <= 0) {
        return quiesce(b,w,alpha,beta,0);
    }
    /* terminal check */
    MoveList ml; ml.count=0;
    generateLegalMoves(b,&ml);
    if (ml.count == 0) {
        if (isInCheck(b, b->side)) return -100000; /* mate */
        return 0; /* stalemate */
    }
    /* 50-move / repetition basic: halfmove clock */
    if (b->halfmove >= 100) return 0;
    /* if only one move */
    orderMoves(&ml,b,w);
    int best = -1000000;
    for (int i=0;i<ml.count;i++) {
        makeMove(b, ml.moves[i]);
        int score = -alphaBeta(b,w,depth-1,-beta,-alpha,0);
        unmakeMove(b);
        if (stopSearch) return best;
        if (score > best) best = score;
        if (score > alpha) alpha = score;
        if (alpha >= beta) break;
    }
    return best;
}

int searchAtDepth(Board *b, const Weights *w, int depth, Move *best) {
    /* returns score */
    MoveList ml; ml.count=0;
    generateLegalMoves(b,&ml);
    if (ml.count==0) return 0;
    if (ml.count==1) { *best=ml.moves[0]; return 0; }
    orderMoves(&ml,b,w);
    int alpha=-1000000, beta=1000000;
    int bestScore=-1000000;
    Move bestM = ml.moves[0];
    for (int i=0;i<ml.count;i++) {
        makeMove(b, ml.moves[i]);
        int score = -alphaBeta(b,w,depth-1,-beta,-alpha,0);
        unmakeMove(b);
        if (stopSearch) {
            /* keep previous best if any? */
            break;
        }
        if (score > bestScore) { bestScore=score; bestM=ml.moves[i]; }
        if (score > alpha) alpha=score;
    }
    *best = bestM;
    return bestScore;
}

Move searchBestMove(Board *b, const Weights *w, int timeMs, SearchStats *stats) {
    gstats = stats;
    if (stats) stats->nodes=0;
    stopSearch=0;
    timeLimitMs = timeMs;
    if (timeMs > 0) setDeadline(timeMs);
    MoveList ml; ml.count=0;
    generateLegalMoves(b,&ml);
    Move bestM;
    if (ml.count==0) { bestM.from=0; bestM.to=0; bestM.promo=0; return bestM; }
    if (ml.count==1) return ml.moves[0];
    bestM = ml.moves[0];
    /* iterative deepening */
    for (int d=1; d<=64 && !stopSearch; d++) {
        Move cur;
        int score = searchAtDepth(b,w,d,&cur);
        if (!stopSearch) {
            bestM = cur;
        } else {
            /* if time ran during depth, discard partial; keep previous */
            break;
        }
        (void)score;
        if (timeUp()) break;
    }
    gstats=NULL;
    timeLimitMs=0;
    return bestM;
}

int evaluateForSearch(const Board *b, const Weights *w) {
    return evalPosition(b,w);
}
