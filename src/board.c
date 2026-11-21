#include "board.h"
#include "movegen.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

void pos_init_startpos(Position *p) {
    memset(p, 0, sizeof(*p));
    p->bb[0][PT_PAWN]   = 0x000000000000FF00ULL;
    p->bb[0][PT_KNIGHT] = 0x0000000000000042ULL;
    p->bb[0][PT_BISHOP] = 0x0000000000000024ULL;
    p->bb[0][PT_ROOK]   = 0x0000000000000081ULL;
    p->bb[0][PT_QUEEN]  = 0x0000000000000008ULL;
    p->bb[0][PT_KING]   = 0x0000000000000010ULL;
    p->bb[1][PT_PAWN]   = 0x00FF000000000000ULL;
    p->bb[1][PT_KNIGHT] = 0x4200000000000000ULL;
    p->bb[1][PT_BISHOP] = 0x2400000000000000ULL;
    p->bb[1][PT_ROOK]   = 0x8100000000000000ULL;
    p->bb[1][PT_QUEEN]  = 0x0800000000000000ULL;
    p->bb[1][PT_KING]   = 0x1000000000000000ULL;
    p->side = 0;
    p->castle = 0x0F;
    p->ep_square = -1;
    p->halfmove = 0;
    p->fullmove = 1;
}

bool pos_square_ok(int sq) { return sq >= 0 && sq < 64; }

int pos_piece_at(const Position *p, int sq, int *color) {
    if (!pos_square_ok(sq)) return PT_NONE;
    uint64_t bit = 1ULL << sq;
    for (int c = 0; c < 2; c++) {
        for (int pt = 0; pt < 6; pt++) {
            if (p->bb[c][pt] & bit) {
                if (color) *color = c;
                return pt;
            }
        }
    }
    if (color) *color = -1;
    return PT_NONE;
}

uint64_t pos_occupancy(const Position *p, int color) {
    uint64_t o = 0;
    for (int pt = 0; pt < 6; pt++) o |= p->bb[color][pt];
    return o;
}

uint64_t pos_all_occupancy(const Position *p) {
    return pos_occupancy(p,0) | pos_occupancy(p,1);
}

static int fen_piece(char c, int *color, int *pt) {
    *color = (c >= 'a' && c <= 'z') ? COLOR_BLACK : COLOR_WHITE;
    char u = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
    switch (u) {
        case 'P': *pt = PT_PAWN; return 1;
        case 'N': *pt = PT_KNIGHT; return 1;
        case 'B': *pt = PT_BISHOP; return 1;
        case 'R': *pt = PT_ROOK; return 1;
        case 'Q': *pt = PT_QUEEN; return 1;
        case 'K': *pt = PT_KING; return 1;
        default: return 0;
    }
}

int pos_from_fen(Position *p, const char *fen) {
    if (!p || !fen) return -1;
    memset(p, 0, sizeof(*p));
    p->ep_square = -1;
    p->halfmove = 0;
    p->fullmove = 1;
    p->side = 0;
    p->castle = 0;

    const char *s = fen;
    int rank = 7, file = 0;
    while (*s && *s != ' ') {
        if (*s == '/') {
            rank--;
            file = 0;
            if (rank < 0) return -1;
        } else if (*s >= '1' && *s <= '8') {
            int n = *s - '0';
            file += n;
            if (file > 8) return -1;
        } else {
            int color, pt;
            if (!fen_piece(*s, &color, &pt)) return -1;
            if (rank < 0 || file > 7) return -1;
            int sq = rank*8 + file;
            p->bb[color][pt] |= (1ULL << sq);
            file++;
            if (file > 8) return -1;
        }
        s++;
    }
    if (!(rank == 0 && file == 8)) return -1;
    if (*s == ' ') s++;
    if (*s == 'w' || *s == 'W') { p->side = 0; s++; }
    else if (*s == 'b' || *s == 'B') { p->side = 1; s++; }
    else return -1;
    if (*s != ' ') return -1;
    s++;
    if (*s == '-') { s++; }
    else {
        while (*s && *s != ' ') {
            switch (*s) {
                case 'K': p->castle |= 1; break;
                case 'Q': p->castle |= 2; break;
                case 'k': p->castle |= 4; break;
                case 'q': p->castle |= 8; break;
                default: return -1;
            }
            s++;
        }
    }
    if (*s == ' ') s++;
    else if (*s != '\0') return -1;

    if (*s == '-') {
        p->ep_square = -1;
        s++;
    } else if (*s >= 'a' && *s <= 'h') {
        int f = *s - 'a';
        s++;
        if (*s < '1' || *s > '8') return -1;
        int r = *s - '1';
        s++;
        p->ep_square = r*8 + f;
    } else {
        if (*s != '\0') return -1;
    }
    while (*s == ' ') s++;
    if (*s) {
        char *end;
        long hm = strtol(s, &end, 10);
        if (end != s) {
            s = end;
            p->halfmove = (int)hm;
            while (*s == ' ') s++;
            if (*s) {
                long fm = strtol(s, &end, 10);
                if (end != s) {
                    s = end;
                    p->fullmove = (int)fm;
                    if (p->fullmove < 1) p->fullmove = 1;
                }
            }
        }
    }
    if (!(p->bb[0][PT_KING] && p->bb[1][PT_KING])) return -1;
    return 0;
}

int pos_to_fen(const Position *p, char *buf, size_t bufsz) {
    if (!buf || bufsz < 80) return -1;
    size_t pos = 0;
#define APP(c) do { if (pos+1 >= bufsz) return -1; buf[pos++]= (char)(c); } while(0)
    for (int rank = 7; rank >= 0; rank--) {
        int empty = 0;
        for (int file = 0; file < 8; file++) {
            int sq = rank*8+file;
            int color;
            int pt = pos_piece_at(p, sq, &color);
            if (pt == PT_NONE) {
                empty++;
            } else {
                if (empty > 0) {
                    char t = '0' + empty; APP(t); empty = 0;
                }
                char pieces[] = "PNBRQK";
                char ch = pieces[pt];
                if (color == COLOR_BLACK) ch = (char)(ch - 'A' + 'a');
                APP(ch);
            }
        }
        if (empty > 0) {
            char t = '0' + empty; APP(t);
        }
        if (rank > 0) APP('/');
    }
    APP(' ');
    APP(p->side == 0 ? 'w' : 'b');
    APP(' ');
    if (p->castle == 0) APP('-');
    else {
        if (p->castle & 1) APP('K');
        if (p->castle & 2) APP('Q');
        if (p->castle & 4) APP('k');
        if (p->castle & 8) APP('q');
    }
    APP(' ');
    if (p->ep_square < 0) APP('-');
    else {
        int f = p->ep_square % 8;
        int r = p->ep_square / 8;
        char file = 'a' + f;
        char rankc = '1' + r;
        APP(file); APP(rankc);
    }
    char tmp[32];
    snprintf(tmp, sizeof(tmp), " %d %d", p->halfmove, p->fullmove);
    for (int i = 0; tmp[i]; i++) APP(tmp[i]);
    buf[pos] = '\0';
#undef APP
    return 0;
}

int pos_side_to_move(const Position *p) { return p->side; }

static bool is_attacked_by_pawn(const Position *p, int sq, int color) {
    if (color == COLOR_WHITE) {
        int r = sq / 8, c = sq % 8;
        if (r == 0) return false;
        uint64_t mask = 0;
        if (c > 0) mask |= 1ULL << ((r-1)*8 + (c-1));
        if (c < 7) mask |= 1ULL << ((r-1)*8 + (c+1));
        return (p->bb[color][PT_PAWN] & mask) != 0;
    } else {
        int r = sq / 8, c = sq % 8;
        if (r == 7) return false;
        uint64_t mask = 0;
        if (c > 0) mask |= 1ULL << ((r+1)*8 + (c-1));
        if (c < 7) mask |= 1ULL << ((r+1)*8 + (c+1));
        return (p->bb[color][PT_PAWN] & mask) != 0;
    }
}

bool pos_in_check(const Position *p, int color) {
    uint64_t kingbb = p->bb[color][PT_KING];
    if (!kingbb) return true;
    int ksq = __builtin_ctzll(kingbb);
    int opp = color ^ 1;
    if (is_attacked_by_pawn(p, ksq, opp)) return true;
    int kr = ksq/8, kc = ksq%8;
    static const int nOff[8][2] = {{2,1},{2,-1},{-2,1},{-2,-1},{1,2},{1,-2},{-1,2},{-1,-2}};
    for (int i = 0; i < 8; i++) {
        int rr = kr + nOff[i][0];
        int cc = kc + nOff[i][1];
        if (rr<0||rr>7||cc<0||cc>7) continue;
        int sq = rr*8+cc;
        if (p->bb[opp][PT_KNIGHT] & (1ULL<<sq)) return true;
    }
    for (int dr = -1; dr <= 1; dr++) for (int dc = -1; dc <= 1; dc++) {
        if (dr==0 && dc==0) continue;
        int rr = kr+dr, cc = kc+dc;
        if (rr<0||rr>7||cc<0||cc>7) continue;
        int sq = rr*8+cc;
        if (p->bb[opp][PT_KING] & (1ULL<<sq)) return true;
    }
    static const int rDirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    for (int d = 0; d < 4; d++) {
        int rr = kr, cc = kc;
        for (;;) {
            rr += rDirs[d][0]; cc += rDirs[d][1];
            if (rr<0||rr>7||cc<0||cc>7) break;
            int sq = rr*8+cc;
            uint64_t bit = 1ULL<<sq;
            if (p->bb[opp][PT_ROOK] & bit || p->bb[opp][PT_QUEEN] & bit) return true;
            if ((pos_all_occupancy(p) & bit)) break;
        }
    }
    static const int bDirs[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
    for (int d = 0; d < 4; d++) {
        int rr = kr, cc = kc;
        for (;;) {
            rr += bDirs[d][0]; cc += bDirs[d][1];
            if (rr<0||rr>7||cc<0||cc>7) break;
            int sq = rr*8+cc;
            uint64_t bit = 1ULL<<sq;
            if (p->bb[opp][PT_BISHOP] & bit || p->bb[opp][PT_QUEEN] & bit) return true;
            if ((pos_all_occupancy(p) & bit)) break;
        }
    }
    return false;
}

static void remove_piece(Position *p, int color, int pt, int sq) {
    p->bb[color][pt] &= ~(1ULL << sq);
}
static void add_piece(Position *p, int color, int pt, int sq) {
    p->bb[color][pt] |= (1ULL << sq);
}

void pos_make_move(Position *p, int from, int to, int promo, Undo *u) {
    memset(u, 0, sizeof(*u));
    u->from = from;
    u->to = to;
    u->promo = promo;
    u->prev_castle = p->castle;
    u->prev_ep = p->ep_square;
    u->prev_halfmove = p->halfmove;
    u->prev_fullmove = p->fullmove;

    int color = p->side;
    int opp = color ^ 1;
    int moved_piece = PT_NONE;
    for (int pt = 0; pt < 6; pt++) if (p->bb[color][pt] & (1ULL<<from)) { moved_piece = pt; break; }
    u->moved_piece = moved_piece;
    u->moved_color = color;

    int captured_pt = PT_NONE;
    uint64_t tobit = 1ULL << to;
    for (int pt = 0; pt < 6; pt++) {
        if (p->bb[opp][pt] & tobit) { captured_pt = pt; break; }
    }
    u->captured = captured_pt;
    u->captured_color = opp;

    u->was_ep = false;
    if (moved_piece == PT_PAWN && to == p->ep_square && p->ep_square >= 0) {
        u->was_ep = true;
        int cap_sq = (color == COLOR_WHITE) ? to - 8 : to + 8;
        u->captured = PT_PAWN;
        u->captured_color = opp;
        remove_piece(p, opp, PT_PAWN, cap_sq);
    }

    remove_piece(p, color, moved_piece, from);
    if (captured_pt != PT_NONE && !u->was_ep) {
        remove_piece(p, opp, captured_pt, to);
    }

    u->was_promo = false;
    int placed = moved_piece;
    if (promo != 0 && moved_piece == PT_PAWN) {
        placed = promo;
        u->was_promo = true;
    }
    add_piece(p, color, placed, to);

    u->was_castle = false;
    if (moved_piece == PT_KING && (from-to == 2 || to-from == 2)) {
        u->was_castle = true;
        if (to > from) {
            remove_piece(p, color, PT_ROOK, from+3);
            add_piece(p, color, PT_ROOK, from+1);
        } else {
            remove_piece(p, color, PT_ROOK, from-4);
            add_piece(p, color, PT_ROOK, from-1);
        }
    }

    if (moved_piece == PT_KING) {
        if (color == COLOR_WHITE) { p->castle &= ~(1|2); }
        else { p->castle &= ~(4|8); }
    }
    if (moved_piece == PT_ROOK) {
        if (from == 0) p->castle &= ~2;
        if (from == 7) p->castle &= ~1;
        if (from == 56) p->castle &= ~8;
        if (from == 63) p->castle &= ~4;
    }
    if (captured_pt == PT_ROOK && !u->was_ep) {
        if (to == 0) p->castle &= ~2;
        if (to == 7) p->castle &= ~1;
        if (to == 56) p->castle &= ~8;
        if (to == 63) p->castle &= ~4;
    }

    p->ep_square = -1;
    if (moved_piece == PT_PAWN && ((from/8 == 1 && to/8 == 3) || (from/8 == 6 && to/8 == 4))) {
        p->ep_square = (from + to) / 2;
    }

    if (moved_piece == PT_PAWN || captured_pt != PT_NONE || u->was_ep) {
        p->halfmove = 0;
    } else {
        p->halfmove++;
    }
    if (color == COLOR_BLACK) p->fullmove++;
    p->side ^= 1;
}

void pos_unmake_move(Position *p, const Undo *u) {
    p->side ^= 1; /* revert to mover */
    int color = u->moved_color;
    int opp = color ^ 1;
    int placed = u->moved_piece;
    if (u->was_promo) placed = u->promo;
    remove_piece(p, color, placed, u->to);
    add_piece(p, color, u->moved_piece, u->from);
    if (u->was_ep) {
        int cap_sq = (color == COLOR_WHITE) ? u->to - 8 : u->to + 8;
        add_piece(p, opp, PT_PAWN, cap_sq);
    } else if (u->captured != PT_NONE) {
        add_piece(p, u->captured_color, u->captured, u->to);
    }
    if (u->was_castle) {
        if (u->to > u->from) {
            remove_piece(p, color, PT_ROOK, u->from+1);
            add_piece(p, color, PT_ROOK, u->from+3);
        } else {
            remove_piece(p, color, PT_ROOK, u->from-1);
            add_piece(p, color, PT_ROOK, u->from-4);
        }
    }
    p->castle = u->prev_castle;
    p->ep_square = u->prev_ep;
    p->halfmove = u->prev_halfmove;
    p->fullmove = u->prev_fullmove;
}

bool pos_has_legal_moves(Position *p) {
    return movegen_count_legal(p) > 0;
}

int pos_game_result(const Position *p) {
    /* 0 ongoing, 1 mate (side to move mated), 2 stalemate, 3 draw50 */
    Position tmp = *p;
    if (movegen_count_legal(&tmp) > 0) {
        if (p->halfmove >= 100) return 3;
        return 0;
    }
    if (pos_in_check(p, p->side)) return 1;
    return 2;
}