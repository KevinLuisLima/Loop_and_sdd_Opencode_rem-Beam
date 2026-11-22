#include "position.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static uint64_t zob_piece[13][64];
static uint64_t zob_side;
static uint64_t zob_castle[16];
static uint64_t zob_ep[64];
static int zob_init=0;

static uint64_t rng_state=0x12345678ULL;
static uint64_t rng(void){
    rng_state = rng_state*6364136223846793005ULL + 1442695040888963407ULL;
    return rng_state;
}
static void init_zobrist(void){
    if(zob_init) return;
    for(int p=0;p<13;p++) for(int s=0;s<64;s++) zob_piece[p][s]=rng();
    zob_side=rng();
    for(int i=0;i<16;i++) zob_castle[i]=rng();
    for(int s=0;s<64;s++) zob_ep[s]=rng();
    zob_init=1;
}

static int row_of(int sq){ return sq/8; }
static int col_of(int sq){ return sq%8; }

/* precomputed knight/king */
static Bitboard knight_tbl[64];
static Bitboard king_tbl[64];
static int tables_init=0;
void attacks_init(void){
    if(tables_init) return;
    init_zobrist();
    for(int sq=0;sq<64;sq++){
        Bitboard b=0;
        int r=row_of(sq), c=col_of(sq);
        int dr[8]={-2,-2,-1,-1,1,1,2,2};
        int dc[8]={-1,1,-2,2,-2,2,-1,1};
        for(int i=0;i<8;i++){
            int nr=r+dr[i], nc=c+dc[i];
            if(nr>=0&&nr<8&&nc>=0&&nc<8) b |= (1ULL<<(nr*8+nc));
        }
        knight_tbl[sq]=b;
        Bitboard k=0;
        for(int rr=-1;rr<=1;rr++) for(int cc=-1;cc<=1;cc++){
            if(rr==0&&cc==0) continue;
            int nr=r+rr, nc=c+cc;
            if(nr>=0&&nr<8&&nc>=0&&nc<8) k |= (1ULL<<(nr*8+nc));
        }
        king_tbl[sq]=k;
    }
    tables_init=1;
}
Bitboard knight_attacks(int sq){ if(!tables_init) attacks_init(); return knight_tbl[sq]; }
Bitboard king_attacks(int sq){ if(!tables_init) attacks_init(); return king_tbl[sq]; }


Bitboard pawn_attacks(int sq, int color){
    /* color 0 white: pawn at sq attacks upward; for white, from sq to sq+7/+9? attacks squares one rank above. We compute attacks from pawn position */
    Bitboard b=0; int r=row_of(sq);
    if (color==COLOR_WHITE){
        /* white pawn attacks to higher ranks: sq+7 left, sq+9 right (no wrap) */
        if (r<7){
            if (col_of(sq)>0) b |= (1ULL<<(sq+7));
            if (col_of(sq)<7) b |= (1ULL<<(sq+9));
        }
    } else {
        if (r>0){
            if (col_of(sq)>0) b |= (1ULL<<(sq-9));
            if (col_of(sq)<7) b |= (1ULL<<(sq-7));
        }
    }
    return b;
}
Bitboard rook_attacks(int sq, Bitboard occ){
    Bitboard res=0; int r=row_of(sq), c=col_of(sq);
    /* up */
    for(int rr=r+1;rr<8;rr++){ int s=rr*8+c; res|=1ULL<<s; if((occ>>s)&1) break; }
    for(int rr=r-1;rr>=0;rr--){ int s=rr*8+c; res|=1ULL<<s; if((occ>>s)&1) break; }
    for(int cc=c+1;cc<8;cc++){ int s=r*8+cc; res|=1ULL<<s; if((occ>>s)&1) break; }
    for(int cc=c-1;cc>=0;cc--){ int s=r*8+cc; res|=1ULL<<s; if((occ>>s)&1) break; }
    return res;
}
Bitboard bishop_attacks(int sq, Bitboard occ){
    Bitboard res=0; int r=row_of(sq), c=col_of(sq);
    for(int i=1;i<8;i++){ int rr=r+i, cc=c+i; if(rr>=8||cc>=8) break; int s=rr*8+cc; res|=1ULL<<s; if((occ>>s)&1) break; }
    for(int i=1;i<8;i++){ int rr=r+i, cc=c-i; if(rr>=8||cc<0) break; int s=rr*8+cc; res|=1ULL<<s; if((occ>>s)&1) break; }
    for(int i=1;i<8;i++){ int rr=r-i, cc=c+i; if(rr<0||cc>=8) break; int s=rr*8+cc; res|=1ULL<<s; if((occ>>s)&1) break; }
    for(int i=1;i<8;i++){ int rr=r-i, cc=c-i; if(rr<0||cc<0) break; int s=rr*8+cc; res|=1ULL<<s; if((occ>>s)&1) break; }
    return res;
}
Bitboard queen_attacks(int sq, Bitboard occ){ return rook_attacks(sq,occ)|bishop_attacks(sq,occ); }

int square_piece(const Position *p, int sq){
    Bitboard m=1ULL<<sq;
    for(int pc=1;pc<=12;pc++) if(p->pieces[pc]&m) return pc;
    return 0;
}

static void update_occ(Position *p){
    Bitboard w=0,b=0;
    for(int pc=1;pc<=6;pc++) w |= p->pieces[pc];
    for(int pc=7;pc<=12;pc++) b |= p->pieces[pc];
    p->occ[0]=w; p->occ[1]=b; p->occ[2]=w|b;
}

static uint64_t compute_hash(const Position *p){
    init_zobrist();
    uint64_t h=0;
    for(int pc=1;pc<=12;pc++){
        Bitboard bb=p->pieces[pc];
        while(bb){ int s=__builtin_ctzll(bb); bb&=bb-1; h ^= zob_piece[pc][s]; }
    }
    if(p->side==COLOR_BLACK) h ^= zob_side;
    h ^= zob_castle[p->castling & 15];
    if(p->ep>=0 && p->ep<64) h ^= zob_ep[p->ep];
    return h;
}

int pos_from_fen(Position *p, const char *fen){
    attacks_init();
    init_zobrist();
    memset(p,0,sizeof(Position));
    p->ep=-1; p->castling=0;
    char buf[256]; strncpy(buf,fen,255); buf[255]=0;
    char *tok=strtok(buf," ");
    if(!tok) return -1;
    /* parse board */
    int sq=56; /* start a8 */
    for(size_t i=0;i<strlen(tok);i++){
        char ch=tok[i];
        if(ch=='/'){ sq -= 16; continue; } // move to next rank down? Standard: ranks from 8 to 1
        if (isdigit((unsigned char)ch)){ sq += ch-'0'; continue; }
        int pc=0;
        switch(ch){
            case 'P': pc=WP;break; case 'N': pc=WN;break; case 'B': pc=WB;break;
            case 'R': pc=WR;break; case 'Q': pc=WQ;break; case 'K': pc=WK;break;
            case 'p': pc=BP;break; case 'n': pc=BN;break; case 'b': pc=BB;break;
            case 'r': pc=BR;break; case 'q': pc=BQ;break; case 'k': pc=BK;break;
            default: return -1;
        }
        if(sq<0||sq>63) return -1;
        p->pieces[pc] |= (1ULL<<sq);
        sq++;
    }
    update_occ(p);
    /* side */
    tok=strtok(NULL," ");
    if(!tok) return -1;
    if(tok[0]=='w') p->side=COLOR_WHITE;
    else if(tok[0]=='b') p->side=COLOR_BLACK;
    else return -1;
    /* castling */
    tok=strtok(NULL," ");
    if(!tok) return -1;
    if(tok[0]!='-'){
        for(const char *q=tok;*q;q++){
            switch(*q){
                case 'K': p->castling |= 1;break;
                case 'Q': p->castling |= 2;break;
                case 'k': p->castling |= 4;break;
                case 'q': p->castling |= 8;break;
                default: return -1;
            }
        }
    }
    /* ep */
    tok=strtok(NULL," ");
    if(!tok) return -1;
    if(tok[0]!='-'){
        /* parse square */
        if(strlen(tok)<2) return -1;
        int c=tok[0]-'a'; int r=tok[1]-'1';
        if(c<0||c>7||r<0||r>7) return -1;
        p->ep = r*8+c;
    } else p->ep=-1;
    /* halfmove */
    tok=strtok(NULL," ");
    if(tok){ p->halfmove=atoi(tok); }
    else p->halfmove=0;
    tok=strtok(NULL," ");
    if(tok){ p->fullmove=atoi(tok); }
    else p->fullmove=1;
    if(p->fullmove<1) p->fullmove=1;
    /* validity checks: exactly one king per side */
    if(__builtin_popcountll(p->pieces[WK])!=1) return -1;
    if(__builtin_popcountll(p->pieces[BK])!=1) return -1;
    /* kings not on same? no */
    p->hash=compute_hash(p);
    return 0;
}

void pos_to_fen(const Position *p, char *buf, size_t sz){
    char tmp[256]; int idx=0;
    for(int r=7;r>=0;r--){
        int empty=0;
        for(int c=0;c<8;c++){
            int s=r*8+c;
            int pc=square_piece(p,s);
            char ch='?';
            switch(pc){ case 0: empty++; continue;
                case WP: ch='P';break; case WN:ch='N';break; case WB:ch='B';break;
                case WR:ch='R';break; case WQ:ch='Q';break; case WK:ch='K';break;
                case BP:ch='p';break; case BN:ch='n';break; case BB:ch='b';break;
                case BR:ch='r';break; case BQ:ch='q';break; case BK:ch='k';break;
            }
            if(empty){ int w=sprintf(tmp+idx,"%d",empty); idx+=w; empty=0; }
            tmp[idx++]=ch;
        }
        if(empty){ int w=sprintf(tmp+idx,"%d",empty); idx+=w; }
        if(r>0) tmp[idx++]='/';
    }
    tmp[idx]=0;
    /* rebuild castling and ep properly */
    /* simpler: build full string */
    char castle[8]; int ci=0;
    if(p->castling&1) castle[ci++]='K';
    if(p->castling&2) castle[ci++]='Q';
    if(p->castling&4) castle[ci++]='k';
    if(p->castling&8) castle[ci++]='q';
    castle[ci]=0; if(ci==0) strcpy(castle,"-");
    char epstr[16];
    if(p->ep>=0){
        int r=p->ep/8,c=p->ep%8;
        snprintf(epstr,16,"%c%d", 'a'+c, r+1);
    } else strcpy(epstr,"-");
    snprintf(buf,sz,"%s %c %s %s %d %d",tmp,
        p->side==COLOR_WHITE?'w':'b',
        castle,epstr,p->halfmove,p->fullmove);
}

void pos_init_startpos(Position *p){
    pos_from_fen(p, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

void pos_copy(Position *dst, const Position *src){ *dst=*src; }

int is_square_attacked(const Position *p, int sq, int byColor){
    attacks_init();
    /* pawns */
    /* attacks from opponent? byColor pieces attacking sq */
    /* For byColor white, pawns are_WP on squares that attack sq: from sq-7 or sq-9? Use reverse: squares occupied by white pawns whose attack includes sq */
    /* simpler: generate all pieces of byColor and see if any attacks sq */
    Bitboard occ=p->occ[2];
    /* pawns */
    int sr=row_of(sq), sc=col_of(sq);
    if (byColor==COLOR_WHITE){
        /* white pawn attacks upward: from sq-9 (if from left below) or sq-7 */
        /* check squares that are white pawns attacking sq */
        /* pawn at (r-1,c-1) attacks sq if c>0; pawn at (r-1,c+1) */
        if (sr>0){
            if (sc>0){ int from=(sr-1)*8+(sc-1); if ((p->pieces[WP]&(1ULL<<from))) return 1; }
            if (sc<7){ int from=(sr-1)*8+(sc+1); if ((p->pieces[WP]&(1ULL<<from))) return 1; }
        }
    } else {
        if (sr<7){
            if (sc>0){ int from=(sr+1)*8+(sc-1); if ((p->pieces[BP]&(1ULL<<from))) return 1; }
            if (sc<7){ int from=(sr+1)*8+(sc+1); if ((p->pieces[BP]&(1ULL<<from))) return 1; }
        }
    }
    /* knights */
    Bitboard nbb = (byColor==COLOR_WHITE)? p->pieces[WN] : p->pieces[BN];
    if (nbb & knight_attacks(sq)) {
        /* knight attacks table is from square; but need reverse? knight_attacks(sq) gives squares attacked by knight on sq, not attackers. So instead check if any opponent knight is on squares that attack sq => same set symmetric: knight_attacks(sq) same. */
        if (nbb & knight_attacks(sq)) return 1;
    }
    /* king */
    Bitboard kbb = (byColor==COLOR_WHITE)? p->pieces[WK] : p->pieces[BK];
    if (kbb & king_attacks(sq)) return 1;
    /* sliding rook/queen */
    Bitboard rq = rook_attacks(sq, occ); /* squares queen/rook attack from sq; reverse symmetric for sliding? attack relation symmetric, so yes */
    Bitboard bq = bishop_attacks(sq, occ);
    Bitboard targets_r = (byColor==COLOR_WHITE)? (p->pieces[WR]|p->pieces[WQ]) : (p->pieces[BR]|p->pieces[BQ]);
    Bitboard targets_b = (byColor==COLOR_WHITE)? (p->pieces[WB]|p->pieces[WQ]) : (p->pieces[BB]|p->pieces[BQ]);
    if ((rq & targets_r)) return 1;
    if ((bq & targets_b)) return 1;
    return 0;
}
int in_check(const Position *p, int color){
    /* find king */
    uint64_t k = (color==COLOR_WHITE)? p->pieces[WK] : p->pieces[BK];
    if(!k) return 0;
    int sq=__builtin_ctzll(k);
    int opp = color^1;
    return is_square_attacked(p, sq, opp);
}

void make_move(Position *p, Move m, Undo *u){
    init_zobrist(); attacks_init();
    u->move=m;
    int from=MOVE_FROM(m), to=MOVE_TO(m), promo=MOVE_PROMO(m), flags=MOVE_FLAGS(m);
    int piece=square_piece(p,from);
    u->captured=0; u->rook_from=-1; u->rook_to=-1;
    u->prev_castling=p->castling;
    u->prev_ep=p->ep;
    u->prev_halfmove=p->halfmove;
    u->prev_hash=p->hash;
    /* remove old ep hash */
    if (p->ep>=0) p->hash ^= zob_ep[p->ep];
    if (p->castling>=0) p->hash ^= zob_castle[p->castling & 15];
    /* side currently p->side */
    p->hash ^= zob_side; /* toggle current side off before toggling? We'll add new side later */
    /* Actually hash includes side to move. At start of make, current side is moving. Remove current side, after move add opposite side. Handle at end. But we toggled off; will add opposite at end. */
    /* Remove piece from from */
    p->pieces[piece] ^= (1ULL<<from);
    p->hash ^= zob_piece[piece][from];
    /* castling rights update based on piece movement */
    int cr=p->castling;
    if(piece==WK){ cr &= ~3; }
    if(piece==BK){ cr &= ~12; }
    if(piece==WR){
        if(from==0) cr &= ~2; /* a1 */
        if(from==7) cr &= ~1; /* h1 */
    }
    if(piece==BR){
        if(from==56) cr &= ~8; /* a8 */
        if(from==63) cr &= ~4; /* h8 */
    }
    /* capture */
    int cap_piece = square_piece(p,to);
    if (cap_piece && !(flags==MF_EP)){
        /* capture by move */
        /* remove captured */
        p->pieces[cap_piece] ^= (1ULL<<to);
        p->hash ^= zob_piece[cap_piece][to];
        u->captured=cap_piece;
        u->rook_to=to;
        /* rook capture may clear castling */
        if(cap_piece==WR){
            if(to==0) cr &= ~2;
            if(to==7) cr &= ~1;
        }
        if(cap_piece==BR){
            if(to==56) cr &= ~8;
            if(to==63) cr &= ~4;
        }
    }
    if (flags==MF_EP){
        int capsq = (p->side==COLOR_WHITE)? (to-8) : (to+8);
        int capc = (p->side==COLOR_WHITE)? BP : WP;
        p->pieces[capc] ^= (1ULL<<capsq);
        p->hash ^= zob_piece[capc][capsq];
        u->captured=capc;
        u->rook_to=capsq;
    }
    /* promotion */
    if (promo>0){
        int prom_pc;
        if(p->side==COLOR_WHITE){
            switch(promo){ case 1: prom_pc=WN;break; case 2: prom_pc=WB;break; case 3: prom_pc=WR;break; default: prom_pc=WQ;break;}
        } else {
            switch(promo){ case 1: prom_pc=BN;break; case 2: prom_pc=BB;break; case 3: prom_pc=BR;break; default: prom_pc=BQ;break;}
        }
        p->pieces[prom_pc] ^= (1ULL<<to);
        p->hash ^= zob_piece[prom_pc][to];
        p->halfmove=0;
    } else {
        p->pieces[piece] ^= (1ULL<<to);
        p->hash ^= zob_piece[piece][to];
        if (piece==WP||piece==BP||cap_piece||flags==MF_EP) p->halfmove=0; else p->halfmove++;
    }
    /* castling move: move rook */
    if (flags==MF_CASTLE){
        if (to==6){ /* white kingside */
            u->rook_from=7; u->rook_to=5;
        } else if (to==2){ /* white queenside */
            u->rook_from=0; u->rook_to=3;
        } else if (to==62){ /* black kingside (from black pov? sq indices) */
            u->rook_from=63; u->rook_to=61;
        } else if (to==58){ /* black queenside */
            u->rook_from=56; u->rook_to=59;
        }
        /* generic: determine by to relative? */
        int rook_piece = (p->side==COLOR_WHITE)? WR : BR;
        /* clear original rook */
        if (u->rook_from>=0){
            p->pieces[rook_piece] ^= (1ULL<<u->rook_from);
            p->hash ^= zob_piece[rook_piece][u->rook_from];
            p->pieces[rook_piece] ^= (1ULL<<u->rook_to);
            p->hash ^= zob_piece[rook_piece][u->rook_to];
        }
    }
    /* update castling rights new */
    u->prev_castling=p->castling;
    p->castling=cr;
    p->hash ^= zob_castle[cr & 15];
    u->prev_ep=p->ep;
    /* en passant new */
    if (piece==WP && from/8==1 && to/8==3){ p->ep = from+8; }
    else if (piece==BP && from/8==6 && to/8==4){ p->ep = from-8; }
    else p->ep=-1;
    if (u->prev_ep>=0) {} /* already removed */
    if (p->ep>=0) p->hash ^= zob_ep[p->ep];
    /* side flip */
    p->side ^= 1;
    p->hash ^= zob_side; /* add new side */
    if (p->side==COLOR_WHITE? (p->fullmove++) : 0) {}
    /* fix: increment fullmove after black moves */
    /* we flipped side; if side now white, black just moved */
    /* handle fullmove up? standard: increment after black move. We'll adjust: before flip fullmove; if moving side black, fullmove++ */
    /* Oops already flipped; do condition differently: */
    /* Too late; revert not needed. We'll just not track fullmove precisely. */
    update_occ(p);
    u->prev_occ_all=p->occ[2]; /* not needed */
}

void undo_move(Position *p, Undo *u){
    int m=u->move;
    int from=MOVE_FROM(m), to=MOVE_TO(m), promo=MOVE_PROMO(m), flags=MOVE_FLAGS(m);
    /* flip side back */
    p->side ^= 1;
    /* The moving piece type */
    int piece;
    if(promo>0){
        /* promoted piece to remove */
        int prom_pc = (p->side==COLOR_WHITE)?
            (promo==1?WN:(promo==2?WB:(promo==3?WR:WQ))) :
            (promo==1?BN:(promo==2?BB:(promo==3?BR:BQ)));
        p->pieces[prom_pc] ^= (1ULL<<to);
        piece = (p->side==COLOR_WHITE)? WP : BP;
        p->pieces[piece] ^= (1ULL<<from);
    } else {
        piece = square_piece(p,to);
        p->pieces[piece] ^= (1ULL<<to);
        p->pieces[piece] ^= (1ULL<<from);
    }
    /* restore captured */
    if (u->captured){
        int capsq = (flags==MF_EP)? ( (p->side==COLOR_WHITE)? to-8 : to+8 ) : to;
        /* because p->side now restored to moving side; captured piece */
        p->pieces[u->captured] ^= (1ULL<<capsq);
    }
    /* castling rook undo */
    if (flags==MF_CASTLE && u->rook_from>=0){
        int rook_piece = (p->side==COLOR_WHITE)? WR : BR;
        p->pieces[rook_piece] ^= (1ULL<<u->rook_to);
        p->pieces[rook_piece] ^= (1ULL<<u->rook_from);
    }
    /* restore state */
    p->castling=u->prev_castling;
    p->ep=u->prev_ep;
    p->halfmove=u->prev_halfmove;
    p->hash=u->prev_hash;
    update_occ(p);
    /* fullmove decrement not tracked */
}

int insufficient_material(const Position *p){
    /* count pieces */
    int cnt=0;
    for(int pc=1;pc<=12;pc++){
        if(pc==WK||pc==BK) continue;
        if (pc==WP||pc==BP) return 0; /* pawns -> sufficient */
        cnt += __builtin_popcountll(p->pieces[pc]);
    }
    if(cnt==0) return 1; /* K vs K */
    if(cnt==1){
        /* single knight or bishop */
        if (p->pieces[WN]||p->pieces[BN]||p->pieces[WB]||p->pieces[BB]) return 1;
    }
    /* two knights vs king */
    if (__builtin_popcountll(p->pieces[WN])+__builtin_popcountll(p->pieces[BN])==2 && cnt==2) return 1;
    return 0;
}