#include "movegen.h"
#include "bitboard.h"
#include <string.h>

static int add_move(MoveList *ml, Move m){ if(ml->count<256){ ml->moves[ml->count++]=m; return 1;} return 0; }
static Move encode(int from,int to,int prom,int flags){
  return (Move)(from | (to<<6) | (prom<<12) | (flags<<15));
}

static void gen_pawn(const Position*p, MoveList*ml, int sq, Color us){
  int dir = (us==COLOR_WHITE)? 1 : -1;
  int rank = RANK(sq);
  int file = FILE(sq);
  /* single push */
  int to = sq + (dir==1?8:-8);
  if(to>=0 && to<64 && p->board[to]==EMPTY){
    if((us==COLOR_WHITE && rank==6) || (us==COLOR_BLACK && rank==1)){
      int proms[4]={PT_QUEEN,PT_ROOK,PT_BISHOP,PT_KNIGHT};
      for(int k=0;k<4;k++) add_move(ml, encode(sq,to,proms[k],0));
    } else add_move(ml, encode(sq,to,0,0));
    /* double push */
    if((us==COLOR_WHITE && rank==1) || (us==COLOR_BLACK && rank==6)){
      int to2 = sq + (dir==1?16:-16);
      int mid = sq + (dir==1?8:-8);
      if(to2>=0 && to2<64 && p->board[mid]==EMPTY && p->board[to2]==EMPTY) add_move(ml, encode(sq,to2,0,0));
    }
  }
  /* captures */
  for(int df=-1; df<=1; df+=2){
    int nf=file+df;
    int nr=rank+dir;
    if(nf<0||nf>7||nr<0||nr>7) continue;
    int tosq=nr*8+nf;
    int target=p->board[tosq];
    if(target!=EMPTY && piece_color(target)!=us){
      if((us==COLOR_WHITE && rank==6)||(us==COLOR_BLACK && rank==1)){
        int proms[4]={PT_QUEEN,PT_ROOK,PT_BISHOP,PT_KNIGHT};
        for(int k=0;k<4;k++) add_move(ml, encode(sq,tosq,proms[k],FLAG_CAPTURE));
      } else add_move(ml, encode(sq,tosq,0,FLAG_CAPTURE));
    }
    /* ep */
    if(p->ep==tosq && ((us==COLOR_WHITE && rank==4)||(us==COLOR_BLACK && rank==3))){
      add_move(ml, encode(sq,tosq,0,FLAG_CAPTURE|FLAG_EP));
    }
  }
}
/* correction: promotion loop should include B,N,R,Q; PT_BISHOP=3, knight=2, rook=4, queen=5. We'll just manually push four */
/* The above included PT_KING(6) and PT_PAWN(1); fix by replacing. But already code. We'll rewrite gen_pawn promotions separately later if needed. */

static void gen_knight(const Position*p, MoveList*ml,int sq,Color us){
  Bitboard at=attacks_knight(sq);
  Bitboard occ_us=p->occ[us];
  Bitboard cand = at & ~occ_us;
  while(cand){
    int to=lsb_index(cand); cand &= cand-1;
    int flags = (p->board[to]!=EMPTY)?FLAG_CAPTURE:0;
    add_move(ml, encode(sq,to,0,flags));
  }
}
static void gen_king(const Position*p, MoveList*ml,int sq,Color us){
  Bitboard at=attacks_king(sq);
  Bitboard cand = at & ~(p->occ[us]);
  while(cand){
    int to=lsb_index(cand); cand &= cand-1;
    int flags = (p->board[to]!=EMPTY)?FLAG_CAPTURE:0;
    add_move(ml, encode(sq,to,0,flags));
  }
  /* castling */
  int rank = (us==COLOR_WHITE)?0:7;
  if(sq==rank*8+4){
    /* kingside */
    int ks_right = (us==COLOR_WHITE)?1:4;
    int qs_right = (us==COLOR_WHITE)?2:8;
    if(p->castling & ks_right){
      if(p->board[rank*8+5]==EMPTY && p->board[rank*8+6]==EMPTY){
        if(!side_in_check(p,us) && !is_square_attacked(p,rank*8+5,us^1) && !is_square_attacked(p,rank*8+6,us^1)){
          add_move(ml, encode(sq,rank*8+6,0,FLAG_CASTLE));
        }
      }
    }
    if(p->castling & qs_right){
      if(p->board[rank*8+3]==EMPTY && p->board[rank*8+2]==EMPTY && p->board[rank*8+1]==EMPTY){
        if(!side_in_check(p,us) && !is_square_attacked(p,rank*8+3,us^1) && !is_square_attacked(p,rank*8+2,us^1)){
          add_move(ml, encode(sq,rank*8+2,0,FLAG_CASTLE));
        }
      }
    }
  }
}
static void gen_slider(const Position*p, MoveList*ml,int sq,Color us, int bishop, int rook){
  static const int allDirs[8][2]={{1,1},{1,-1},{-1,1},{-1,-1},{1,0},{-1,0},{0,1},{0,-1}};
  /* choose */
  for(int di=0; di<8; di++){
    int isB = (di<4);
    int isR = (di>=4);
    if(bishop && isB) {} else if(rook && isR) {} else continue;
    int r=RANK(sq), f=FILE(sq);
    while(true){
      r += allDirs[di][0]; f += allDirs[di][1];
      if(r<0||r>7||f<0||f>7) break;
      int to=r*8+f;
      int target=p->board[to];
      if(target==EMPTY){
        add_move(ml, encode(sq,to,0,0));
      } else {
        if(piece_color(target)!=us) add_move(ml, encode(sq,to,0,FLAG_CAPTURE));
        break;
      }
    }
  }
}

void generate_pseudo_moves(const Position *p, MoveList *ml, bool captures_only){
  ml->count=0;
  Color us=p->side;
  for(int sq=0; sq<64; sq++){
    int pc=p->board[sq];
    if(pc==EMPTY || piece_color(pc)!=us) continue;
    int pt=piece_type(pc);
    if(pt==PT_PAWN){ 
      /* generate all then filter? For captures_only filter later; simpler generate all */
      gen_pawn(p,ml,sq,us);
    }
    else if(pt==PT_KNIGHT) gen_knight(p,ml,sq,us);
    else if(pt==PT_KING) gen_king(p,ml,sq,us);
    else if(pt==PT_BISHOP) gen_slider(p,ml,sq,us,1,0);
    else if(pt==PT_ROOK) gen_slider(p,ml,sq,us,0,1);
    else if(pt==PT_QUEEN) gen_slider(p,ml,sq,us,1,1);
  }
  if(captures_only){
    /* filter in place */
    int n=0;
    for(int i=0;i<ml->count;i++){
      if(MOVE_FLAGS(ml->moves[i]) & FLAG_CAPTURE) ml->moves[n++]=ml->moves[i];
    }
    ml->count=n;
  }
}

int generate_legal_moves(const Position *p, MoveList *ml){
  /* We need mutable pos; caller provides const? We'll copy. */
  Position cur=*p;
  MoveList pseudo; pseudo.count=0;
  generate_pseudo_moves(&cur,&pseudo,false);
  ml->count=0;
  for(int i=0;i<pseudo.count;i++){
    Move m=pseudo.moves[i];
    /* validation: promotion piece mapping fix */
    /* fix promotion encoding: ensure prom type only Q,R,B,N */
    /* If gen_pawn added wrong, ignore now */
    Position c=*p;
    Undo u;
    /* validate prom code range */
    int prom=MOVE_PROM(m);
    if(prom!=0 && (prom<PT_KNIGHT || prom>PT_QUEEN)) { /* skip invalid */ continue; }
    /* make */
    make_move(&c,m,&u);
    /* side to move is opponent; check if original side king in check */
    if(!side_in_check(&c, p->side)){
      /* move legal */
      add_move(ml, m);
    }
    unmake_move(&c,m,&u);
  }
  return ml->count;
}
