#include "board.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

uint64_t KNIGHT_ATK[64];
uint64_t KING_ATK[64];
uint64_t PAWN_ATK[2][64];

static uint64_t piece_rand[2][6][64];
static uint64_t side_rand;
static uint64_t castle_rand[16];
static uint64_t ep_rand[64];

static uint64_t rng_state = 0x123456789ABCDEFULL;
static uint64_t rng(void) { rng_state = rng_state * 6364136223846793005ULL + 1442695040888963407ULL; return rng_state; }

static void precompute_attacks(void) {
    for (int sq = 0; sq < 64; sq++) {
        int r = sq / 8, c = sq % 8;
        uint64_t k = 0, kn = 0;
        int dr[8] = {-2,-2,-1,-1,1,1,2,2};
        int dc[8] = {-1,1,-2,2,-2,2,-1,1};
        for (int i = 0; i < 8; i++) {
            int nr = r + dr[i], nc = c + dc[i];
            if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8) kn |= 1ULL << (nr * 8 + nc);
        }
        int kdr[8] = {-1,-1,-1,0,0,1,1,1};
        int kdc[8] = {-1,0,1,-1,1,-1,0,1};
        for (int i = 0; i < 8; i++) {
            int nr = r + kdr[i], nc = c + kdc[i];
            if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8) k |= 1ULL << (nr * 8 + nc);
        }
        KNIGHT_ATK[sq] = kn;
        KING_ATK[sq] = k;
        uint64_t wp = 0, bp = 0;
        if (r < 7) {
            if (c > 0) wp |= 1ULL << ((r + 1) * 8 + (c - 1));
            if (c < 7) wp |= 1ULL << ((r + 1) * 8 + (c + 1));
        }
        if (r > 0) {
            if (c > 0) bp |= 1ULL << ((r - 1) * 8 + (c - 1));
            if (c < 7) bp |= 1ULL << ((r - 1) * 8 + (c + 1));
        }
        PAWN_ATK[WHITE][sq] = wp;
        PAWN_ATK[BLACK][sq] = bp;
    }
}

static void init_zobrist(void) {
    for (int col = 0; col < 2; col++)
        for (int pt = 0; pt < 6; pt++)
            for (int sq = 0; sq < 64; sq++)
                piece_rand[col][pt][sq] = rng();
    side_rand = rng();
    for (int i = 0; i < 16; i++) castle_rand[i] = rng();
    for (int i = 0; i < 64; i++) ep_rand[i] = rng();
}

void board_init(Board *b) {
    if (!KNIGHT_ATK[0]) { precompute_attacks(); }
    if (!side_rand) { init_zobrist(); }
    memset(b, 0, sizeof(*b));
    const char *pieces = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR";
    b->side = WHITE;
    b->castle = 15;
    b->ep = -1;
    int sq = 56;
    for (const char *p = pieces; *p; p++) {
        if (*p == '/') { sq -= 16; continue; }
        if (isdigit((unsigned char)*p)) { sq += *p - '0'; continue; }
        int color = isupper((unsigned char)*p) ? WHITE : BLACK;
        int pt = -1;
        char ch = tolower((unsigned char)*p);
        if (ch == 'p') pt = PAWN;
        else if (ch == 'n') pt = KNIGHT;
        else if (ch == 'b') pt = BISHOP;
        else if (ch == 'r') pt = ROOK;
        else if (ch == 'q') pt = QUEEN;
        else if (ch == 'k') pt = KING;
        if (pt >= 0) {
            b->pieces[color][pt] |= 1ULL << sq;
            sq++;
        }
    }
    for (int col = 0; col < 2; col++)
        for (int pt = 0; pt < 6; pt++)
            b->occ[col] |= b->pieces[col][pt];
    b->occ[2] = b->occ[0] | b->occ[1];
    b->hash = hash_board(b);
    b->hash_count = 0;
    b->hash_history[b->hash_count++] = b->hash;
}

static int piece_at(const Board *b, int sq, int *color, int *pt) {
    uint64_t bit = 1ULL << sq;
    for (int col = 0; col < 2; col++)
        for (int p = 0; p < 6; p++)
            if (b->pieces[col][p] & bit) { if (color) *color = col; if (pt) *pt = p; return 1; }
    return 0;
}

uint64_t hash_board(const Board *b) {
    uint64_t h = 0;
    for (int col = 0; col < 2; col++)
        for (int pt = 0; pt < 6; pt++) {
            uint64_t bb = b->pieces[col][pt];
            while (bb) { int sq = __builtin_ctzll(bb); bb &= bb - 1; h ^= piece_rand[col][pt][sq]; }
        }
    if (b->side == BLACK) h ^= side_rand;
    h ^= castle_rand[b->castle & 15];
    if (b->ep >= 0) h ^= ep_rand[b->ep];
    return h;
}

int board_load_fen(Board *b, const char *fen) {
    if (!KNIGHT_ATK[0]) { precompute_attacks(); }
    if (!side_rand) { init_zobrist(); }
    memset(b, 0, sizeof(*b));
    b->ep = -1;
    b->hash_count = 0;
    char buf[512]; strncpy(buf, fen, 511); buf[511] = 0;
    char *save = buf;
    char *boardpart = strtok_r(save, " ", &save);
    if (!boardpart) return -1;
    int sq = 56;
    for (char *p = boardpart; *p; p++) {
        if (*p == '/') { sq -= 16; continue; }
        if (isdigit((unsigned char)*p)) { sq += *p - '0'; continue; }
        int color = isupper((unsigned char)*p) ? WHITE : BLACK;
        int pt = -1;
        char ch = tolower((unsigned char)*p);
        if (ch == 'p') pt = PAWN;
        else if (ch == 'n') pt = KNIGHT;
        else if (ch == 'b') pt = BISHOP;
        else if (ch == 'r') pt = ROOK;
        else if (ch == 'q') pt = QUEEN;
        else if (ch == 'k') pt = KING;
        if (pt < 0) return -1;
        if (sq < 0 || sq > 63) return -1;
        b->pieces[color][pt] |= 1ULL << sq;
        sq++;
    }
    for (int col = 0; col < 2; col++)
        for (int pt = 0; pt < 6; pt++)
            b->occ[col] |= b->pieces[col][pt];
    b->occ[2] = b->occ[0] | b->occ[1];

    char *side = strtok_r(NULL, " ", &save);
    if (!side) return -1;
    b->side = (side[0] == 'b' || side[0] == 'B') ? BLACK : WHITE;

    char *castle = strtok_r(NULL, " ", &save);
    if (!castle) return -1;
    b->castle = 0;
    if (strchr(castle, 'K')) b->castle |= 1;
    if (strchr(castle, 'Q')) b->castle |= 2;
    if (strchr(castle, 'k')) b->castle |= 4;
    if (strchr(castle, 'q')) b->castle |= 8;
    if (castle[0] == '-') b->castle = 0;

    char *ep = strtok_r(NULL, " ", &save);
    if (ep) {
        if (ep[0] == '-') b->ep = -1;
        else b->ep = (ep[1] - '1') * 8 + (ep[0] - 'a');
    }
    char *hm = strtok_r(NULL, " ", &save);
    if (hm) b->halfmove = atoi(hm);
    char *fm = strtok_r(NULL, " ", &save);
    if (fm) b->fullmove = atoi(fm);
    else b->fullmove = 1;
    if (__builtin_popcountll(b->pieces[WHITE][KING]) != 1 || __builtin_popcountll(b->pieces[BLACK][KING]) != 1) {
        return -1;
    }
    b->hash = hash_board(b);
    b->hash_count = 0;
    b->hash_history[b->hash_count++] = b->hash;
    return 0;
}

void board_to_fen(const Board *b, char *buf, size_t sz) {
    int pos = 0;
    for (int r = 7; r >= 0; r--) {
        int empty = 0;
        for (int c = 0; c < 8; c++) {
            int sq = r * 8 + c;
            int col, pt;
            if (piece_at(b, sq, &col, &pt)) {
                if (empty) { buf[pos++] = '0' + empty; empty = 0; }
                char ch = "pnbrqk"[pt];
                if (col == WHITE) ch = toupper((unsigned char)ch);
                buf[pos++] = ch;
            } else empty++;
        }
        if (empty) buf[pos++] = '0' + empty;
        if (r > 0) buf[pos++] = '/';
    }
    buf[pos++] = ' ';
    buf[pos++] = b->side == WHITE ? 'w' : 'b';
    buf[pos++] = ' ';
    int any = 0;
    if (b->castle & 1) { buf[pos++] = 'K'; any = 1; }
    if (b->castle & 2) { buf[pos++] = 'Q'; any = 1; }
    if (b->castle & 4) { buf[pos++] = 'k'; any = 1; }
    if (b->castle & 8) { buf[pos++] = 'q'; any = 1; }
    if (!any) buf[pos++] = '-';
    buf[pos++] = ' ';
    if (b->ep >= 0) {
        buf[pos++] = 'a' + (b->ep % 8);
        buf[pos++] = '1' + (b->ep / 8);
    } else buf[pos++] = '-';
    buf[pos++] = ' ';
    char tmp[32]; snprintf(tmp, sizeof(tmp), "%d", b->halfmove);
    for (char *t = tmp; *t; t++) buf[pos++] = *t;
    buf[pos++] = ' ';
    snprintf(tmp, sizeof(tmp), "%d", b->fullmove);
    for (char *t = tmp; *t; t++) buf[pos++] = *t;
    buf[pos] = '\0';
    (void)sz;
}

static uint64_t sliding_attacks(int sq, uint64_t occ, int diag) {
    uint64_t res = 0;
    int dirs[4][2];
    if (diag) { int d[4][2]={{-1,-1},{-1,1},{1,-1},{1,1}}; memcpy(dirs,d,sizeof(dirs)); }
    else { int d[4][2]={{-1,0},{1,0},{0,-1},{0,1}}; memcpy(dirs,d,sizeof(dirs)); }
    int r = sq / 8, c = sq % 8;
    for (int d = 0; d < 4; d++) {
        int rr = r, cc = c;
        while (1) {
            rr += dirs[d][0]; cc += dirs[d][1];
            if (rr < 0 || rr > 7 || cc < 0 || cc > 7) break;
            int ns = rr * 8 + cc;
            res |= 1ULL << ns;
            if (occ & (1ULL << ns)) break;
        }
    }
    return res;
}

uint64_t attacks_to(const Board *b, int sq, int attacker_side) {
    uint64_t res = 0;
    /* pawns */
    uint64_t pawns = b->pieces[attacker_side][PAWN];
    uint64_t tmp = pawns;
    uint64_t pawn_att = 0;
    while (tmp) { int psq = __builtin_ctzll(tmp); tmp &= tmp - 1; pawn_att |= PAWN_ATK[attacker_side][psq]; }
    if (pawn_att & (1ULL << sq)) res |= 1ULL << sq;
    /* knights */
    tmp = b->pieces[attacker_side][KNIGHT];
    while (tmp) { int ks = __builtin_ctzll(tmp); tmp &= tmp - 1; if (KNIGHT_ATK[ks] & (1ULL << sq)) { res |= 1ULL << sq; break; } }
    /* king */
    if (b->pieces[attacker_side][KING]) {
        int ks = __builtin_ctzll(b->pieces[attacker_side][KING]);
        if (KING_ATK[ks] & (1ULL << sq)) res |= 1ULL << sq;
    }
    /* bishops/queens */
    tmp = b->pieces[attacker_side][BISHOP] | b->pieces[attacker_side][QUEEN];
    while (tmp) { int bs = __builtin_ctzll(tmp); tmp &= tmp - 1; if (sliding_attacks(bs, b->occ[2], 1) & (1ULL << sq)) { res |= 1ULL << sq; break; } }
    /* rooks/queens */
    tmp = b->pieces[attacker_side][ROOK] | b->pieces[attacker_side][QUEEN];
    while (tmp) { int rs = __builtin_ctzll(tmp); tmp &= tmp - 1; if (sliding_attacks(rs, b->occ[2], 0) & (1ULL << sq)) { res |= 1ULL << sq; break; } }
    return res;
}

int in_check(const Board *b, int side) {
    if (b->pieces[side][KING] == 0) return 1;
    int ks = __builtin_ctzll(b->pieces[side][KING]);
    uint64_t atk = attacks_to(b, ks, side ^ 1);
    return atk != 0;
}

static void add_move(Move *moves, int *count, int from, int to, int promo, int flags) {
    moves[(*count)++] = MOVE_MAKE(from, to, promo, flags);
}

static void gen_pseudo(const Board *b, Move *moves, int *count, int caps_only) {
    int color = b->side;
    uint64_t own = color == WHITE ? b->occ[0] : b->occ[1];
    (void)own;
    uint64_t all = b->occ[2];

    /* pawns */
    uint64_t pawns = b->pieces[color][PAWN];
    uint64_t bb = pawns;
    int fwd = (color == WHITE) ? 8 : -8;
    int start_rank = (color == WHITE) ? 1 : 6;
    while (bb) {
        int sq = __builtin_ctzll(bb); bb &= bb - 1;
        int r = sq / 8;
        /* single push */
        int to = sq + fwd;
        if (!caps_only && to >= 0 && to < 64 && !(all & (1ULL << to))) {
            int toRank = to / 8;
            int isPromo = (color == WHITE) ? (toRank == 7) : (toRank == 0);
            if (isPromo) {
                for (int pr = 0; pr < 4; pr++) {
                    int code = (pr == 0) ? 4 : (pr == 1) ? 3 : (pr == 2) ? 2 : 1;
                    add_move(moves, count, sq, to, code, FLAG_PROMO);
                }
            } else {
                add_move(moves, count, sq, to, 0, FLAG_NONE);
                /* double push */
                if (r == start_rank) {
                    int to2 = sq + 2 * fwd;
                    if (to2 >= 0 && to2 < 64 && !(all & (1ULL << to2)))
                        add_move(moves, count, sq, to2, 0, FLAG_NONE);
                }
            }
        }
        /* captures */
        uint64_t capmask = PAWN_ATK[color][sq] & (color == WHITE ? b->occ[1] : b->occ[0]);
        uint64_t tmp = capmask;
        while (tmp) {
            int tosq = __builtin_ctzll(tmp); tmp &= tmp - 1;
            int toRank = tosq / 8;
            int isPromo = (color == WHITE) ? (toRank == 7) : (toRank == 0);
            if (isPromo) {
                for (int pr = 0; pr < 4; pr++) {
                    int code = (pr == 0) ? 4 : (pr == 1) ? 3 : (pr == 2) ? 2 : 1;
                    add_move(moves, count, sq, tosq, code, FLAG_PROMO);
                }
            } else {
                add_move(moves, count, sq, tosq, 0, FLAG_CAPTURE);
            }
        }
        /* en passant */
        if (b->ep >= 0) {
            if (PAWN_ATK[color][sq] & (1ULL << b->ep)) {
                /* ensure capture square occupied by opponent pawn? trust ep */
                add_move(moves, count, sq, b->ep, 0, FLAG_EP);
            }
        }
    }

    /* knights */
    bb = b->pieces[color][KNIGHT];
    while (bb) {
        int sq = __builtin_ctzll(bb); bb &= bb - 1;
        uint64_t atk = KNIGHT_ATK[sq] & ~((color == WHITE) ? b->occ[0] : b->occ[1]);
        uint64_t tmpa = atk;
        while (tmpa) {
            int to = __builtin_ctzll(tmpa); tmpa &= tmpa - 1;
            int fl = ((color == WHITE ? b->occ[1] : b->occ[0]) & (1ULL << to)) ? FLAG_CAPTURE : FLAG_NONE;
            if (caps_only && fl == FLAG_NONE) continue;
            add_move(moves, count, sq, to, 0, fl);
        }
    }
    /* bishops */
    bb = b->pieces[color][BISHOP];
    while (bb) {
        int sq = __builtin_ctzll(bb); bb &= bb - 1;
        uint64_t atk = sliding_attacks(sq, all, 1) & ~((color == WHITE) ? b->occ[0] : b->occ[1]);
        uint64_t tmpa = atk;
        while (tmpa) {
            int to = __builtin_ctzll(tmpa); tmpa &= tmpa - 1;
            int fl = ((color == WHITE ? b->occ[1] : b->occ[0]) & (1ULL << to)) ? FLAG_CAPTURE : FLAG_NONE;
            if (caps_only && fl == FLAG_NONE) continue;
            add_move(moves, count, sq, to, 0, fl);
        }
    }
    /* rooks */
    bb = b->pieces[color][ROOK];
    while (bb) {
        int sq = __builtin_ctzll(bb); bb &= bb - 1;
        uint64_t atk = sliding_attacks(sq, all, 0) & ~((color == WHITE) ? b->occ[0] : b->occ[1]);
        uint64_t tmpa = atk;
        while (tmpa) {
            int to = __builtin_ctzll(tmpa); tmpa &= tmpa - 1;
            int fl = ((color == WHITE ? b->occ[1] : b->occ[0]) & (1ULL << to)) ? FLAG_CAPTURE : FLAG_NONE;
            if (caps_only && fl == FLAG_NONE) continue;
            add_move(moves, count, sq, to, 0, fl);
        }
    }
    /* queens */
    bb = b->pieces[color][QUEEN];
    while (bb) {
        int sq = __builtin_ctzll(bb); bb &= bb - 1;
        uint64_t atk = (sliding_attacks(sq, all, 1) | sliding_attacks(sq, all, 0)) & ~((color == WHITE) ? b->occ[0] : b->occ[1]);
        uint64_t tmpa = atk;
        while (tmpa) {
            int to = __builtin_ctzll(tmpa); tmpa &= tmpa - 1;
            int fl = ((color == WHITE ? b->occ[1] : b->occ[0]) & (1ULL << to)) ? FLAG_CAPTURE : FLAG_NONE;
            if (caps_only && fl == FLAG_NONE) continue;
            add_move(moves, count, sq, to, 0, fl);
        }
    }
    /* king */
    bb = b->pieces[color][KING];
    if (bb) {
        int sq = __builtin_ctzll(bb);
        uint64_t ownocc = (color == WHITE) ? b->occ[0] : b->occ[1];
        uint64_t atk = KING_ATK[sq] & ~ownocc;
        uint64_t tmpa = atk;
        while (tmpa) {
            int to = __builtin_ctzll(tmpa); tmpa &= tmpa - 1;
            int fl = ((color == WHITE ? b->occ[1] : b->occ[0]) & (1ULL << to)) ? FLAG_CAPTURE : FLAG_NONE;
            if (caps_only && fl == FLAG_NONE) continue;
            add_move(moves, count, sq, to, 0, fl);
        }
        if (!caps_only) {
            int kingStatus = in_check(b, color);
            (void)kingStatus;
            if (color == WHITE) {
                if ((b->castle & 1) && !(all & (1ULL<<5)) && !(all & (1ULL<<6)) && !in_check(b,WHITE) && !(attacks_to(b,5,BLACK)) && !(attacks_to(b,6,BLACK)))
                    add_move(moves,count,4,6,0,FLAG_CASTLE);
                if ((b->castle & 2) && !(all & (1ULL<<1)) && !(all & (1ULL<<2)) && !(all & (1ULL<<3)) && !in_check(b,WHITE) && !(attacks_to(b,2,BLACK)) && !(attacks_to(b,3,BLACK)))
                    add_move(moves,count,4,2,0,FLAG_CASTLE);
            } else {
                if ((b->castle & 4) && !(all & (1ULL<<61)) && !(all & (1ULL<<62)) && !in_check(b,BLACK) && !(attacks_to(b,61,WHITE)) && !(attacks_to(b,62,WHITE)))
                    add_move(moves,count,60,62,0,FLAG_CASTLE);
                if ((b->castle & 8) && !(all & (1ULL<<57)) && !(all & (1ULL<<58)) && !(all & (1ULL<<59)) && !in_check(b,BLACK) && !(attacks_to(b,58,WHITE)) && !(attacks_to(b,59,WHITE)))
                    add_move(moves,count,60,58,0,FLAG_CASTLE);
            }
        }
    }
}

void gen_moves(const Board *b, Move *moves, int *count) {
    *count = 0;
    static Move buf[4096];
    int pc = 0;
    gen_pseudo(b, buf, &pc, 0);
    for (int i = 0; i < pc; i++) {
        Board tmp = *b;
        Undo u; memset(&u,0,sizeof(u));
        make_move(&tmp, buf[i], &u);
        int original = b->side;
        int ksq = __builtin_ctzll(tmp.pieces[original][KING]);
        uint64_t atk = attacks_to(&tmp, ksq, tmp.side);
        if (!atk) moves[(*count)++] = buf[i];
        /* unmake not needed because tmp copy */
    }
}

void gen_captures(const Board *b, Move *moves, int *count) {
    *count = 0;
    static Move buf[4096];
    int pc = 0;
    gen_pseudo(b, buf, &pc, 1);
    for (int i = 0; i < pc; i++) {
        Board tmp = *b;
        Undo u; memset(&u,0,sizeof(u));
        make_move(&tmp, buf[i], &u);
        int original = b->side;
        int ksq = __builtin_ctzll(tmp.pieces[original][KING]);
        uint64_t atk = attacks_to(&tmp, ksq, tmp.side);
        if (!atk) moves[(*count)++] = buf[i];
    }
}

int make_move(Board *b, Move m, Undo *u) {
    int from = MOVE_FROM(m);
    int to = MOVE_TO(m);
    int promo = MOVE_PROMO(m);
    int flags = MOVE_FLAGS(m);
    memset(u, 0, sizeof(*u));
    u->prev_castle = b->castle;
    u->prev_ep = b->ep;
    u->prev_halfmove = b->halfmove;
    u->prev_fullmove = b->fullmove;
    u->prev_hash = b->hash;
    u->capture_sq = to;

    int color = b->side;
    int opp = color ^ 1;

    int piece = -1;
    for (int p = 0; p < 6; p++) if (b->pieces[color][p] & (1ULL << from)) { piece = p; break; }
    if (piece < 0) return -1;
    u->moved_piece = piece;

    int captured = -1;
    if (flags == FLAG_EP) {
        captured = PAWN;
        u->capture_sq = (color == WHITE) ? to - 8 : to + 8;
    } else {
        for (int p = 0; p < 6; p++) if (b->pieces[opp][p] & (1ULL << to)) { captured = p; break; }
    }
    u->captured = captured;
    u->promo = promo;

    b->pieces[color][piece] &= ~(1ULL << from);

    if (captured >= 0) {
        int capsq = u->capture_sq;
        b->pieces[opp][captured] &= ~(1ULL << capsq);
    }

    int final_piece = piece;
    if (flags == FLAG_PROMO) {
        if (promo == 4) final_piece = QUEEN;
        else if (promo == 3) final_piece = ROOK;
        else if (promo == 2) final_piece = BISHOP;
        else if (promo == 1) final_piece = KNIGHT;
        else final_piece = QUEEN;
        b->pieces[color][final_piece] |= (1ULL << to);
    } else {
        b->pieces[color][piece] |= (1ULL << to);
    }

    if (flags == FLAG_CASTLE) {
        if (color == WHITE) {
            if (to == 6) { b->pieces[WHITE][ROOK] &= ~(1ULL << 7); b->pieces[WHITE][ROOK] |= (1ULL << 5); }
            else { b->pieces[WHITE][ROOK] &= ~(1ULL << 0); b->pieces[WHITE][ROOK] |= (1ULL << 3); }
        } else {
            if (to == 62) { b->pieces[BLACK][ROOK] &= ~(1ULL << 63); b->pieces[BLACK][ROOK] |= (1ULL << 61); }
            else { b->pieces[BLACK][ROOK] &= ~(1ULL << 56); b->pieces[BLACK][ROOK] |= (1ULL << 59); }
        }
    }

    if (piece == KING) {
        if (color == WHITE) b->castle &= ~3;
        else b->castle &= ~12;
    }
    if (piece == ROOK) {
        if (color == WHITE) {
            if (from == 0) b->castle &= ~2;
            if (from == 7) b->castle &= ~1;
        } else {
            if (from == 56) b->castle &= ~8;
            if (from == 63) b->castle &= ~4;
        }
    }
    if (captured == ROOK) {
        int cs = u->capture_sq;
        if (cs == 0) b->castle &= ~2;
        if (cs == 7) b->castle &= ~1;
        if (cs == 56) b->castle &= ~8;
        if (cs == 63) b->castle &= ~4;
    }

    b->ep = -1;
    if (piece == PAWN && (to - from == 16 || from - to == 16)) {
        b->ep = (from + to) / 2;
    }

    /* halfmove */
    if (piece == PAWN || captured >= 0) b->halfmove = 0;
    else b->halfmove++;

    for (int col = 0; col < 2; col++) b->occ[col] = 0;
    for (int p = 0; p < 6; p++) { b->occ[0] |= b->pieces[0][p]; b->occ[1] |= b->pieces[1][p]; }
    b->occ[2] = b->occ[0] | b->occ[1];

    if (color == BLACK) b->fullmove++;
    b->side ^= 1;
    b->hash = hash_board(b);
    if (b->hash_count < 1024) b->hash_history[b->hash_count++] = b->hash;

    return 0;
}

void unmake_move(Board *b, Move m, const Undo *u) {
    (void)m;
    int from = MOVE_FROM(m);
    int to = MOVE_TO(m);
    int color = b->side ^ 1;
    int opp = color ^ 1;
    int piece = u->moved_piece;

    int final_piece = piece;
    if (u->promo) {
        if (u->promo == 4) final_piece = QUEEN;
        else if (u->promo == 3) final_piece = ROOK;
        else if (u->promo == 2) final_piece = BISHOP;
        else if (u->promo == 1) final_piece = KNIGHT;
    }
    if (u->promo) {
        b->pieces[color][final_piece] &= ~(1ULL << to);
    } else {
        b->pieces[color][piece] &= ~(1ULL << to);
    }

    int flags = MOVE_FLAGS(m);
    if (flags == FLAG_CASTLE) {
        if (color == WHITE) {
            if (to == 6) { b->pieces[WHITE][ROOK] &= ~(1ULL<<5); b->pieces[WHITE][ROOK] |= (1ULL<<7); }
            else { b->pieces[WHITE][ROOK] &= ~(1ULL<<3); b->pieces[WHITE][ROOK] |= (1ULL<<0); }
        } else {
            if (to == 62) { b->pieces[BLACK][ROOK] &= ~(1ULL<<61); b->pieces[BLACK][ROOK] |= (1ULL<<63); }
            else { b->pieces[BLACK][ROOK] &= ~(1ULL<<59); b->pieces[BLACK][ROOK] |= (1ULL<<56); }
        }
    }

    b->pieces[color][piece] |= (1ULL << from);

    if (u->captured >= 0) {
        int cs = u->capture_sq;
        b->pieces[opp][u->captured] |= (1ULL << cs);
    }

    b->castle = u->prev_castle;
    b->ep = u->prev_ep;
    b->halfmove = u->prev_halfmove;
    b->fullmove = u->prev_fullmove;
    b->side = color;
    if (b->hash_count > 0) b->hash_count--;
    b->hash = u->prev_hash;

    for (int col = 0; col < 2; col++) b->occ[col] = 0;
    for (int p = 0; p < 6; p++) { b->occ[0] |= b->pieces[0][p]; b->occ[1] |= b->pieces[1][p]; }
    b->occ[2] = b->occ[0] | b->occ[1];
}

int game_over(const Board *b) {
    Move moves[4096]; int mc = 0;
    Board bb = *b;
    gen_moves(&bb, moves, &mc);
    if (mc == 0) {
        if (in_check(b, b->side)) return 1;
        return 2;
    }
    if (b->halfmove >= 100) return 2;
    if (repetition(b)) return 2;
    if (insufficient_material(b)) return 2;
    return 0;
}

int insufficient_material(const Board *b) {
    for (int col = 0; col < 2; col++) {
        if (b->pieces[col][PAWN]) return 0;
        if (b->pieces[col][ROOK]) return 0;
        if (b->pieces[col][QUEEN]) return 0;
    }
    int minors = 0;
    for (int col = 0; col < 2; col++) {
        minors += __builtin_popcountll(b->pieces[col][KNIGHT]);
        minors += __builtin_popcountll(b->pieces[col][BISHOP]);
    }
    if (minors <= 1) return 1;
    if (minors == 2) {
        if (__builtin_popcountll(b->pieces[0][KNIGHT]) + __builtin_popcountll(b->pieces[1][KNIGHT]) == 2 &&
            !b->pieces[0][BISHOP] && !b->pieces[1][BISHOP]) return 1;
    }
    return 0;
}

int repetition(const Board *b) {
    if (b->hash_count < 3) return 0;
    int cnt = 0;
    for (int i = 0; i < b->hash_count - 1; i++) {
        if (b->hash_history[i] == b->hash) cnt++;
    }
    if (cnt >= 2) return 1;
    return 0;
}

void print_board(const Board *b) { (void)b; }
