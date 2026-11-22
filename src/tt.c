#include "tt.h"
#include <string.h>

void tt_clear(TransTable *tt){
    if (!tt) return;
    memset(tt,0,sizeof(TransTable));
}
TtEntry *tt_probe(TransTable *tt, uint64_t key){
    if (!tt) return NULL;
    TtEntry *e=&tt->entries[key % TT_SIZE];
    if (e->key==key && e->depth>=0) return e;
    if (e->key==0) return NULL; /* not found */
    /* verification */
    if (e->key==key) return e;
    return NULL;
}
void tt_store(TransTable *tt, uint64_t key, int depth, int score, int flags, Move move){
    if (!tt) return;
    TtEntry *e=&tt->entries[key % TT_SIZE];
    if (e->key != key || depth >= e->depth){
        e->key=key; e->depth=depth; e->score=score; e->flags=flags; e->move=move;
    }
}