#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "position.h"
#include "weights.h"
#include "search.h"
#include "movegen.h"

int main(int argc, char **argv){
    int analyze=0, idx=1;
    if (argc>=2 && strcmp(argv[1],"--analyze")==0){ analyze=1; idx=2; }
    if (argc - idx < 1){
        fprintf(stderr, "usage: %s [--analyze] <FEN> [weights] [depth]\n", argv[0]);
        return 1;
    }
    Position pos;
    if (pos_from_fen(&pos, argv[idx]) != 0){
        fprintf(stderr, "invalid FEN\n");
        return 1;
    }
    char wpath[512]; wpath[0]=0;
    if (argc - idx >= 2) snprintf(wpath,sizeof(wpath),"%s",argv[idx+1]);
    else weights_default_path(wpath,sizeof(wpath));
    Weights w; memset(&w,0,sizeof(w));
    if (weights_load(&w,wpath)!=0){
        fprintf(stderr, "weights load failed: %s\n", wpath);
        return 1;
    }
    Move legal[MAX_MOVES];
    int n=gen_legal_moves(&pos,legal);
    if (n==0){
        printf("0000\n");
        weights_free(&w);
        return 0;
    }
    if (n==1){
        char buf[16]; move_to_uci(legal[0],buf);
        if (analyze) printf("bestmove %s score 0\n", buf);
        else printf("%s\n",buf);
        weights_free(&w);
        return 0;
    }
    SearchCtx ctx; search_init(&ctx, 7.0);
    int fd = 0;
    if (analyze){
        fd = (argc - idx >= 3)? atoi(argv[idx+2]) : 6;
        if (fd <= 0) fd = 6;
    }
    Move m=search_bestmove(&ctx,&pos,&w,fd,7.0);
    if (m==0) m=legal[0];
    Move lm=0;
    /* match legal uci */
    char buf[16]; move_to_uci(m,buf);
    for (int i=0;i<n;i++){
        char b2[16]; move_to_uci(legal[i],b2);
        if (strcmp(b2,buf)==0){ lm=legal[i]; break; }
    }
    if (!lm) lm=legal[0];
    move_to_uci(lm,buf);
    if (analyze) printf("bestmove %s score %d\n", buf, ctx.best_root_score);
    else printf("%s\n",buf);
    weights_free(&w);
    return 0;
}