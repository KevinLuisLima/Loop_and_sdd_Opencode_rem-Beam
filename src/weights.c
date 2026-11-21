#include "weights.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static uint32_t read_u32_le(FILE *f) {
    unsigned char b[4];
    if (fread(b,1,4,f)!=4) return 0;
    return (uint32_t)b[0] | ((uint32_t)b[1]<<8) | ((uint32_t)b[2]<<16) | ((uint32_t)b[3]<<24);
}

int weightsLoad(const char *path, Weights *out, char *errbuf, size_t errsz) {
    if (!path||!out) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) { if(errbuf) snprintf(errbuf,errsz,"cannot open %s",path); return 0; }
    uint32_t version = read_u32_le(f);
    uint32_t num = read_u32_le(f);
    if (version != WEIGHTS_VERSION) {
        if(errbuf) snprintf(errbuf,errsz,"bad version %u", version);
        fclose(f); return 0;
    }
    if (num != EXPECTED_WEIGHTS) {
        if(errbuf) snprintf(errbuf,errsz,"bad num weights %u", num);
        fclose(f); return 0;
    }
    size_t got = fread(out->w, sizeof(float), num, f);
    if (got != num) {
        if(errbuf) snprintf(errbuf,errsz,"truncated read");
        fclose(f); return 0;
    }
    int ok = weightsValidate(out);
    if (!ok) {
        if(errbuf) snprintf(errbuf,errsz,"invalid values");
    }
    fclose(f);
    out->count = (int)num;
    return ok;
}

int weightsValidate(const Weights *w) {
    if (!w) return 0;
    for (int i=0;i<EXPECTED_WEIGHTS;i++) {
        if (isnan(w->w[i]) || isinf(w->w[i])) return 0;
    }
    return 1;
}