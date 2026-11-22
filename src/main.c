#include "position.h"
#include "search.h"
#include "eval.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(void){
    fprintf(stderr, "usage: chess [--fen FEN] [--time MS] [--weights FILE] [--uci] [--selftest]\n");
}

static int parse_args(int argc, char**argv, char**fen, int*time_ms, char**weights, int*uci_out, int*selftest, int*json_out){
    *fen=NULL; *time_ms=7000; *weights="models/model_0.bin"; *uci_out=0; *selftest=0; *json_out=0;
    for(int i=1;i<argc;i++){
        if(strcmp(argv[i],"--fen")==0 && i+1<argc){ *fen=argv[++i]; }
        else if(strcmp(argv[i],"--time")==0 && i+1<argc){ *time_ms=atoi(argv[++i]); }
        else if(strcmp(argv[i],"--weights")==0 && i+1<argc){ *weights=argv[++i]; }
        else if(strcmp(argv[i],"--uci")==0){ *uci_out=1; }
        else if(strcmp(argv[i],"--selftest")==0){ *selftest=1; }
        else if(strcmp(argv[i],"--json")==0 || strcmp(argv[i],"--score")==0){ *json_out=1; }
        else { return -1; }
    }
    return 0;
}

int main(int argc, char**argv){
    char *fen=NULL; int time_ms=7000; char *weights="models/model_0.bin"; int uci_out=0; int selftest=0; int json_out=0;
    if(parse_args(argc,argv,&fen,&time_ms,&weights,&uci_out,&selftest,&json_out)!=0){ print_usage(); return 2; }

    if(!fen){
        /* read from stdin */
        static char buf[4096];
        if(fgets(buf,sizeof(buf),stdin)){
            /* trim newline */
            buf[strcspn(buf,"\n")]=0;
            fen=buf;
        }
    }

    EvalWeights ew;
    if(eval_load_weights(&ew, weights)!=0){
        fprintf(stderr,"erro: nao foi possivel carregar pesos '%s'\n", weights);
        return 2;
    }

    if(!fen){ fprintf(stderr,"erro: FEN ausente\n"); return 2; }
    Position pos;
    if(position_from_fen(&pos, fen)!=0){ fprintf(stderr,"erro: FEN invalido ou incompleto\n"); return 2; }

    /* handle no legal moves */
    MoveList ml;
    movegen_legal(&pos,&ml,0);
    if(ml.count==0){
        if(uci_out) printf("bestmove 0000\n");
        else printf("0000\n");
        return 0;
    }

    SearchContext ctx;
    search_init(&ctx,&ew);
    int mv = search_best_move(&ctx,&pos,time_ms);
    search_free(&ctx);

    int from, to, promo;
    /* mv stored as from*64+to+(promo<<12) */
    promo = (mv>>12)&7;
    int idx = mv & ((1<<12)-1);
    from = idx / 64;
    to = idx % 64;

    char mstr[8];
    snprintf(mstr, sizeof(mstr), "%c%c%c%c",
        'a'+(from%8), '1'+(from/8),
        'a'+(to%8), '1'+(to/8));
    if(promo!=PIECE_NONE && promo>=PIECE_KNIGHT && promo<=PIECE_QUEEN){
        char pch = "nbrq"[promo-PIECE_KNIGHT];
        size_t l=strlen(mstr);
        mstr[l]=pch; mstr[l+1]='\0';
    }

    if(json_out) printf("{\"move\":\"%s\",\"score\":%d,\"depth\":%d}\n", mstr, ctx.best_score, ctx.depth_reached);
    else if(uci_out) printf("bestmove %s\n", mstr);
    else printf("%s\n", mstr);
    return 0;
}
