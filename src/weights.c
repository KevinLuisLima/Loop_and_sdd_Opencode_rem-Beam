#include "weights.h"
#include "eval.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define WEIGHTS_MAGIC 0x31535754
#define WEIGHTS_VERSION 1

void weights_default(Weights *w) {
    w->version = 1;
    w->count = NUM_WEIGHTS;
    for (int i = 0; i < NUM_WEIGHTS; i++) w->vals[i] = 0.0f;
    w->vals[W_MATERIAL+0] = 100.0f;
    w->vals[W_MATERIAL+1] = 300.0f;
    w->vals[W_MATERIAL+2] = 310.0f;
    w->vals[W_MATERIAL+3] = 500.0f;
    w->vals[W_MATERIAL+4] = 900.0f;
    for (int sq = 0; sq < 64; sq++) {
        int r = sq / 8;
        w->vals[W_PST + PAWN*64 + sq] = 5.0f * (r - 1);
    }
    for (int sq = 0; sq < 64; sq++) {
        int r = sq/8, c = sq%8;
        int df = (c < 4) ? 3-c : c-4;
        int dr = (r < 4) ? 3-r : r-4;
        w->vals[W_PST + KNIGHT*64 + sq] = 20.0f - 4.0f*(df+dr);
    }
    for (int sq = 0; sq < 64; sq++) {
        int r = sq/8, c = sq%8;
        int center = (r>=2 && r<=5 && c>=2 && c<=5) ? 10 : 0;
        w->vals[W_PST + BISHOP*64 + sq] = center;
    }
    for (int sq = 0; sq < 64; sq++) {
        int r = sq/8;
        w->vals[W_PST + ROOK*64 + sq] = (r == 6) ? 10.0f : 0.0f;
    }
    for (int sq = 0; sq < 64; sq++) {
        int r = sq/8, c = sq%8;
        int dfc = (c < 4)?3-c:c-4; int drc=(r<4)?3-r:r-4;
        w->vals[W_PST + QUEEN*64 + sq] = 8.0f - 1.5f*(dfc+drc);
    }
    for (int sq = 0; sq < 64; sq++) {
        int r = sq/8, c = sq%8;
        float v = 0;
        if (r == 0 && (c <= 2 || c >= 5)) v = 20.0f;
        if (r <= 1) v += 5.0f;
        w->vals[W_PST + KING*64 + sq] = v;
    }
    w->vals[W_MOBILITY] = 1.0f;
    w->vals[W_DOUBLED] = -10.0f;
    w->vals[W_ISOLATED] = -8.0f;
    w->vals[W_KING] = -5.0f;
    for (int i = 0; i < NUM_WEIGHTS; i++) if (isnan(w->vals[i]) || isinf(w->vals[i])) w->vals[i]=0;
}

int weights_load(const char *path, Weights *w, char *err, size_t errsz) {
    FILE *f = fopen(path, "rb");
    if (!f) { if (err) snprintf(err, errsz, "cannot open %s", path); return -1; }
    unsigned char hdr[16];
    if (fread(hdr, 1, 16, f) != 16) { if (err) snprintf(err,errsz,"truncated header"); fclose(f); return -1; }
    unsigned int magic = hdr[0] | (hdr[1]<<8) | (hdr[2]<<16) | (hdr[3]<<24);
    if (magic != WEIGHTS_MAGIC) { if (err) snprintf(err,errsz,"bad magic"); fclose(f); return -1; }
    unsigned int version = hdr[4] | (hdr[5]<<8) | (hdr[6]<<16) | (hdr[7]<<24);
    unsigned int count = hdr[8] | (hdr[9]<<8) | (hdr[10]<<16) | (hdr[11]<<24);
    if (count > 1024) { if (err) snprintf(err,errsz,"too many weights"); fclose(f); return -1; }
    if (version != WEIGHTS_VERSION) { if (err) snprintf(err,errsz,"bad version %u", version); fclose(f); return -1; }
    w->version = version;
    w->count = count;
    size_t got = fread(w->vals, sizeof(float), count, f);
    if (got != count) { if (err) snprintf(err,errsz,"truncated weights"); fclose(f); return -1; }
    for (unsigned i = 0; i < count; i++) {
        if (isnan(w->vals[i]) || isinf(w->vals[i])) { if (err) snprintf(err,errsz,"nan/inf in weights"); fclose(f); return -1; }
    }
    for (unsigned i = count; i < 1024; i++) w->vals[i] = 0.0f;
    fclose(f);
    return 0;
}

int weights_save(const char *path, const Weights *w, char *err, size_t errsz) {
    FILE *f = fopen(path, "wb");
    if (!f) { if (err) snprintf(err,errsz,"cannot open %s", path); return -1; }
    unsigned char hdr[16];
    unsigned int magic = WEIGHTS_MAGIC;
    hdr[0]=magic&0xff; hdr[1]=(magic>>8)&0xff; hdr[2]=(magic>>16)&0xff; hdr[3]=(magic>>24)&0xff;
    unsigned int version = w->version;
    hdr[4]=version&0xff; hdr[5]=(version>>8)&0xff; hdr[6]=(version>>16)&0xff; hdr[7]=(version>>24)&0xff;
    unsigned int count = w->count;
    hdr[8]=count&0xff; hdr[9]=(count>>8)&0xff; hdr[10]=(count>>16)&0xff; hdr[11]=(count>>24)&0xff;
    hdr[12]=0; hdr[13]=0; hdr[14]=0; hdr[15]=0;
    if (fwrite(hdr,1,16,f)!=16) { if (err) snprintf(err,errsz,"write hdr fail"); fclose(f); return -1; }
    if (fwrite(w->vals, sizeof(float), count, f) != count) { if(err) snprintf(err,errsz,"write vals fail"); fclose(f); return -1; }
    fclose(f);
    return 0;
}
