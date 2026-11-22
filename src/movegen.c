#include "movegen.h"
#include <stdio.h>
#include <string.h>

static inline int on_board(int sq){ return sq>=0 && sq<64; }
static inline int file_of(int sq){ return sq&7; }

static void add_move(Move *out, int *n, int from, int to, int promo, int flags){
    if (*n >= MAX_MOVES) return;
    out[(*n)++] = make_move_int(from,to,promo,flags);
}

int gen_pseudo_moves(const Position *p, Move *out, int captures_only){
    attacks_init();
    int n=0;
    int side=p->side;
    int pawn = side==COLOR_WHITE? WP : BP;
    int knight = side==COLOR_WHITE? WN : BN;
    int bishop = side==COLOR_WHITE? WB : BB;
    int rook = side==COLOR_WHITE? WR : BR;
    int queen = side==COLOR_WHITE? WQ : BQ;
    int king = side==COLOR_WHITE? WK : BK;
    /* opponent pieces occupancy */
    Bitboard occ_all=p->occ[2];
    Bitboard own=(side==COLOR_WHITE)? p->occ[0] : p->occ[1];
    Bitboard opp=(side==COLOR_WHITE)? p->occ[1] : p->occ[0];

    /* pawns */
    for (Bitboard bb=p->pieces[pawn]; bb; bb&=bb-1){
        int sq=__builtin_ctzll(bb);
        int r=sq/8;
        (void)r;
        int fwd = (side==COLOR_WHITE)? 8 : -8;
        int promo_rank = (side==COLOR_WHITE)? 6 : 1; /* from square rank? pawn on rank7 advances to rank8 */
        /* quiet pushes */
        if (!captures_only){
            int to=sq+fwd;
            if (on_board(to) && !((occ_all>>to)&1)){
                if (r == promo_rank){
                    for(int pr=1;pr<=4;pr++) add_move(out,&n,sq,to,pr,MF_PROMO);
                } else {
                    add_move(out,&n,sq,to,0,MF_QUIET);
                    /* double push */
                    int start_rank = (side==COLOR_WHITE)? 1 : 6;
                    if (r==start_rank){
                        int to2=sq+2*fwd;
                        if (on_board(to2) && !((occ_all>>to2)&1)) add_move(out,&n,sq,to2,0,MF_DOUBLE);
                    }
                }
            }
        }
        /* captures */
        /* attack squares */
        Bitboard attacks = pawn_attacks(sq, side);
        Bitboard targets = attacks & opp;
        /* en passant */
        if (p->ep>=0){
            Bitboard epm = 1ULL<<p->ep;
            if (attacks & epm) {
                /* ep capture */
                add_move(out,&n,sq,p->ep,0,MF_EP);
            }
        }
        while(targets){
            int to=__builtin_ctzll(targets); targets&=targets-1;
            if (r == promo_rank){
                for(int pr=1;pr<=4;pr++) add_move(out,&n,sq,to,pr,MF_PROMO);
                /* mark capture? promo capture counted as capture in qsearch? For captures_only include yes */
            } else {
                add_move(out,&n,sq,to,0,MF_CAPTURE);
            }
        }
        /* if captures_only and quiet promoted? promotion captures handled */
    }

    /* knights */
    for (Bitboard bb=p->pieces[knight]; bb; bb&=bb-1){
        int sq=__builtin_ctzll(bb);
        Bitboard cand = knight_attacks(sq) & ~own;
        while(cand){
            int to=__builtin_ctzll(cand); cand&=cand-1;
            int iscap = (opp>>to)&1;
            if (captures_only && !iscap) continue;
            int flag = iscap? MF_CAPTURE : MF_QUIET;
            add_move(out,&n,sq,to,0,flag);
        }
    }
    /* bishops */
    for (Bitboard bb=p->pieces[bishop]; bb; bb&=bb-1){
        int sq=__builtin_ctzll(bb);
        Bitboard cand = bishop_attacks(sq, occ_all) & ~own;
        while(cand){
            int to=__builtin_ctzll(cand); cand&=cand-1;
            int iscap = (opp>>to)&1;
            if (captures_only && !iscap) continue;
            int flag = iscap? MF_CAPTURE : MF_QUIET;
            add_move(out,&n,sq,to,0,flag);
        }
    }
    /* rooks */
    for (Bitboard bb=p->pieces[rook]; bb; bb&=bb-1){
        int sq=__builtin_ctzll(bb);
        Bitboard cand = rook_attacks(sq, occ_all) & ~own;
        while(cand){
            int to=__builtin_ctzll(cand); cand&=cand-1;
            int iscap = (opp>>to)&1;
            if (captures_only && !iscap) continue;
            int flag = iscap? MF_CAPTURE : MF_QUIET;
            add_move(out,&n,sq,to,0,flag);
        }
    }
    /* queens */
    for (Bitboard bb=p->pieces[queen]; bb; bb&=bb-1){
        int sq=__builtin_ctzll(bb);
        Bitboard cand = queen_attacks(sq, occ_all) & ~own;
        while(cand){
            int to=__builtin_ctzll(cand); cand&=cand-1;
            int iscap = (opp>>to)&1;
            if (captures_only && !iscap) continue;
            int flag = iscap? MF_CAPTURE : MF_QUIET;
            add_move(out,&n,sq,to,0,flag);
        }
    }
    /* king */
    for (Bitboard bb=p->pieces[king]; bb; bb&=bb-1){
        int sq=__builtin_ctzll(bb);
        Bitboard cand = king_attacks(sq) & ~own;
        while(cand){
            int to=__builtin_ctzll(cand); cand&=cand-1;
            int iscap = (opp>>to)&1;
            if (captures_only && !iscap) continue;
            int flag = iscap? MF_CAPTURE : MF_QUIET;
            add_move(out,&n,sq,to,0,flag);
        }
    }

    /* castling */
    if (!captures_only){
        int ksq = (side==COLOR_WHITE)? (p->pieces[WK]? __builtin_ctzll(p->pieces[WK]):-1) : (p->pieces[BK]? __builtin_ctzll(p->pieces[BK]):-1);
        int oppcol = side^1;
        if (ksq>=0 && !in_check(p, side)){
            /* kingside */
            int right_bit = (side==COLOR_WHITE)? 1 : 4;
            if (p->castling & right_bit){
                int f1=(side==COLOR_WHITE)?5:61, f2=(side==COLOR_WHITE)?6:62, rooksq=(side==COLOR_WHITE)?7:63;
                if (square_piece(p,f1)==0 && square_piece(p,f2)==0 && !is_square_attacked(p,f1,oppcol) && !is_square_attacked(p,f2,oppcol) && square_piece(p,rooksq)!=0){
                    add_move(out,&n,ksq,f2,0,MF_CASTLE);
                }
            }
            /* queenside */
            int left_bit = (side==COLOR_WHITE)? 2 : 8;
            if (p->castling & left_bit){
                int f1=(side==COLOR_WHITE)?3:59, f2=(side==COLOR_WHITE)?2:58, f3=(side==COLOR_WHITE)?1:57, rooksq=(side==COLOR_WHITE)?0:56;
                if (square_piece(p,f1)==0 && square_piece(p,f2)==0 && square_piece(p,f3)==0 && !is_square_attacked(p,f1,oppcol) && !is_square_attacked(p,f2,oppcol) && square_piece(p,rooksq)!=0){
                    add_move(out,&n,ksq,f2,0,MF_CASTLE);
                }
            }
        }
    }

    return n;
}

int gen_legal_moves(Position *p, Move *out){
    Move pseudo[MAX_MOVES];
    int n=gen_pseudo_moves(p,pseudo,0);
    int legal=0;
    for (int i=0;i<n;i++){
        Undo u;
        make_move(p,pseudo[i],&u);
        int side_now = p->side ^ 1; /* side that moved */
        if (!in_check(p, side_now)){
            if (legal < MAX_MOVES) out[legal++]=pseudo[i];
        }
        undo_move(p,&u);
    }
    return legal;
}

int move_is_legal(Position *p, Move m){
    Move out[MAX_MOVES];
    int n=gen_legal_moves(p,out);
    for(int i=0;i<n;i++) if(out[i]==m) return 1;
    return 0;
}

Move parse_move(const char *s){
    if (strlen(s)<4) return 0;
    int from=(s[1]-'1')*8+(s[0]-'a');
    int to=(s[3]-'1')*8+(s[2]-'a');
    int promo=0;
    if (strlen(s)>=5){
        switch(s[4]){
            case 'n': case 'N': promo=1;break;
            case 'b': case 'B': promo=2;break;
            case 'r': case 'R': promo=3;break;
            case 'q': case 'Q': promo=4;break;
        }
    }
    /* determine flag later; just quiet */
    return make_move_int(from,to,promo, MF_QUIET);
}

void move_to_uci(Move m, char *buf){
    int from=MOVE_FROM(m), to=MOVE_TO(m), promo=MOVE_PROMO(m);
    buf[0]='a'+from%8; buf[1]='0'+ (from/8+1)/10; /* not multi-digit? ranks 1-8 */
    /* fix single digit properly: */
    buf[0]='a'+(from%8);
    buf[1]='1'+(from/8);
    buf[2]='a'+(to%8);
    buf[3]='1'+(to/8);
    buf[4]=0;
    if (promo>0){
        const char *pr="nbrq";
        buf[4]=pr[promo-1];
        buf[5]=0;
    }
}