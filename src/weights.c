#include "weights.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

static void default_weight_path(char *buf, size_t sz){
    const char *cands[] = {"weights/default.bin","./weights/default.bin","../weights/default.bin","src/weights/default.bin"};
    for (size_t i=0;i<sizeof(cands)/sizeof(cands[0]);i++){
        FILE *f=fopen(cands[i],"rb");
        if (f){ fclose(f); snprintf(buf,sz,"%s",cands[i]); return; }
    }
    snprintf(buf,sz,"weights/default.bin");
}
int weights_default_path(char *buf, size_t sz){ default_weight_path(buf,sz); return 0; }

int weights_load(Weights *w, const char *path){
    if(!w||!path) return -1;
    FILE *f=fopen(path,"rb");
    if(!f) return -1;
    char magic[4]; uint32_t version, count;
    if (fread(magic,1,4,f)!=4){ fclose(f); return -1; }
    if (magic[0]!='C' || magic[1]!='B' || magic[2]!='S' || magic[3]!='W'){ fclose(f); return -1; }
    if (fread(&version,4,1,f)!=1){ fclose(f); return -1; }
    if (fread(&count,4,1,f)!=1){ fclose(f); return -1; }
    /* little endian? assume host little */
    unsigned char dtype;
    if (fread(&dtype,1,1,f)!=1){ fclose(f); return -1; }
    if (version!=WEIGHT_VERSION) { fclose(f); return -1; }
    if (count != WEIGHT_COUNT) { fclose(f); return -1; }
    if (dtype != 0) { fclose(f); return -1; }
    w->count = count;
    w->data = malloc(count*sizeof(float));
    if (!w->data){ fclose(f); return -1; }
    if (fread(w->data,sizeof(float),count,f) != count){ free(w->data); w->data=NULL; fclose(f); return -1; }
    /* check nan/inf */
    for (uint32_t i=0;i<count;i++){
        if (!isfinite(w->data[i])) { free(w->data); w->data=NULL; fclose(f); return -1; }
    }
    /* ensure no trailing large */
    fclose(f);
    return 0;
}
void weights_free(Weights *w){
    if (!w) return;
    free(w->data);
    w->data=NULL; w->count=0;
}
float weights_get(const Weights *w, int idx){
    if (!w || !w->data || idx<0 || idx>=w->count) return 0.0f;
    return w->data[idx];
}