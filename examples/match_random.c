#include "position.h"
#include "search.h"
#include "eval.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int greedy_move(Position *p){
    MoveList ml; movegen_legal(p,&ml,0);
    if(ml.count==0) return -1;
    int best=0,maxscore=-1000000;
    for(int i=0;i<ml.count;i++){
        // score simple MVV
        int s=0;
        Position tmp=*p; Undo u; make_move(&tmp,&ml.moves[i],&u);
        // prefer captures by checking if to had piece
        (void)0;
        unmake_move(&tmp,&ml.moves[i],&u);
        ml.moves[i].score = 0;
        // material gain estimate:
        int victim=0;
        for(int c=0;c<2;c++) for(int t=0;t<6;t++) if(p->pieces[c][t]&(1ULL<<ml.moves[i].to)) victim=t;
        if(victim) s += 10 + victim;
        if(s>maxscore){ maxscore=s; best=i; }
    }
    return best;
}

int main(void){
    EvalWeights ew; eval_weights_default(&ew);
    // load actual model 0
    if(eval_load_weights(&ew,"models/model_0.bin")!=0){}
    unsigned rng=12345;
    int games[2]; // 0 random, 1 greedy
    for(int mode=0; mode<2; mode++){
        int engineWins=0, illegal=0, totalGames=20;
        for(int g=0; g<totalGames; g++){
            Position pos; position_from_fen(&pos, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
            int engineIsWhite = (g%2==0);
            int moves=0;
            int winner=0; /* 0 draw, 1 white, 2 black */
            while(moves < 150){
                MoveList ml; movegen_legal(&pos,&ml,0);
                if(ml.count==0){
                    if(in_check(&pos,pos.side)) winner = (pos.side==COLOR_WHITE?2:1);
                    else winner=0;
                    break;
                }
                Move chosen;
                int engineTurn = (pos.side==COLOR_WHITE)==engineIsWhite;
                if(engineTurn){
                    SearchContext ctx; search_init(&ctx,&ew);
                    int mv = search_best_move(&ctx,&pos,10);
                    search_free(&ctx);
                    if(mv<0){ illegal++; break; }
                    int idx=-1;
                    for(int i=0;i<ml.count;i++){
                        if(ml.moves[i].from*64+ml.moves[i].to + (ml.moves[i].promo<<12) == mv){ idx=i; break;}
                    }
                    if(idx<0){ illegal++; break; }
                    chosen=ml.moves[idx];
                } else {
                    if(mode==0){
                        rng = rng*1103515245+12345;
                        chosen = ml.moves[rng % ml.count];
                    } else {
                        int gi=greedy_move(&pos);
                        chosen = ml.moves[gi];
                    }
                }
                Undo u; Position tmp=pos; make_move(&tmp,&chosen,&u);
                if(in_check(&tmp, pos.side)){ illegal++; break; }
                pos=tmp; moves++;
            }
            // if loop ended due moves
            if(winner==0 && moves>=150) winner=0;
            // determine by material if no mate
            if(winner==0){
                // engine win if opponent has no king?
                winner=0;
            }
            int engineWon=0;
            if(winner==1 && engineIsWhite) engineWon=1;
            if(winner==2 && !engineIsWhite) engineWon=1;
            if(engineWon) engineWins++;
        }
        printf("mode=%d games=%d engineWins=%d illegal=%d (winrate=%.2f)\n",
            mode,totalGames,engineWins,illegal,(double)engineWins/totalGames*100.0);
    }
    return 0;
}
