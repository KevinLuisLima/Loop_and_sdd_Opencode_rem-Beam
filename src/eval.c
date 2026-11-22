#include "eval.h"
static int file_of(int sq){ return sq & 7; }
static int rank_of(int sq){ return sq >> 3; }
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define IDX_MAT(p) (p)
#define IDX_PST(piece, sq) (10 + (piece)*64 + (sq))
#define IDX_MOB_KNIGHT (10+6*64)
#define IDX_MOB_BISHOP (IDX_MOB_KNIGHT+1)
#define IDX_MOB_ROOK (IDX_MOB_KNIGHT+2)
#define IDX_MOB_QUEEN (IDX_MOB_KNIGHT+3)
#define IDX_DOUBLED (IDX_MOB_KNIGHT+4)
#define IDX_ISOLATED (IDX_MOB_KNIGHT+5)
#define IDX_PASSED (IDX_MOB_KNIGHT+6)
#define IDX_KINGSAF (IDX_MOB_KNIGHT+7)

static int is_naninf(float x){ return isnan(x) || isinf(x); }

void eval_weights_default(EvalWeights *ew){
    memset(ew,0,sizeof(*ew));
    float d[EVAL_WEIGHT_COUNT];
    for(int i=0;i<EVAL_WEIGHT_COUNT;i++) d[i]=0.0f;
    /* material */
    d[0]=100.0f; d[1]=320.0f; d[2]=330.0f; d[3]=500.0f; d[4]=900.0f; d[5]=0.0f;
    /* PST basic */
    for(int p=0;p<6;p++) for(int sq=0;sq<64;sq++){
        int idx=IDX_PST(p,sq);
        float v=0;
        /* center bonus for knights */
        if(p==PIECE_KNIGHT){
            int f=sq%8,r=sq/8;
            float c = ( (f>=2&&f<=5) && (r>=2&&r<=5) ) ? 20.0f : 0.0f;
            v += c;
            /* edge penalty */
            if(f==0||f==7||r==0||r==7) v -= 15.0f;
        }
        if(p==PIECE_PAWN){
            int r=sq/8;
            v += (float)r*3.0f; /* advanced better (white perspective) */
            if(r==6) v += 0; 
        }
        if(p==PIECE_KING){
            int f=sq%8,r=sq/8;
            /* encourage castled/corner */
            if(r==0 && (f==1||f==2||f==5||f==6)) v += 10.0f;
            if(r==7 && (f==1||f==2||f==5||f==6)) v += 10.0f;
        }
        d[idx]=v;
    }
    /* mobility/small terms */
    d[IDX_MOB_KNIGHT]=5.0f; d[IDX_MOB_BISHOP]=3.0f; d[IDX_MOB_ROOK]=2.0f; d[IDX_MOB_QUEEN]=1.0f;
    d[IDX_DOUBLED]=-10.0f; d[IDX_ISOLATED]=-12.0f; d[IDX_PASSED]=20.0f; d[IDX_KINGSAF]=-8.0f;

    for(int i=0;i<EVAL_WEIGHT_COUNT;i++){ ew->w[i]=d[i]; ew->iw[i]=(int)(d[i]+(d[i]>=0?0.5f:-0.5f)); }
}

int eval_load_weights(EvalWeights *ew, const char *path){
    FILE *f=fopen(path,"rb");
    if(!f) return -1;
    char magic[4];
    if(fread(magic,1,4,f)!=4){ fclose(f); return -1; }
    if(memcmp(magic,"BWCW",4)!=0){ fclose(f); return -1; }
    uint32_t ver,count,type;
    if(fread(&ver,4,1,f)!=1){ fclose(f); return -1; }
    if(fread(&count,4,1,f)!=1){ fclose(f); return -1; }
    if(fread(&type,4,1,f)!=1){ fclose(f); return -1; }
    /* little-endian assumed */
    if(ver!=1){ fclose(f); return -1; }
    if(count!=EVAL_WEIGHT_COUNT){ fclose(f); return -1; }
    if(type!=0){ fclose(f); return -1; }
    for(int i=0;i<EVAL_WEIGHT_COUNT;i++){
        float v;
        if(fread(&v,4,1,f)!=1){ fclose(f); return -1; }
        if(is_naninf(v)){ fclose(f); return -1; }
        ew->w[i]=v;
        ew->iw[i]=(int)(v + (v>=0?0.5f:-0.5f));
    }
    /* ensure no extra huge? */
    fclose(f);
    return 0;
}

static inline int mirror_sq(int sq){ return sq ^ 56; }

static int eval_material_pst(const Position *p, const EvalWeights *ew){
    int score=0;
    /* white pieces */
    for(int t=0;t<6;t++){
        Bitboard bb=p->pieces[COLOR_WHITE][t];
        while(bb){
            int sq=__builtin_ctzll(bb); bb&=bb-1;
            score += ew->iw[IDX_MAT(t)] + ew->iw[IDX_PST(t,sq)];
        }
    }
    /* black pieces subtract; use same pst mirrored for black (turn board) */
    for(int t=0;t<6;t++){
        Bitboard bb=p->pieces[COLOR_BLACK][t];
        while(bb){
            int sq=__builtin_ctzll(bb); bb&=bb-1;
            score -= ew->iw[IDX_MAT(t)] + ew->iw[IDX_PST(t, mirror_sq(sq))];
        }
    }
    return score;
}

static int pawn_structure(const Position *p, const EvalWeights *ew){
    (void)ew;
    int score=0;
    /* doubled/isolated for white */
    int wfiles[8]={0}, bfiles[8]={0};
    Bitboard bb=p->pieces[COLOR_WHITE][PIECE_PAWN];
    while(bb){ int sq=__builtin_ctzll(bb); bb&=bb-1; wfiles[sq%8]++; }
    bb=p->pieces[COLOR_BLACK][PIECE_PAWN];
    while(bb){ int sq=__builtin_ctzll(bb); bb&=bb-1; bfiles[sq%8]++; }
    for(int f=0;f<8;f++){
        if(wfiles[f]>1) score += ew->iw[IDX_DOUBLED]*(wfiles[f]-1);
        if(bfiles[f]>1) score -= ew->iw[IDX_DOUBLED]*(bfiles[f]-1);
        /* isolated */
        int wleft = (f>0)?wfiles[f-1]:0;
        int wright = (f<7)?wfiles[f+1]:0;
        if(wfiles[f]>0 && wleft==0 && wright==0) score += ew->iw[IDX_ISOLATED]*wfiles[f];
        int bleft = (f>0)?bfiles[f-1]:0;
        int bright = (f<7)?bfiles[f+1]:0;
        if(bfiles[f]>0 && bleft==0 && bright==0) score -= ew->iw[IDX_ISOLATED]*bfiles[f];
    }
    /* passed pawns white */
    for(int sq=0; sq<64; sq++){
        if(!(p->pieces[COLOR_WHITE][PIECE_PAWN]&(1ULL<<sq))) continue;
        int f=sq%8, r=sq/8;
        int blocked=0;
        for(int rf=r+1; rf<=7; rf++){
            int idx=rf*8+f;
            if(p->pieces[COLOR_BLACK][PIECE_PAWN]&(1ULL<<idx)){ blocked=1; break; }
            if(f>0 && (p->pieces[COLOR_BLACK][PIECE_PAWN]&(1ULL<<(idx-1)))) { blocked=1; break; }
            if(f<7 && (p->pieces[COLOR_BLACK][PIECE_PAWN]&(1ULL<<(idx+1)))) { blocked=1; break; }
        }
        if(!blocked) score += ew->iw[IDX_PASSED];
    }
    for(int sq=0; sq<64; sq++){
        if(!(p->pieces[COLOR_BLACK][PIECE_PAWN]&(1ULL<<sq))) continue;
        int f=sq%8, r=sq/8;
        int blocked=0;
        for(int rf=r-1; rf>=0; rf--){
            int idx=rf*8+f;
            if(p->pieces[COLOR_WHITE][PIECE_PAWN]&(1ULL<<idx)){ blocked=1; break; }
            if(f>0 && (p->pieces[COLOR_WHITE][PIECE_PAWN]&(1ULL<<(idx-1)))) { blocked=1; break; }
            if(f<7 && (p->pieces[COLOR_WHITE][PIECE_PAWN]&(1ULL<<(idx+1)))) { blocked=1; break; }
        }
        if(!blocked) score -= ew->iw[IDX_PASSED];
    }
    (void)p;
    return score;
}

static int king_safety(const Position *p, const EvalWeights *ew){
    int score=0;
    for(int c=0;c<2;c++){
        /* attackers around king */
        int ksq=-1;
        Bitboard kb=p->pieces[c][PIECE_KING];
        if(!kb) continue;
        ksq=__builtin_ctzll(kb);
        int f=file_of(ksq), r=rank_of(ksq);
        int att=0;
        for(int df=-1;df<=1;df++) for(int dr=-1;dr<=1;dr++){
            int nf=f+df, nr=r+dr;
            if(nf<0||nf>7||nr<0||nr>7) continue;
            int s=nr*8+nf;
            /* count enemy pieces attacking s */
            if(square_attacked(p,s,c^1)) att++;
        }
        if(c==COLOR_WHITE) score += ew->iw[IDX_KINGSAF]*att;
        else score -= ew->iw[IDX_KINGSAF]*att;
    }
    return score;
}

int evaluate_position(const Position *p, const EvalWeights *ew){
    int score = eval_material_pst(p,ew) + pawn_structure(p,ew) + king_safety(p,ew);
    /* mobility optional light: piece counts toward center? skip to avoid movegen */
    if(p->side==COLOR_BLACK) return -score;
    return score;
}
