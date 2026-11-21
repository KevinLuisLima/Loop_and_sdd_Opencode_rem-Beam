#include "position.h"
#include "bitboard.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static uint64_t Z_PIECE[13][64];
static uint64_t Z_SIDE;
static uint64_t Z_CASTLE[16];
static uint64_t Z_EP[64];
static int zobrist_ready=0;

static uint64_t splitmix(uint64_t *x){
  uint64_t z=(*x+=0x9e3779b97f4a7c15ULL);
  z=(z^(z>>30))*0xbf58476d1ce4e5b9ULL;
  z=(z^(z>>27))*0x94d049bb133111b9ULL;
  return z^(z>>31);
}
static void init_zobrist(void){
  if(zobrist_ready) return;
  uint64_t x=12345ULL;
  for(int p=0;p<13;p++) for(int s=0;s<64;s++) Z_PIECE[p][s]=splitmix(&x);
  Z_SIDE=splitmix(&x);
  for(int i=0;i<16;i++) Z_CASTLE[i]=splitmix(&x);
  for(int i=0;i<64;i++) Z_EP[i]=splitmix(&x);
  zobrist_ready=1;
}

static void rebuild(Position*p){
  for(int i=0;i<13;i++) p->bb[i]=0;
  p->occ[0]=p->occ[1]=p->occ_all=0;
  for(int s=0;s<64;s++){
    int pc=p->board[s];
    if(pc!=EMPTY){
      p->bb[pc] |= 1ULL<<s;
      Color c = piece_color(pc);
      p->occ[c] |= 1ULL<<s;
      p->occ_all |= 1ULL<<s;
    }
  }
}
static uint64_t compute_hash(const Position*p){
  uint64_t h=0;
  for(int s=0;s<64;s++){
    if(p->board[s]!=EMPTY) h ^= Z_PIECE[p->board[s]][s];
  }
  if(p->side==COLOR_BLACK) h ^= Z_SIDE;
  h ^= Z_CASTLE[p->castling & 15];
  if(p->ep>=0 && p->ep<64) h ^= Z_EP[p->ep];
  return h;
}

static int char_to_piece(char c){
  switch(c){
    case 'P':return W_PAWN; case 'N':return W_KNIGHT; case 'B':return W_BISHOP;
    case 'R':return W_ROOK; case 'Q':return W_QUEEN; case 'K':return W_KING;
    case 'p':return B_PAWN; case 'n':return B_KNIGHT; case 'b':return B_BISHOP;
    case 'r':return B_ROOK; case 'q':return B_QUEEN; case 'k':return B_KING;
  }
  return EMPTY;
}

int pos_from_fen(Position *p, const char *fen){
  if(!p||!fen) return -1;
  memset(p,0,sizeof(*p));
  init_zobrist();
  for(int i=0;i<64;i++) p->board[i]=EMPTY;
  p->ep=-1; p->castling=0; p->halfmove=0; p->fullmove=1; p->side=COLOR_WHITE;

  /* copy fen */
  char buf[512];
  snprintf(buf,sizeof(buf),"%s",fen);
  /* tokenize by spaces */
  char *tok=strtok(buf," ");
  /* board */
  if(!tok) return -1;
  int sq=56;
  for(const char *c=tok; *c; c++){
    if(*c=='/'){ sq -= 16; /* move to start of next rank? Let's instead track */ }
    else if(*c>='1' && *c<='8'){
      int n=*c-'0';
      for(int i=0;i<n;i++){ if(sq>=0&&sq<64) p->board[sq++]=EMPTY; }
    } else {
      int pc=char_to_piece(*c);
      if(pc!=EMPTY && sq>=0 && sq<64){ p->board[sq]=pc; sq++; }
    }
  }
  /* Oops row handling: standard algorithm: start sq=56, on '/' sq -= 16? Actually when encountering '/', subtract 8? Let's fix by redoing parse with rank index. */
  /* Re-evaluate clean below not possible; we'll just redo manually here by resetting. */
  for(int i=0;i<64;i++) p->board[i]=EMPTY;
  /* parse again cleanly */
  snprintf(buf,sizeof(buf),"%s",fen);
  char *t=strtok(buf," ");
  int rank=7, file=0;
  for(const char *c=t; *c; c++){
    if(*c=='/'){ rank--; file=0; }
    else if(*c>='1' && *c<='8'){ file += *c-'0'; }
    else {
      int pc=char_to_piece(*c);
      if(pc!=EMPTY && rank>=0 && rank<8 && file>=0 && file<8){
        p->board[rank*8+file]=pc; file++;
      }
    }
  }
  /* side */
  t=strtok(NULL," ");
  if(t && t[0]=='b') p->side=COLOR_BLACK; else p->side=COLOR_WHITE;
  /* castling */
  t=strtok(NULL," ");
  if(t){
    for(const char*c=t;*c;c++){
      if(*c=='K') p->castling|=1;
      if(*c=='Q') p->castling|=2;
      if(*c=='k') p->castling|=4;
      if(*c=='q') p->castling|=8;
    }
  }
  /* ep */
  t=strtok(NULL," ");
  if(t && t[0]!='-'){
    int f=t[0]-'a', r=t[1]-'1';
    if(f>=0&&f<8&&r>=0&&r<8) p->ep=r*8+f;
  }
  /* halfmove */
  t=strtok(NULL," ");
  if(t) p->halfmove=atoi(t);
  /* fullmove */
  t=strtok(NULL," ");
  if(t) p->fullmove=atoi(t);

  /* validation */
  int wk=0,bk=0;
  for(int s=0;s<64;s++){
    if(p->board[s]==W_KING) wk++;
    if(p->board[s]==B_KING) bk++;
  }
  if(wk!=1 || bk!=1) { return -1; }
  rebuild(p);
  p->hash=compute_hash(p);
  return 0;
}

void pos_init_startpos(Position *p){ pos_from_fen(p,"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"); }

int pos_to_fen(const Position *p, char *buf, size_t bufsz){
  if(!p||!buf) return -1;
  size_t pos=0;
  for(int r=7;r>=0;r--){
    int empty=0;
    for(int f=0;f<8;f++){
      int pc=p->board[r*8+f];
      if(pc==EMPTY) empty++;
      else {
        if(empty){ buf[pos++]= '0'+empty; empty=0; }
        char ch="?PNBRQKpnbrqk"[pc];
        buf[pos++]=ch;
      }
    }
    if(empty){ buf[pos++]='0'+empty; }
    if(r>0) buf[pos++]='/';
  }
  /* side */
  pos += snprintf(buf+pos, bufsz-pos, " %c ", p->side==COLOR_WHITE?'w':'b');
  /* castling */
  char cbuf[8]; int ci=0;
  if(p->castling&1) cbuf[ci++]='K';
  if(p->castling&2) cbuf[ci++]='Q';
  if(p->castling&4) cbuf[ci++]='k';
  if(p->castling&8) cbuf[ci++]='q';
  if(ci==0) cbuf[ci++]='-';
  cbuf[ci]=0;
  pos += snprintf(buf+pos, bufsz-pos, "%s ", cbuf);
  if(p->ep>=0){
    char eb[3]; eb[0]='a'+FILE(p->ep); eb[1]='1'+RANK(p->ep); eb[2]=0;
    pos += snprintf(buf+pos, bufsz-pos, "%s ", eb);
  } else pos += snprintf(buf+pos, bufsz-pos, "- ");
  pos += snprintf(buf+pos, bufsz-pos, "%d %d", p->halfmove, p->fullmove);
  buf[pos]=0;
  return 0;
}

uint64_t pos_hash(const Position *p){ return p->hash; }

static inline void add_piece(Position*p, int sq, int pc){
  p->board[sq]=pc;
  p->bb[pc] |= 1ULL<<sq;
  p->occ[piece_color(pc)] |= 1ULL<<sq;
  p->occ_all |= 1ULL<<sq;
  p->hash ^= Z_PIECE[pc][sq];
}
static inline void remove_piece(Position*p, int sq, int pc){
  p->board[sq]=EMPTY;
  p->bb[pc] &= ~(1ULL<<sq);
  p->occ[piece_color(pc)] &= ~(1ULL<<sq);
  p->occ_all &= ~(1ULL<<sq);
  p->hash ^= Z_PIECE[pc][sq];
}

void make_move(Position *p, Move m, Undo *u){
  init_zobrist();
  int from=MOVE_FROM(m);
  int to=MOVE_TO(m);
  int prom=MOVE_PROM(m);
  int flags=MOVE_FLAGS(m);
  u->from=from; u->to=to; u->promoted=prom; u->captured=p->board[to];
  u->castling=p->castling; u->ep=p->ep; u->halfmove=p->halfmove; u->hash=p->hash; u->flags=flags;

  int pc=p->board[from];
  Color us=p->side;
  Color them = us^1;

  /* hash side out now? We'll update at end */
  /* remove moving piece from from */
  remove_piece(p,from,pc);
  /* capture */
  int captured = p->board[to];
  if(captured!=EMPTY){
    remove_piece(p,to,captured);
    p->hash ^= 0; /* already removed */
  }
  /* ep capture */
  if(flags & FLAG_EP){
    int cap_sq = (us==COLOR_WHITE)? to-8 : to+8;
    int cap_piece = (us==COLOR_WHITE)? B_PAWN : W_PAWN;
    remove_piece(p,cap_sq,cap_piece);
    u->captured = cap_piece; /* store */
  }
  /* promotion */
  int placed = pc;
  if(prom!=0){
    placed = (us==COLOR_WHITE) ? prom : (prom+6);
  }
  add_piece(p,to,placed);

  /* castling rook move */
  if(flags & FLAG_CASTLE){
    if(to==6){ /* white ks */
      remove_piece(p,7,W_ROOK); add_piece(p,5,W_ROOK);
    } else if(to==2){ /* white qs */
      remove_piece(p,0,W_ROOK); add_piece(p,3,W_ROOK);
    } else if(to==62){ /* black ks */
      remove_piece(p,63,B_ROOK); add_piece(p,61,B_ROOK);
    } else if(to==58){ /* black qs */
      remove_piece(p,56,B_ROOK); add_piece(p,59,B_ROOK);
    }
  }

  /* castling rights updates */
  if(pc==W_KING){ p->castling &= ~3; }
  if(pc==B_KING){ p->castling &= ~12; }
  if(from==7||to==7) p->castling &= ~1;
  if(from==0||to==0) p->castling &= ~2;
  if(from==63||to==63) p->castling &= ~4;
  if(from==56||to==56) p->castling &= ~8;

  /* ep square */
  p->ep = -1;
  if(pc==W_PAWN && from/8==1 && to/8==3){ p->ep = from+8; p->hash ^= Z_EP[p->ep? p->ep:0]?0:0; if(p->ep>=0) p->hash ^= Z_EP[p->ep]; }
  /* Actually need hash update for ep change; handled by recompute? We'll just set p->hash already mutated by piece moves; ep hash should be toggled before/after. To keep simple, recompute hash at end. */
  if(pc==B_PAWN && from/8==6 && to/8==4){ p->ep = from-8; }

  /* halfmove */
  if(pc==W_PAWN || pc==B_PAWN || captured!=EMPTY || (flags&FLAG_EP)) p->halfmove=0;
  else p->halfmove++;

  if(us==COLOR_BLACK) p->fullmove++;

  /* side */
  p->side = them;
  /* recompute hash cleanly to avoid inconsistencies */
  p->hash = compute_hash(p);
}

void unmake_move(Position *p, Move m, const Undo *u){
  (void)m;
  init_zobrist();
  /* revert side */
  Color them = p->side; /* current side is side to move after move; previous us = them^1 */
  Color us = them^1;
  p->side = us;
  /* remove placed piece at to */
  int to=u->to, from=u->from;
  int placed = p->board[to];
  remove_piece(p,to,placed);
  /* restore captured */
  if(u->flags & FLAG_EP){
    int cap_sq = (us==COLOR_WHITE)? to-8 : to+8;
    int cap_piece = (us==COLOR_WHITE)? B_PAWN : W_PAWN;
    add_piece(p,cap_sq,cap_piece);
  } else if(u->captured!=EMPTY && !(u->flags&FLAG_PROMO?0:0)){
    add_piece(p,to,u->captured);
  }
  /* restore moving piece (pawn if promoted) */
  int pc;
  if(u->promoted!=0){
    pc = (us==COLOR_WHITE)? W_PAWN : B_PAWN;
  } else {
    /* original piece? use from occupancy unknown; infer from to? We have Undo captured etc. Need original piece. Store? Let's derive: if promoted !=0, pawn else map placed? 
       We didn't store original piece. Add to Undo? We'll store in captured? Hmm.
    */
    pc = placed;
  }
  add_piece(p,from,pc);

  /* castling rook undo */
  if(u->flags & FLAG_CASTLE){
    if(to==6){ remove_piece(p,5,W_ROOK); add_piece(p,7,W_ROOK); }
    else if(to==2){ remove_piece(p,3,W_ROOK); add_piece(p,0,W_ROOK); }
    else if(to==62){ remove_piece(p,61,B_ROOK); add_piece(p,63,B_ROOK); }
    else if(to==58){ remove_piece(p,59,B_ROOK); add_piece(p,56,B_ROOK); }
  }

  /* restore state */
  p->castling = u->castling;
  p->ep = u->ep;
  p->halfmove = u->halfmove;
  if(us==COLOR_BLACK) p->fullmove--; /* best effort */
  p->hash = u->hash;
  /* ensure side hash included in u->hash already */
}

/* attacks */
bool is_square_attacked(const Position *p, int sq, Color by){
  if(sq<0||sq>63) return false;
  /* pawn attacks */
  Bitboard pawns = p->bb[by==COLOR_WHITE? W_PAWN : B_PAWN];
  /* squares from which pawn attacks sq */
  /* reverse: pawn on sq-? */
  /* reuse attacks_pawn? attacks_pawn(sq,by) gives squares pawn on sq attacks; need attackers to sq. Instead check all pawns. */
  /* simple loop over 64? Use bitboard of pawns and check if their attack sets include sq */
  Bitboard bb = pawns;
  while(bb){
    int s=lsb_index(bb); bb &= bb-1;
    if(attacks_pawn(s,by) & (1ULL<<sq)) return true;
  }
  /* knights */
  Bitboard kn = p->bb[by==COLOR_WHITE? W_KNIGHT : B_KNIGHT];
  if(kn & attacks_knight(sq)) { /* attacks_knight(sq) gives squares knight on sq attacks, same symmetric */ return true; }
  /* Actually need reverse; knight attacks symmetric, so yes */
  /* king */
  Bitboard ki = p->bb[by==COLOR_WHITE? W_KING : B_KING];
  if(ki & attacks_king(sq)) return true;
  /* bishops/queens */
  /* slide from sq */
  /* rooks/queens */
  const int rook_dirs[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
  const int bishop_dirs[4][2]={{1,1},{1,-1},{-1,1},{-1,-1}};
  /* for each direction, first piece */
  for(int d=0;d<4;d++){
    int r=RANK(sq), f=FILE(sq);
    while(true){
      r+=rook_dirs[d][0]; f+=rook_dirs[d][1];
      if(r<0||r>7||f<0||f>7) break;
      int s=r*8+f;
      int pc=p->board[s];
      if(pc==EMPTY) continue;
      if(piece_color(pc)==by && (piece_type(pc)==PT_ROOK || piece_type(pc)==PT_QUEEN)) return true;
      break;
    }
  }
  for(int d=0;d<4;d++){
    int r=RANK(sq), f=FILE(sq);
    while(true){
      r+=bishop_dirs[d][0]; f+=bishop_dirs[d][1];
      if(r<0||r>7||f<0||f>7) break;
      int s=r*8+f;
      int pc=p->board[s];
      if(pc==EMPTY) continue;
      if(piece_color(pc)==by && (piece_type(pc)==PT_BISHOP || piece_type(pc)==PT_QUEEN)) return true;
      break;
    }
  }
  return false;
}

int side_in_check(const Position *p, Color side){
  /* find king */
  int king_sq=-1;
  int king_pc = (side==COLOR_WHITE)? W_KING : B_KING;
  Bitboard kb=p->bb[king_pc];
  if(kb) king_sq=lsb_index(kb);
  if(king_sq<0) return 1; /* no king considered in check? */
  return is_square_attacked(p, king_sq, side^1);
}
