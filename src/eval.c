#include "eval.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define WEIGHT_MAGIC0 'C'
#define WEIGHT_MAGIC1 'H'
#define WEIGHT_MAGIC2 'W'
#define WEIGHT_MAGIC3 'T'

static void pawn_structure(Position *pos, int *dw, int *iw, int *db, int *ib) {
    *dw=*iw=*db=*ib=0;
    /* files 0..7 */
    int wcount[8]={0}, bcount[8]={0};
    for (int f=0;f<8;f++) {
        for (int r=0;r<8;r++) {
            int sq=r*8+f;
            if (pos->board[sq]==WPAWN) wcount[f]++;
            if (pos->board[sq]==BPAWN) bcount[f]++;
        }
    }
    /* doubled: extra pawns per file */
    for (int f=0;f<8;f++) {
        if (wcount[f]>1) *dw += wcount[f]-1;
        if (bcount[f]>1) *db += bcount[f]-1;
    }
    /* isolated */
    for (int f=0;f<8;f++) {
        for (int r=0;r<8;r++) {
            int sq=r*8+f;
            if (pos->board[sq]==WPAWN) {
                int left = (f>0)? wcount[f-1] : 0;
                int right = (f<7)? wcount[f+1] : 0;
                if (left==0 && right==0) (*iw)++;
            }
            if (pos->board[sq]==BPAWN) {
                int left = (f>0)? bcount[f-1] : 0;
                int right = (f<7)? bcount[f+1] : 0;
                if (left==0 && right==0) (*ib)++;
            }
        }
    }
}

void eval_features(Position *pos, float *feat) {
    memset(feat, 0, FEATURE_COUNT*sizeof(float));
    for (int sq=0; sq<64; sq++) {
        int pc = pos->board[sq];
        if (pc == EMPTY) continue;
        int col = code_color(pc);
        int ti = code_type_idx(pc);
        /* only tune material for pawn..queen (0..4). king no material */
        if (ti <= 4) {
            if (col == WT) feat[ti] += 1.0f;
            else feat[ti] -= 1.0f;
        }
        /* PST index */
        int pst_base = 5 + ti*64; /* 5 material + 5 types? king ti=5 => 5+320=325 */
        /* Wait: king ti=5 -> base 5+320=325 okay. material indices 0..4 uses type idx as material index, but king type idx 5 ignored above. */
        int mirror = sq ^ 56;
        if (col == WT) {
            feat[pst_base + sq] += 1.0f;
        } else {
            feat[pst_base + mirror] -= 1.0f;
        }
    }
    int dw,iw,db,ib;
    pawn_structure(pos,&dw,&iw,&db,&ib);
    feat[389] += (float)(dw - db);
    feat[390] += (float)(iw - ib);
    /* mobility 391 stays 0 */
}

int eval_score(Position *pos, const float *w) {
    float feat[FEATURE_COUNT];
    eval_features(pos, feat);
    float s = 0.0f;
    for (int i=0;i<FEATURE_COUNT;i++) s += feat[i]*w[i];
    if (pos->side == BT) s = -s;
    return (int) (s * 1.0f);
}

int eval_load_weights(const char *path, float **out, char *err, size_t errsz) {
    FILE *f = fopen(path, "rb");
    if (!f) { snprintf(err, errsz, "cannot open weights: %s", path); return 1; }
    unsigned char magic[4];
    if (fread(magic,1,4,f) != 4 || magic[0]!=WEIGHT_MAGIC0 || magic[1]!=WEIGHT_MAGIC1 || magic[2]!=WEIGHT_MAGIC2 || magic[3]!=WEIGHT_MAGIC3) {
        fclose(f); snprintf(err,errsz,"bad magic"); return 1;
    }
    uint32_t version, n, type;
    if (fread(&version,4,1,f)!=1 || fread(&n,4,1,f)!=1 || fread(&type,4,1,f)!=1) {
        fclose(f); snprintf(err,errsz,"bad header"); return 1;
    }
    /* ensure little-endian? assume */
    if (version != 1) { fclose(f); snprintf(err,errsz,"bad version %u", version); return 1; }
    if (n != FEATURE_COUNT) { fclose(f); snprintf(err,errsz,"bad nweights %u (exp %d)", n, FEATURE_COUNT); return 1; }
    if (type != 0) { fclose(f); snprintf(err,errsz,"bad type %u", type); return 1; }
    /* read rest maybe size */
    float *buf = malloc(n*sizeof(float));
    if (!buf) { fclose(f); snprintf(err,errsz,"oom"); return 1; }
    if (fread(buf,sizeof(float),n,f) != n) {
        /* maybe weights not enough */
        free(buf); fclose(f); snprintf(err,errsz,"truncated weights"); return 1;
    }
    /* validate finite */
    for (int i=0;i<(int)n;i++) {
        if (!isfinite(buf[i])) { free(buf); fclose(f); snprintf(err,errsz,"nan/inf at %d",i); return 1; }
    }
    fclose(f);
    *out = buf;
    return 0;
}