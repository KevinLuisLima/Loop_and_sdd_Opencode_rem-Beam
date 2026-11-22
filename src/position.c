#include "position.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int file_of(int sq){ return sq & 7; }
static int rank_of(int sq){ return sq >> 3; }
static int absi(int x){ return x<0? -x : x; }

static int piece_at(const Position *p, int sq){
    for(int c=0;c<2;c++) for(int t=0;t<6;t++){
        if(p->pieces[c][t] & (1ULL<<sq)) return c*6+t;
    }
    return -1;
}

static uint64_t zpiece(int piece, int sq){
    uint64_t x = (uint64_t)(piece+1)*0x9e3779b97f4a7c15ULL + (uint64_t)(sq+1)*0xc2b2ae3d27d4eb4fULL;
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111b9ULL;
    x ^= x >> 31;
    return x;
}
static uint64_t zcastling(int rights){
    uint64_t x = (uint64_t)rights*0x85ebca6bULL ^ 0xc2b2ae3d;
    x ^= x >> 16; return x;
}
static uint64_t zep(int sq){
    uint64_t x = (uint64_t)(sq+100)*0x9e3779b9ULL;
    x ^= x >> 16; return x;
}
static uint64_t zside(void){ return 0x8f2a55f3c2e36f1ULL; }

static void rebuild_occ(Position *p){
    for(int c=0;c<2;c++){
        p->occ[c]=0;
        for(int t=0;t<6;t++) p->occ[c]|=p->pieces[c][t];
    }
    p->occ_all = p->occ[0] | p->occ[1];
}

Position position_start(void){
    Position p;
    memset(&p,0,sizeof(p));
    position_from_fen(&p, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    return p;
}

int position_from_fen(Position *p, const char *fen){
    memset(p,0,sizeof(Position));
    p->ep_square=-1;
    p->castling=0;
    p->side=COLOR_WHITE;
    p->halfmove=0;
    p->fullmove=1;

    if(!fen) return -1;
    char board[512];
    strncpy(board, fen, sizeof(board)-1);
    board[sizeof(board)-1]='\0';
    char *save=NULL;
    char *tok = strtok_r(board, " ", &save);
    if(!tok) return -1;
    int rank=7;
    int file=0;
    for(const char *c=tok; *c; c++){
        if(*c=='/'){ rank--; file=0; if(rank<0) return -1; continue; }
        if(isdigit((unsigned char)*c)){
            int n=*c-'0';
            if(n<1 || n>8) return -1;
            file+=n;
            if(file>8) return -1;
            continue;
        }
        int color, type;
        switch(*c){
            case 'P': type=PIECE_PAWN; color=COLOR_WHITE; break;
            case 'N': type=PIECE_KNIGHT; color=COLOR_WHITE; break;
            case 'B': type=PIECE_BISHOP; color=COLOR_WHITE; break;
            case 'R': type=PIECE_ROOK; color=COLOR_WHITE; break;
            case 'Q': type=PIECE_QUEEN; color=COLOR_WHITE; break;
            case 'K': type=PIECE_KING; color=COLOR_WHITE; break;
            case 'p': type=PIECE_PAWN; color=COLOR_BLACK; break;
            case 'n': type=PIECE_KNIGHT; color=COLOR_BLACK; break;
            case 'b': type=PIECE_BISHOP; color=COLOR_BLACK; break;
            case 'r': type=PIECE_ROOK; color=COLOR_BLACK; break;
            case 'q': type=PIECE_QUEEN; color=COLOR_BLACK; break;
            case 'k': type=PIECE_KING; color=COLOR_BLACK; break;
            default: return -1;
        }
        if(rank<0 || file>7) return -1;
        int sq = rank*8+file;
        p->pieces[color][type] |= (1ULL<<sq);
        file++;
    }
    if(rank!=0 || file!=8) return -1;

    if(__builtin_popcountll(p->pieces[COLOR_WHITE][PIECE_KING])!=1) return -1;
    if(__builtin_popcountll(p->pieces[COLOR_BLACK][PIECE_KING])!=1) return -1;

    tok = strtok_r(NULL, " ", &save);
    if(!tok) return -1;
    if(tok[0]=='w') p->side=COLOR_WHITE;
    else if(tok[0]=='b') p->side=COLOR_BLACK;
    else return -1;

    tok = strtok_r(NULL, " ", &save);
    if(!tok) return -1;
    if(strcmp(tok,"-")!=0){
        for(const char *c=tok; *c; c++){
            switch(*c){
                case 'K': p->castling |= 1; break;
                case 'Q': p->castling |= 2; break;
                case 'k': p->castling |= 4; break;
                case 'q': p->castling |= 8; break;
                default: return -1;
            }
        }
    }

    tok = strtok_r(NULL, " ", &save);
    if(!tok) return -1;
    if(strcmp(tok,"-")!=0){
        if(strlen(tok)!=2) return -1;
        int f=tok[0]-'a';
        int r=tok[1]-'1';
        if(f<0||f>7||r<0||r>7) return -1;
        p->ep_square = r*8+f;
    }

    tok = strtok_r(NULL, " ", &save);
    if(tok){
        char *end=NULL;
        long v=strtol(tok,&end,10);
        if(*end!='\0' || v<0) return -1;
        p->halfmove=(int)v;
    }
    tok = strtok_r(NULL, " ", &save);
    if(tok){
        char *end=NULL;
        long v=strtol(tok,&end,10);
        if(*end!='\0' || v<1) return -1;
        p->fullmove=(int)v;
    }

    rebuild_occ(p);
    uint64_t h=0;
    for(int c=0;c<2;c++) for(int t=0;t<6;t++){
        Bitboard bb=p->pieces[c][t];
        while(bb){ int sq=__builtin_ctzll(bb); bb&=bb-1; h ^= zpiece(c*6+t,sq); }
    }
    h ^= zcastling(p->castling);
    if(p->ep_square!=-1) h ^= zep(p->ep_square);
    if(p->side==COLOR_BLACK) h ^= zside();
    p->hash=h;
    return 0;
}

int position_to_fen(const Position *p, char *buf, int n){
    char *out=buf;
    int written=0;
    for(int rank=7; rank>=0; rank--){
        int empty=0;
        for(int file=0; file<8; file++){
            int sq=rank*8+file;
            int piece=piece_at(p,sq);
            if(piece==-1){ empty++; continue; }
            if(empty){
                char t[8]; t[0]=(char)('0'+empty); t[1]='\0';
                int l=strlen(t);
                if(written+l+1>=n) return -1;
                strcpy(out,t); out+=l; written+=l; empty=0;
            }
            char ch;
            switch(piece%6){
                case PIECE_PAWN: ch='p'; break;
                case PIECE_KNIGHT: ch='n'; break;
                case PIECE_BISHOP: ch='b'; break;
                case PIECE_ROOK: ch='r'; break;
                case PIECE_QUEEN: ch='q'; break;
                default: ch='k'; break;
            }
            if(piece<6) ch=toupper((unsigned char)ch);
            if(written+1>=n) return -1;
            *out++=ch; *out='\0'; written++;
        }
        if(empty){
            char t[8]; t[0]=(char)('0'+empty); t[1]='\0';
            int l=strlen(t);
            if(written+l+1>=n) return -1;
            strcpy(out,t); out+=l; written+=l;
        }
        if(rank>0){
            if(written+1>=n) return -1;
            *out++='/'; *out='\0'; written++;
        }
    }
    int l = snprintf(out, n-written, " %c ", p->side==COLOR_WHITE?'w':'b');
    out+=l; written+=l;
    char cbuf[8]; int ci=0;
    if(p->castling & 1) cbuf[ci++]='K';
    if(p->castling & 2) cbuf[ci++]='Q';
    if(p->castling & 4) cbuf[ci++]='k';
    if(p->castling & 8) cbuf[ci++]='q';
    if(ci==0){
        if(written+3>=n) return -1;
        *out++='-'; *out++=' '; *out='\0'; written+=2;
    } else {
        if(written+ci+2>=n) return -1;
        memcpy(out,cbuf,ci); out+=ci; written+=ci;
        *out++=' '; *out='\0'; written++;
    }
    if(p->ep_square==-1){
        if(written+3>=n) return -1;
        *out++='-'; *out++=' '; *out='\0'; written+=2;
    } else {
        char e[16]; snprintf(e,sizeof(e),"%c%d", 'a'+(p->ep_square%8), (p->ep_square/8)+1);
        int el=strlen(e);
        if(written+el+2>=n) return -1;
        strcpy(out,e); out+=el; written+=el;
        *out++=' '; *out='\0'; written++;
    }
    snprintf(out, n-written, "%d %d", p->halfmove, p->fullmove);
    return 0;
}

void position_reset(Position *p){ *p=position_start(); }

int square_attacked(const Position *p, int sq, Color attacker){
    /* pawns */
    if(attacker==COLOR_WHITE){
        if(rank_of(sq)>0){
            if(file_of(sq)>0 && (p->pieces[COLOR_WHITE][PIECE_PAWN]&(1ULL<<(sq-9)))) return 1;
            if(file_of(sq)<7 && (p->pieces[COLOR_WHITE][PIECE_PAWN]&(1ULL<<(sq-7)))) return 1;
        }
    } else {
        if(rank_of(sq)<7){
            if(file_of(sq)>0 && (p->pieces[COLOR_BLACK][PIECE_PAWN]&(1ULL<<(sq+7)))) return 1;
            if(file_of(sq)<7 && (p->pieces[COLOR_BLACK][PIECE_PAWN]&(1ULL<<(sq+9)))) return 1;
        }
    }
    /* knights */
    static const int kn[8]={-17,-15,-10,-6,6,10,15,17};
    for(int i=0;i<8;i++){
        int s=sq+kn[i];
        if(s<0||s>63) continue;
        if(absi(file_of(s)-file_of(sq))>2) continue;
        if(p->pieces[attacker][PIECE_KNIGHT]&(1ULL<<s)) return 1;
    }
    /* king */
    static const int kg[8]={-9,-8,-7,-1,1,7,8,9};
    for(int i=0;i<8;i++){
        int s=sq+kg[i];
        if(s<0||s>63) continue;
        if(absi(file_of(s)-file_of(sq))>1) continue;
        if(p->pieces[attacker][PIECE_KING]&(1ULL<<s)) return 1;
    }
    /* bishops/queens */
    int bdirs[4][2]={{-1,-1},{-1,1},{1,-1},{1,1}};
    for(int di=0;di<4;di++){
        int f=file_of(sq), r=rank_of(sq);
        while(1){
            f+=bdirs[di][0]; r+=bdirs[di][1];
            if(f<0||f>7||r<0||r>7) break;
            int s=r*8+f;
            if(p->pieces[attacker][PIECE_BISHOP]&(1ULL<<s) || p->pieces[attacker][PIECE_QUEEN]&(1ULL<<s)) return 1;
            if(p->occ_all & (1ULL<<s)) break;
        }
    }
    /* rooks/queens */
    int rdirs[4][2]={{-1,0},{1,0},{0,-1},{0,1}};
    for(int di=0;di<4;di++){
        int f=file_of(sq), r=rank_of(sq);
        while(1){
            f+=rdirs[di][0]; r+=rdirs[di][1];
            if(f<0||f>7||r<0||r>7) break;
            int s=r*8+f;
            if(p->pieces[attacker][PIECE_ROOK]&(1ULL<<s) || p->pieces[attacker][PIECE_QUEEN]&(1ULL<<s)) return 1;
            if(p->occ_all & (1ULL<<s)) break;
        }
    }
    return 0;
}

int in_check(const Position *p, Color side){
    int king_sq=-1;
    Bitboard kbb=p->pieces[side][PIECE_KING];
    if(kbb) king_sq=__builtin_ctzll(kbb);
    if(king_sq<0) return 1;
    return square_attacked(p, king_sq, side^1);
}

/* move generation */
static void add_move(MoveList *ml, int from, int to, int promo){
    if(ml->count>=MAX_MOVES) return;
    Move *m=&ml->moves[ml->count++];
    m->from=from; m->to=to; m->promo=promo; m->score=0;
}

static void gen_pawn_moves(const Position *p, MoveList *ml, int captures_only){
    Color c=p->side;
    int dir = (c==COLOR_WHITE)? 8 : -8;
    int start_rank = (c==COLOR_WHITE)? 1 : 6;
    int promo_rank = (c==COLOR_WHITE)? 7 : 0;
    Bitboard pawns=p->pieces[c][PIECE_PAWN];
    Bitboard occupied=p->occ_all;
    Bitboard enemy_occ = p->occ[c^1];
    Bitboard bb=pawns;
    while(bb){
        int from=__builtin_ctzll(bb); bb&=bb-1;
        if(!captures_only){
            int to=from+dir;
            if(to>=0 && to<64 && !(occupied&(1ULL<<to))){
                if(rank_of(to)==promo_rank){
                    for(int pr=PIECE_KNIGHT; pr<=PIECE_QUEEN; pr++) add_move(ml,from,to,pr);
                } else {
                    add_move(ml,from,to,PIECE_NONE);
                    if(rank_of(from)==start_rank){
                        int to2=from+2*dir;
                        if(to2>=0 && to2<64 && !(occupied&(1ULL<<to2))) add_move(ml,from,to2,PIECE_NONE);
                    }
                }
            }
        }
    }
    bb=pawns;
    while(bb){
        int from=__builtin_ctzll(bb); bb&=bb-1;
        int f=file_of(from), r=rank_of(from);
        int targets[2]; int nt=0;
        if(c==COLOR_WHITE){
            if(f>0 && r<7) targets[nt++]=from+7;
            if(f<7 && r<7) targets[nt++]=from+9;
        } else {
            if(f>0 && r>0) targets[nt++]=from-9;
            if(f<7 && r>0) targets[nt++]=from-7;
        }
        for(int i=0;i<nt;i++){
            int to=targets[i];
            int is_ep = (p->ep_square==to);
            if(is_ep || (enemy_occ&(1ULL<<to))){
                if(rank_of(to)==promo_rank){
                    for(int pr=PIECE_KNIGHT; pr<=PIECE_QUEEN; pr++) add_move(ml,from,to,pr);
                } else {
                    add_move(ml,from,to,PIECE_NONE);
                }
            }
        }
    }
    (void)pawns;
}

static void gen_knight_moves(const Position *p, MoveList *ml, int captures_only){
    Color c=p->side;
    Bitboard bb=p->pieces[c][PIECE_KNIGHT];
    Bitboard own=p->occ[c];
    static const int offsets[8]={-17,-15,-10,-6,6,10,15,17};
    while(bb){
        int from=__builtin_ctzll(bb); bb&=bb-1;
        for(int i=0;i<8;i++){
            int to=from+offsets[i];
            if(to<0||to>63) continue;
            if(absi(file_of(to)-file_of(from))>2) continue;
            if(own&(1ULL<<to)) continue;
            if(captures_only && !(p->occ[c^1]&(1ULL<<to))) continue;
            add_move(ml,from,to,PIECE_NONE);
        }
    }
}
static void gen_king_moves(const Position *p, MoveList *ml, int captures_only){
    Color c=p->side;
    Bitboard bb=p->pieces[c][PIECE_KING];
    if(!bb) return;
    int from=__builtin_ctzll(bb);
    Bitboard own=p->occ[c];
    static const int offsets[8]={-9,-8,-7,-1,1,7,8,9};
    for(int i=0;i<8;i++){
        int to=from+offsets[i];
        if(to<0||to>63) continue;
        if(absi(file_of(to)-file_of(from))>1) continue;
        if(own&(1ULL<<to)) continue;
        if(captures_only && !(p->occ[c^1]&(1ULL<<to))) continue;
        add_move(ml,from,to,PIECE_NONE);
    }
    if(captures_only) return;
    if(!in_check(p,c)){
        int r=(c==COLOR_WHITE)?0:7;
        int ks=r*8+4;
        if(from==ks){
            /* kingside */
            int rights=(c==COLOR_WHITE)?(p->castling&1):(p->castling&4);
            if(rights){
                int f1=r*8+5, f2=r*8+6;
                if(!(p->occ_all&(1ULL<<f1)) && !(p->occ_all&(1ULL<<f2))){
                    if(!square_attacked(p,f1,c^1) && !square_attacked(p,f2,c^1)) add_move(ml,ks,r*8+6,PIECE_NONE);
                }
            }
            /* queenside */
            int qrights=(c==COLOR_WHITE)?(p->castling&2):(p->castling&8);
            if(qrights){
                int b1=r*8+3,b2=r*8+2,b3=r*8+1;
                if(!(p->occ_all&(1ULL<<b1)) && !(p->occ_all&(1ULL<<b2)) && !(p->occ_all&(1ULL<<b3))){
                    if(!square_attacked(p,b1,c^1) && !square_attacked(p,b2,c^1)) add_move(ml,ks,r*8+2,PIECE_NONE);
                }
            }
        }
    }
}

static void gen_sliding_moves(const Position *p, MoveList *ml, int captures_only, int bishop, int rook){
    Color c=p->side;
    Bitboard own=p->occ[c];
    Bitboard enemy=p->occ[c^1];
    /* bishops */
    /* pieces handled by type loop below */
    /* We'll iterate all piece types */
    for(int t=0;t<6;t++){
        int isB=(t==PIECE_BISHOP), isR=(t==PIECE_ROOK), isQ=(t==PIECE_QUEEN);
        int doB = bishop && (isB||isQ);
        int doR = rook && (isR||isQ);
        if(!doB && !doR) continue;
        Bitboard bb=p->pieces[c][t];
        while(bb){
            int from=__builtin_ctzll(bb); bb&=bb-1;
            /* directions */
            int dirs[8][2]; int nd=0;
            if(doB){ int b[4][2]={{-1,-1},{-1,1},{1,-1},{1,1}}; for(int i=0;i<4;i++){dirs[nd][0]=b[i][0];dirs[nd][1]=b[i][1];nd++;} }
            if(doR){ int r[4][2]={{-1,0},{1,0},{0,-1},{0,1}}; for(int i=0;i<4;i++){dirs[nd][0]=r[i][0];dirs[nd][1]=r[i][1];nd++;} }
            /* if both bishop&rook requested for same type queens, we'd double dirs; but doB/doR both true only for Q, directions combine 8 unique (no dup) since sets disjoint. OK */
            for(int di=0;di<nd;di++){
                int f=file_of(from), r=rank_of(from);
                while(1){
                    f+=dirs[di][0]; r+=dirs[di][1];
                    if(f<0||f>7||r<0||r>7) break;
                    int to=r*8+f;
                    if(own&(1ULL<<to)) break;
                    int iscap = (enemy&(1ULL<<to)) ? 1 : 0;
                    if(captures_only){
                        if(iscap) add_move(ml,from,to,PIECE_NONE);
                    } else {
                        add_move(ml,from,to,PIECE_NONE);
                    }
                    if(iscap) break;
                }
            }
        }
    }
}

int movegen_pseudo(const Position *p, MoveList *ml, int captures_only){
    ml->count=0;
    gen_pawn_moves(p,ml,captures_only);
    gen_knight_moves(p,ml,captures_only);
    gen_king_moves(p,ml,captures_only);
    gen_sliding_moves(p,ml,captures_only,1,1);
    return ml->count;
}

int movegen_legal(const Position *p, MoveList *ml, int captures_only){
    MoveList pseudo;
    movegen_pseudo(p,&pseudo,captures_only);
    ml->count=0;
    for(int i=0;i<pseudo.count;i++){
        Position tmp=*p;
        Undo u;
        make_move(&tmp,&pseudo.moves[i],&u);
        if(in_check(&tmp, p->side)) continue;
        unmake_move(&tmp,&pseudo.moves[i],&u);
        if(ml->count<MAX_MOVES) ml->moves[ml->count++]=pseudo.moves[i];
    }
    return ml->count;
}

int make_move(Position *p, const Move *m, Undo *u){
    u->from=m->from; u->to=m->to; u->promo=m->promo;
    u->prev_castling=p->castling;
    u->prev_ep=p->ep_square;
    u->prev_halfmove=p->halfmove;
    u->prev_side=p->side;
    u->prev_fullmove=p->fullmove;
    u->prev_hash=p->hash;
    int us=p->side;
    int moving=piece_at(p,m->from);
    u->moving_piece=moving;
    if(moving<0) return -1;
    int mt=moving%6;
    int captured=-1;
    int cap_sq=m->to;
    int is_ep=0,is_castle=0;
    if(mt==PIECE_PAWN && p->ep_square==m->to){
        is_ep=1;
        cap_sq = (us==COLOR_WHITE)? m->to-8 : m->to+8;
        captured = (us==COLOR_WHITE)? (6+PIECE_PAWN) : PIECE_PAWN;
    } else {
        captured = piece_at(p,m->to);
    }
    if(mt==PIECE_KING && absi(file_of(m->from)-file_of(m->to))==2) is_castle=1;
    u->captured_piece=captured;
    u->capture_square=cap_sq;
    u->is_castle=is_castle;
    u->is_ep=is_ep;

    uint64_t h=p->hash;
    if(p->ep_square!=-1) h ^= zep(p->ep_square);
    h ^= zcastling(p->castling);

    h ^= zpiece(moving,m->from);
    p->pieces[us][mt] &= ~(1ULL<<m->from);

    if(captured!=-1){
        int cc = (captured>=6)?1:0;
        int ct = captured%6;
        p->pieces[cc][ct] &= ~(1ULL<<cap_sq);
        h ^= zpiece(captured,cap_sq);
        int newc=p->castling;
        if(ct==PIECE_ROOK){
            if(cap_sq==0) newc &= ~2;
            if(cap_sq==7) newc &= ~1;
            if(cap_sq==56) newc &= ~8;
            if(cap_sq==63) newc &= ~4;
        }
        p->castling=newc;
    }

    int place_type = (m->promo!=PIECE_NONE)? m->promo : mt;
    int place_piece = us*6 + place_type;
    p->pieces[us][place_type] |= (1ULL<<m->to);
    h ^= zpiece(place_piece,m->to);

    if(is_castle){
        int rook_from,rook_to;
        if(m->to == (us==COLOR_WHITE?6:62)){ rook_from=(us==COLOR_WHITE?7:63); rook_to=(us==COLOR_WHITE?5:61); }
        else { rook_from=(us==COLOR_WHITE?0:56); rook_to=(us==COLOR_WHITE?3:59); }
        int rcode=us*6+PIECE_ROOK;
        p->pieces[us][PIECE_ROOK] &= ~(1ULL<<rook_from);
        p->pieces[us][PIECE_ROOK] |= (1ULL<<rook_to);
        h ^= zpiece(rcode,rook_from);
        h ^= zpiece(rcode,rook_to);
    }

    int new_castling = p->castling;
    if(mt==PIECE_KING){
        if(us==COLOR_WHITE) new_castling &= ~3;
        else new_castling &= ~12;
    }
    if(mt==PIECE_ROOK){
        if(m->from==0) new_castling &= ~2;
        if(m->from==7) new_castling &= ~1;
        if(m->from==56) new_castling &= ~8;
        if(m->from==63) new_castling &= ~4;
    }
    int new_ep=-1;
    if(mt==PIECE_PAWN && absi(rank_of(m->from)-rank_of(m->to))==2){
        new_ep = (m->from+m->to)/2;
    }
    if(new_ep!=-1) h ^= zep(new_ep);
    h ^= zcastling(new_castling);
    p->castling=new_castling;
    p->ep_square=new_ep;

    if(mt==PIECE_PAWN || captured!=-1) p->halfmove=0; else p->halfmove++;
    if(p->side==COLOR_BLACK) p->fullmove++;
    p->side ^= 1;
    h ^= zside();
    rebuild_occ(p);
    p->hash=h;
    return 0;
}

void unmake_move(Position *p, const Move *m, const Undo *u){
    p->side = u->prev_side;
    int us = u->prev_side;
    int place_type = (m->promo!=PIECE_NONE)? m->promo : (u->moving_piece%6);
    p->pieces[us][place_type] &= ~(1ULL<<m->to);
    int mt = u->moving_piece %6;
    p->pieces[us][mt] |= (1ULL<<m->from);
    if(u->captured_piece!=-1){
        int cc = (u->captured_piece>=6)?1:0;
        int ct = u->captured_piece%6;
        p->pieces[cc][ct] |= (1ULL<<u->capture_square);
    }
    if(u->is_castle){
        int rook_from, rook_to;
        if(m->to == (us==COLOR_WHITE?6:62)){ rook_from=(us==COLOR_WHITE?7:63); rook_to=(us==COLOR_WHITE?5:61); }
        else { rook_from=(us==COLOR_WHITE?0:56); rook_to=(us==COLOR_WHITE?3:59); }
        p->pieces[us][PIECE_ROOK] &= ~(1ULL<<rook_to);
        p->pieces[us][PIECE_ROOK] |= (1ULL<<rook_from);
    }
    p->castling=u->prev_castling;
    p->ep_square=u->prev_ep;
    p->halfmove=u->prev_halfmove;
    p->fullmove=u->prev_fullmove;
    p->hash=u->prev_hash;
    rebuild_occ(p);
}

int is_terminal(const Position *p){
    MoveList ml;
    movegen_legal(p,&ml,0);
    if(ml.count==0) return 1;
    if(p->halfmove>=100) return 1;
    return 0;
}

int has_insufficient_material(const Position *p){
    int pieces=0;
    int pawns = __builtin_popcountll(p->pieces[0][PIECE_PAWN])+__builtin_popcountll(p->pieces[1][PIECE_PAWN]);
    for(int c=0;c<2;c++) for(int t=0;t<6;t++){
        if(t==PIECE_PAWN || t==PIECE_KING) continue;
        pieces += __builtin_popcountll(p->pieces[c][t]);
    }
    if(pieces==0 && pawns==0) return 1;
    if(pieces<=1 && pawns==0) return 1;
    return 0;
}

int is_repetition(const Position *p, const Position *history[], int hist_count){
    for(int i=0;i<hist_count;i++){
        if(history[i]->hash==p->hash) return 1;
    }
    return 0;
}

uint64_t perft_position(Position *p, int depth){
    if(depth==0) return 1;
    MoveList ml;
    movegen_legal(p,&ml,0);
    if(depth==1) return (uint64_t)ml.count;
    uint64_t nodes=0;
    for(int i=0;i<ml.count;i++){
        Position tmp=*p;
        Undo u;
        make_move(&tmp,&ml.moves[i],&u);
        nodes += perft_position(&tmp,depth-1);
        unmake_move(&tmp,&ml.moves[i],&u);
    }
    return nodes;
}
