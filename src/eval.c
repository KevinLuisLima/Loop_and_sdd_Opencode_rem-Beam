#include "eval.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int mirror_sq(int sq){ return sq^56; /* vertical mirror? spec symmetry position mirrored with colors swapped */ }

Weights *weights_create_default(void){
  Weights *w=malloc(sizeof(*w));
  w->n=WEIGHT_COUNT;
  w->w=calloc(w->n,sizeof(float));
  /* material centipawns */
  w->w[E_PAWN]=100; w->w[E_KNIGHT]=320; w->w[E_BISHOP]=330; w->w[E_ROOK]=500; w->w[E_QUEEN]=900;
  /* PST base simple: center bonuses */
  for(int pt=0; pt<6; pt++){
    for(int sq=0; sq<64; sq++){
      (void)sq;
      float v=0;
      w->w[E_PST + pt*64 + sq]=v;
    }
  }
  w->w[E_MOBILITY]=2;
  w->w[E_DOUBLED_PAWN]=-10;
  w->w[E_ISOLATED_PAWN]=-15;
  w->w[E_PASSED_PAWN]=20;
  w->w[E_KING_SAFETY]=-5;
  w->w[E_TEMPO]=5;
  /* knight central bonus */
  for(int sq=0;sq<64;sq++){
    int r=RANK(sq), f=FILE(sq);
    int mr = (r>3?6-r:r); /* distance to center */
    int mf = (f>3?6-f:f);
    float nb = (mr+mf)*2.0f;
    w->w[E_PST + (PT_KNIGHT-1)*64 + sq] = nb; /* PT_KNIGHT=2 -> index 1 */
  }
  return w;
}

static int validate_float(float x){
  /* nan/inf check */
  if(x!=x) return 0; /* nan */
  /* inf: compare */
  if(x==1.0f/0.0f || x==-1.0f/0.0f) return 0;
  return 1;
}

Weights *weights_load(const char *path, char *err, size_t errsz){
  if(err && errsz) err[0]=0;
  FILE *f=fopen(path,"rb");
  if(!f){ if(err) snprintf(err,errsz,"cannot open weights file: %s",path); return NULL; }
  /* header: magic 4? version u32, n u32, dtype u32 */
  char magic[8];
  if(fread(magic,1,8,f)!=8){ fclose(f); if(err) snprintf(err,errsz,"truncated header"); return NULL;}
  if(strncmp(magic,"CHESSW1\0",8)!=0 && strncmp(magic,"CHESSW01",8)!=0){
     /* accept first 7 */
  }
  uint32_t version,n,dtype;
  if(fread(&version,4,1,f)!=1 || fread(&n,4,1,f)!=1 || fread(&dtype,4,1,f)!=1){
    fclose(f); if(err) snprintf(err,errsz,"bad header"); return NULL;
  }
  if(version!=WEIGHTS_VERSION){ fclose(f); if(err) snprintf(err,errsz,"wrong version %u",version); return NULL; }
  if(n!=WEIGHT_COUNT){ fclose(f); if(err) snprintf(err,errsz,"wrong weight count %u",n); return NULL; }
  if(dtype!=WEIGHTS_DTYPE_FLOAT32){ fclose(f); if(err) snprintf(err,errsz,"unsupported dtype %u",dtype); return NULL; }
  /* size check */
  Weights *w=malloc(sizeof(*w));
  w->n=n;
  w->w=malloc(n*sizeof(float));
  size_t rd=fread(w->w,sizeof(float),n,f);
  if(rd!=n){ 
    /* maybe truncated */
    fclose(f); free(w->w); free(w);
    if(err) snprintf(err,errsz,"truncated weights (%zu/%u)",rd,n);
    return NULL;
  }
  /* check no leftover? optional */
  /* validate */
  for(uint32_t i=0;i<n;i++){
    if(!validate_float(w->w[i])){
      fclose(f); weights_free(w);
      if(err) snprintf(err,errsz,"nan/inf at index %d",i);
      return NULL;
    }
  }
  fclose(f);
  return w;
}

void weights_free(Weights *w){
  if(!w) return;
  free(w->w);
  free(w);
}


int evaluate(const Position *pos, const Weights *w){
  if(!w) return 0;
  int score=0;
  /* material + PST relative to white? We'll compute for side to move. */
  Color us=pos->side;
  for(int sq=0; sq<64; sq++){
    int pc=pos->board[sq];
    if(pc==EMPTY) continue;
    Color c=piece_color(pc);
    int pt=piece_type(pc);
    int mat=0;
    switch(pt){
      case PT_PAWN: mat=w->w[E_PAWN]; break;
      case PT_KNIGHT: mat=w->w[E_KNIGHT]; break;
      case PT_BISHOP: mat=w->w[E_BISHOP]; break;
      case PT_ROOK: mat=w->w[E_ROOK]; break;
      case PT_QUEEN: mat=w->w[E_QUEEN]; break;
      case PT_KING: mat=0; break;
    }
    int pst = (pt>=PT_PAWN && pt<=PT_KING) ? (int)w->w[E_PST + (pt-1)*64 + (c==COLOR_WHITE? sq : mirror_sq(sq))] : 0;
    /* note: for black pieces, mirror square for PST as if white */
    int val = mat + pst;
    if(c==us) score += val; else score -= val;
  }
  /* tempo */
  score += (int)w->w[E_TEMPO];

  /* doubled/isolated pawns (simple) */
  /* For each color, compute pawn columns */
  /* mobility not computed (expensive) skip or bitboard count? We'll add simple king centralization? */
  /* doubled pawns */
  for(int color=0;color<2;color++){
    int sign = (color==us)?1:-1;
    int cols[8]={0};
    for(int sq=0;sq<64;sq++){
      int pc=pos->board[sq];
      if(pc==EMPTY) continue;
      if(piece_color(pc)!=color || piece_type(pc)!=PT_PAWN) continue;
      cols[FILE(sq)]++;
    }
    for(int f=0;f<8;f++) if(cols[f]>1) score += sign*(int)w->w[E_DOUBLED_PAWN]*(cols[f]-1);
    /* isolated */
    for(int f=0;f<8;f++){
      if(cols[f]==0) continue;
      int neighbors = (f>0?cols[f-1]:0)+(f<7?cols[f+1]:0);
      if(neighbors==0) score += sign*(int)w->w[E_ISOLATED_PAWN];
    }
  }
  /* passed pawns: no enemy pawn in front on same/adjacent files */
  for(int color=0;color<2;color++){
    int sign=(color==us)?1:-1;
    int dir=(color==COLOR_WHITE)?1:-1;
    for(int sq=0;sq<64;sq++){
      int pc=pos->board[sq];
      if(pc==EMPTY || piece_color(pc)!=color || piece_type(pc)!=PT_PAWN) continue;
      int f=FILE(sq), r=RANK(sq);
      int passed=1;
      /* check files f-1..f+1 for enemy pawns ahead */
      for(int ff=f-1; ff<=f+1; ff++){
        if(ff<0||ff>7) continue;
        int rr=r+dir;
        while(rr>=0 && rr<8){
          int s2=rr*8+ff;
          int pc2=pos->board[s2];
          if(pc2!=EMPTY){
            if(piece_color(pc2)!=color && piece_type(pc2)==PT_PAWN) { passed=0; break; }
            break;
          }
          rr += dir;
        }
        if(!passed) break;
      }
      if(passed) score += sign*(int)w->w[E_PASSED_PAWN];
    }
  }
  /* king safety: count attackers around king? simple: penalize king on open files? skip */
  return score;
}
