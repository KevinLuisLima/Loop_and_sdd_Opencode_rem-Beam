#include "position.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Zobrist */
static uint64_t zp[2][6][64];
static uint64_t zside;
static uint64_t zcastle[16];
static uint64_t zep[64];
static int zobrist_ready = 0;

static uint64_t splitmix(uint64_t *s) {
    uint64_t z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111B9ULL;
    return z ^ (z >> 31);
}
static void zobrist_init(void) {
    if (zobrist_ready) return;
    uint64_t s = 1234567ULL;
    for (int c=0;c<2;c++) for (int t=0;t<6;t++) for (int sq=0;sq<64;sq++) zp[c][t][sq]=splitmix(&s);
    zside = splitmix(&s);
    for (int i=0;i<16;i++) zcastle[i]=splitmix(&s);
    for (int i=0;i<64;i++) zep[i]=splitmix(&s);
    zobrist_ready=1;
}

int code_color(int code) {
    if (code >= BKING) return BT;
    return WT;
}
int code_type_idx(int code) {
    switch (code) {
        case WPAWN: case BPAWN: return 0;
        case WKNIGHT: case BKNIGHT: return 1;
        case WBISHOP: case BBISHOP: return 2;
        case WROOK: case BROOK: return 3;
        case WQUEEN: case BQUEEN: return 4;
        case WKING: case BKING: return 5;
    }
    return 0;
}

static uint64_t compute_hash(Position *pos) {
    zobrist_init();
    uint64_t h = 0;
    for (int sq=0;sq<64;sq++) {
        int c = pos->board[sq];
        if (c != EMPTY) {
            int col = code_color(c);
            int ti = code_type_idx(c);
            h ^= zp[col][ti][sq];
        }
    }
    if (pos->side == BT) h ^= zside;
    h ^= zcastle[pos->castling & 15];
    if (pos->ep >= 0) h ^= zep[pos->ep];
    return h;
}

void position_init(Position *pos) {
    memset(pos, 0, sizeof(*pos));
    zobrist_init();
    /* startpos */
    const char *fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    char err[64];
    position_from_fen(pos, fen, err, sizeof(err));
}

int position_from_fen(Position *pos, const char *fen, char *err, size_t errsz) {
    memset(pos, 0, sizeof(*pos));
    zobrist_init();
    pos->ep = -1;
    pos->castling = 0;
    pos->halfmove = 0;
    pos->fullmove = 1;
    pos->side = WT;

    /* copy fen */
    char buf[512];
    strncpy(buf, fen, sizeof(buf)-1);
    buf[sizeof(buf)-1]=0;

    char *tokens[6];
    int ntok=0;
    char *p = buf;
    while (*p && ntok < 6) {
        while (*p == ' ') p++;
        if (*p == 0) break;
        tokens[ntok++] = p;
        while (*p && *p != ' ') p++;
        if (*p) { *p = 0; p++; }
    }
    /* normalize: if fewer tokens, error */
    if (ntok < 1) {
        snprintf(err, errsz, "FEN empty");
        return 1;
    }

    /* piece placement */
    int kings[2] = {0,0};
    int sq = 56;
    char *place = tokens[0];
    int idx = 0;
    while (place[idx] != 0) {
        char ch = place[idx++];
        if (ch == '/') {
            sq -= 16; /* move down one rank: from current file end to next rank start */
            /* sq currently at end-of-rank+? adjust: typical */
            /* We'll recompute more robustly below instead */
        } else if (ch >= '1' && ch <= '8') {
            int n = ch - '0';
            for (int i=0;i<n;i++) {
                if (sq < 0 || sq > 63) { snprintf(err,errsz,"bad square"); return 1; }
                pos->board[sq] = EMPTY;
                sq++;
            }
        } else {
            int code = EMPTY;
            switch(ch) {
                case 'P': code=WPAWN; break;
                case 'N': code=WKNIGHT; break;
                case 'B': code=WBISHOP; break;
                case 'R': code=WROOK; break;
                case 'Q': code=WQUEEN; break;
                case 'K': code=WKING; break;
                case 'p': code=BPAWN; break;
                case 'n': code=BKNIGHT; break;
                case 'b': code=BBISHOP; break;
                case 'r': code=BROOK; break;
                case 'q': code=BQUEEN; break;
                case 'k': code=BKING; break;
                default: snprintf(err,errsz,"bad piece char"); return 1;
            }
            if (sq < 0 || sq > 63) { snprintf(err,errsz,"bad square"); return 1; }
            pos->board[sq] = code;
            if (code == WKING) kings[0]++;
            if (code == BKING) kings[1]++;
            int col = code_color(code);
            int ti = code_type_idx(code);
            pos->pieces[col][ti] |= 1ULL << sq;
            pos->occ[col] |= 1ULL << sq;
            pos->occ_all |= 1ULL << sq;
            sq++;
        }
        /* handle slash advancement */
    }
    /* The above slash handling incremental is buggy; redo cleaner. */
    /* We'll instead parse with a cleaner routine to avoid slash confusion. */
    /* Reset and parse again properly. */
    memset(pos->board,0,sizeof(pos->board));
    memset(pos->pieces,0,sizeof(pos->pieces));
    memset(pos->occ,0,sizeof(pos->occ));
    pos->occ_all=0;
    kings[0]=kings[1]=0;

    int rank=7;
    int file=0;
    int i=0;
    while (place[i] != 0 && place[i] != ' ') {
        char ch = place[i++];
        if (ch == '/') {
            if (file != 8) { snprintf(err,errsz,"incomplete rank"); return 1; }
            rank--;
            file=0;
            if (rank < 0) { snprintf(err,errsz,"too many ranks"); return 1; }
        } else if (ch >= '1' && ch <= '8') {
            int n = ch-'0';
            for (int d=0;d<n;d++) {
                if (file >= 8) { snprintf(err,errsz,"rank overflow"); return 1; }
                int s = rank*8 + file;
                file++;
                pos->board[s]=EMPTY;
            }
        } else {
            int code=EMPTY;
            switch(ch) {
                case 'P': code=WPAWN; break;
                case 'N': code=WKNIGHT; break;
                case 'B': code=WBISHOP; break;
                case 'R': code=WROOK; break;
                case 'Q': code=WQUEEN; break;
                case 'K': code=WKING; break;
                case 'p': code=BPAWN; break;
                case 'n': code=BKNIGHT; break;
                case 'b': code=BBISHOP; break;
                case 'r': code=BROOK; break;
                case 'q': code=BQUEEN; break;
                case 'k': code=BKING; break;
                default: snprintf(err,errsz,"bad piece char"); return 1;
            }
            if (file >= 8) { snprintf(err,errsz,"rank overflow"); return 1; }
            int s = rank*8 + file;
            file++;
            pos->board[s]=code;
            if (code==WKING) kings[0]++;
            if (code==BKING) kings[1]++;
            int col=code_color(code);
            int ti=code_type_idx(code);
            pos->pieces[col][ti] |= 1ULL<<s;
            pos->occ[col] |= 1ULL<<s;
            pos->occ_all |= 1ULL<<s;
        }
    }
    if (rank != 0 || file != 8) { snprintf(err,errsz,"incomplete board"); return 1; }
    if (kings[0] != 1 || kings[1] != 1) { snprintf(err,errsz,"king count invalid"); return 1; }

    /* side to move */
    if (ntok >= 2) {
        if (tokens[1][0] == 'w') pos->side = WT;
        else if (tokens[1][0] == 'b') pos->side = BT;
        else { snprintf(err,errsz,"bad side"); return 1; }
    }

    /* castling */
    if (ntok >= 3) {
        if (tokens[2][0] != '-') {
            for (int j=0; tokens[2][j]; j++) {
                switch (tokens[2][j]) {
                    case 'K': pos->castling |= 1; break;
                    case 'Q': pos->castling |= 2; break;
                    case 'k': pos->castling |= 4; break;
                    case 'q': pos->castling |= 8; break;
                    default: snprintf(err,errsz,"bad castling"); return 1;
                }
            }
        }
    }

    /* ep square */
    if (ntok >= 4) {
        if (tokens[3][0] != '-') {
            if (strlen(tokens[3]) < 2) { snprintf(err,errsz,"bad ep"); return 1; }
            int f = tokens[3][0]-'a';
            int r = tokens[3][1]-'1';
            if (f<0||f>7||r<0||r>7) { snprintf(err,errsz,"bad ep"); return 1; }
            pos->ep = r*8 + f;
        } else pos->ep = -1;
    } else pos->ep = -1;

    /* halfmove, fullmove */
    if (ntok >= 5) pos->halfmove = atoi(tokens[4]);
    if (ntok >= 6) pos->fullmove = atoi(tokens[5]);
    if (pos->halfmove < 0) pos->halfmove = 0;

    /* validate ep side correctness? optional */

    pos->hash = compute_hash(pos);
    /* history */
    pos->hist_count = 0;
    pos->hist[pos->hist_count++] = pos->hash;
    pos->undo_count = 0;
    pos->ply = 0;

    return 0;
}


void position_to_fen(Position *pos, char *buf, size_t sz) {
    char tmp[256];
    int ti = 0;
    for (int rank=7; rank>=0; rank--) {
        int empty=0;
        for (int file=0; file<8; file++) {
            int sq = rank*8+file;
            int c = pos->board[sq];
            if (c == EMPTY) {
                empty++;
            } else {
                if (empty > 0) { tmp[ti++] = '0' + empty; empty=0; }
                    /* mapping codes to chars */
                char pc;
                switch (c) {
                    case WPAWN: pc='P'; break;
                    case WKNIGHT: pc='N'; break;
                    case WBISHOP: pc='B'; break;
                    case WROOK: pc='R'; break;
                    case WQUEEN: pc='Q'; break;
                    case WKING: pc='K'; break;
                    case BPAWN: pc='p'; break;
                    case BKNIGHT: pc='n'; break;
                    case BBISHOP: pc='b'; break;
                    case BROOK: pc='r'; break;
                    case BQUEEN: pc='q'; break;
                    case BKING: pc='k'; break;
                    default: pc='?';
                }
                tmp[ti++] = pc;
            }
        }
        if (empty > 0) tmp[ti++] = '0' + empty;
        if (rank > 0) tmp[ti++] = '/';
    }
    tmp[ti]=0;

    char castle[5];
    int ci=0;
    if (pos->castling & 1) castle[ci++]='K';
    if (pos->castling & 2) castle[ci++]='Q';
    if (pos->castling & 4) castle[ci++]='k';
    if (pos->castling & 8) castle[ci++]='q';
    if (ci==0) castle[ci++]='-';
    castle[ci]=0;

    char ep[3];
    if (pos->ep >= 0) {
        ep[0] = 'a' + (pos->ep % 8);
        ep[1] = '1' + (pos->ep / 8);
        ep[2] = 0;
    } else {
        ep[0]='-'; ep[1]=0;
    }

    snprintf(buf, sz, "%s %c %s %s %d %d", tmp, pos->side==WT?'w':'b', castle, ep, pos->halfmove, pos->fullmove);
}

uint64_t position_hash(Position *pos) {
    if (pos->hash == 0 && pos->occ_all != 0) pos->hash = compute_hash(pos);
    return pos->hash;
}

void position_reset_history(Position *pos) {
    pos->hist_count = 0;
    pos->hist[pos->hist_count++] = pos->hash;
}

/* attack generation */
static int on_board(int r,int c){ return r>=0&&r<8&&c>=0&&c<8;}

uint64_t attacks_to(Position *pos, int sq, int side) {
    uint64_t mask = 0;
    /* pawn attacks: squares from which a pawn of side would attack sq */
    if (side == WT) {
        /* pawns located one rank below sq, adjacent files */
        int r=sq/8, c=sq%8;
        if (r>0) {
            if (c>0) {
                int ps = (r-1)*8 + (c-1);
                if (pos->board[ps]==WPAWN) mask |= 1ULL<<ps;
            }
            if (c<7) {
                int ps = (r-1)*8 + (c+1);
                if (pos->board[ps]==WPAWN) mask |= 1ULL<<ps;
            }
        }
    } else {
        int r=sq/8, c=sq%8;
        if (r<7) {
            if (c>0) {
                int ps=(r+1)*8+(c-1);
                if (pos->board[ps]==BPAWN) mask |= 1ULL<<ps;
            }
            if (c<7) {
                int ps=(r+1)*8+(c+1);
                if (pos->board[ps]==BPAWN) mask |= 1ULL<<ps;
            }
        }
    }
    /* knights */
    uint64_t nm = bb_knight(sq);
    uint64_t opp_knights = (side==WT)? pos->pieces[WT][1] : pos->pieces[BT][1];
    mask |= (nm & opp_knights);
    /* king */
    uint64_t km = bb_king(sq);
    uint64_t opp_king = (side==WT)? pos->pieces[WT][5] : pos->pieces[BT][5];
    mask |= (km & opp_king);
    /* sliding from sq: bishops/queens diagonal, rooks/queens orth */
    /* For attackers of side, check pieces */
    /* We'll iterate directions and stop at first piece; if it is opponent sliding piece, add */
    int r=sq/8,c=sq%8;
    const int rookd[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
    const int bishopd[4][2]={{1,1},{1,-1},{-1,1},{-1,-1}};
    /* rooks/queens orthogonal */
    for (int d=0; d<4; d++) {
        int nr=r+rookd[d][0], nc=c+rookd[d][1];
        while (on_board(nr,nc)) {
            int s=nr*8+nc;
            int pc = pos->board[s];
            if (pc != EMPTY) {
                if ((side==WT && (pc==WROOK||pc==WQUEEN)) ||
                    (side==BT && (pc==BROOK||pc==BQUEEN))) mask |= 1ULL<<s;
                break;
            }
            nr += rookd[d][0]; nc += rookd[d][1];
        }
    }
    for (int d=0; d<4; d++) {
        int nr=r+bishopd[d][0], nc=c+bishopd[d][1];
        while (on_board(nr,nc)) {
            int s=nr*8+nc;
            int pc = pos->board[s];
            if (pc != EMPTY) {
                if ((side==WT && (pc==WBISHOP||pc==WQUEEN)) ||
                    (side==BT && (pc==BBISHOP||pc==BQUEEN))) mask |= 1ULL<<s;
                break;
            }
            nr += bishopd[d][0]; nc += bishopd[d][1];
        }
    }
    return mask;
}

int in_check(Position *pos, int side) {
    /* side's king square */
    int king_code = (side==WT)? WKING : BKING;
    int ksq=-1;
    for (int s=0;s<64;s++) if (pos->board[s]==king_code) { ksq=s; break; }
    if (ksq <0) return 0;
    int opp = side^1;
    return (attacks_to(pos, ksq, opp) != 0);
}

/* generate pseudo moves into moves array, return count */
int generate_pseudo_moves(Position *pos, Move *moves) {
    int side = pos->side;
    int count = 0;
    for (int from=0; from<64; from++) {
        int piece = pos->board[from];
        if (piece == EMPTY) continue;
        if (code_color(piece) != side) continue;
        int ti = code_type_idx(piece);
        if (ti == 0) { /* pawn */
            int dir = (side==WT)? 1 : -1;
            int start_rank = (side==WT)? 1 : 6;
            /* single push */
            int to = from + dir*8;
            if (to >=0 && to<64 && (from/8)!=(side==WT?7:0)) {
                if (pos->board[to]==EMPTY) {
                    /* promotion check */
                    if ((side==WT && to/8==7) || (side==BT && to/8==0)) {
                        moves[count++] = (from) | (to<<6) | (1<<12); // Q
                        moves[count++] = (from) | (to<<6) | (2<<12); // R
                        moves[count++] = (from) | (to<<6) | (3<<12); // B
                        moves[count++] = (from) | (to<<6) | (4<<12); // N
                    } else {
                        moves[count++] = from | (to<<6);
                    }
                    /* double push */
                    if (from/8 == start_rank) {
                        int to2 = from + dir*16;
                        if (pos->board[to2]==EMPTY) {
                            moves[count++] = from | (to2<<6);
                        }
                    }
                }
            }
            /* captures */
            for (int dc=-1; dc<=1; dc+=2) {
                int f = from%8;
                int nf = f + dc;
                int nr = from/8 + dir;
                if (nf<0||nf>7||nr<0||nr>7) continue;
                int to2 = nr*8+nf;
                int target = pos->board[to2];
                /* normal capture */
                if (target != EMPTY && code_color(target) != side) {
                    if ((side==WT && nr==7) || (side==BT && nr==0)) {
                        moves[count++] = from | (to2<<6) | (1<<12);
                        moves[count++] = from | (to2<<6) | (2<<12);
                        moves[count++] = from | (to2<<6) | (3<<12);
                        moves[count++] = from | (to2<<6) | (4<<12);
                    } else {
                        moves[count++] = from | (to2<<6);
                    }
                }
                /* en passant */
                if (to2 == pos->ep && target == EMPTY) {
                    moves[count++] = from | (to2<<6) | (0<<12); /* ep move flagged by equality? need flag. We'll mark as capture in make based on ep */
                    /* mark with high bit? store separately impossible. We'll treat any pawn move to ep square as EP; make handles. */
                }
            }
        } else if (ti == 1) { /* knight */
            uint64_t bb = bb_knight(from);
            uint64_t targets = bb & ~((side==WT)? pos->occ[WT] : pos->occ[BT]);
            while (targets) {
                int to = bb_lsb(targets); targets &= targets-1;
                moves[count++] = from | (to<<6);
            }
        } else if (ti == 2 || ti == 4) { /* bishop/queen */
            /* use queen slides for both directions bishop */
            uint64_t bb;
            if (ti == 2) bb = bb_bishop(from, pos->occ_all);
            else bb = bb_queen(from, pos->occ_all);
            uint64_t targets = bb & ~((side==WT)? pos->occ[WT] : pos->occ[BT]);
            while (targets) {
                int to = bb_lsb(targets); targets &= targets-1;
                moves[count++] = from | (to<<6);
            }
        } else if (ti == 3) { /* rook */
            uint64_t bb = bb_rook(from, pos->occ_all);
            uint64_t targets = bb & ~((side==WT)? pos->occ[WT] : pos->occ[BT]);
            while (targets) {
                int to = bb_lsb(targets); targets &= targets-1;
                moves[count++] = from | (to<<6);
            }
        } else if (ti == 5) { /* king */
            /* normal moves */
            uint64_t bb = bb_king(from);
            uint64_t targets = bb & ~((side==WT)? pos->occ[WT] : pos->occ[BT]);
            while (targets) {
                int to = bb_lsb(targets); targets &= targets-1;
                moves[count++] = from | (to<<6);
            }
            /* castling: king on start, rights */
            if (side == WT) {
                if (from == 4) {
                    if ((pos->castling & 1) && pos->board[5]==EMPTY && pos->board[6]==EMPTY &&
                        pos->board[7]==WROOK &&
                        attacks_to(pos,4,BT)==0 && attacks_to(pos,5,BT)==0 && attacks_to(pos,6,BT)==0) {
                        moves[count++] = 4 | (6<<6);
                    }
                    if ((pos->castling & 2) && pos->board[3]==EMPTY && pos->board[2]==EMPTY && pos->board[1]==EMPTY &&
                        pos->board[0]==WROOK &&
                        attacks_to(pos,4,BT)==0 && attacks_to(pos,3,BT)==0 && attacks_to(pos,2,BT)==0) {
                        moves[count++] = 4 | (2<<6);
                    }
                }
            } else {
                if (from == 60) {
                    if ((pos->castling & 4) && pos->board[61]==EMPTY && pos->board[62]==EMPTY &&
                        pos->board[63]==BROOK &&
                        attacks_to(pos,60,WT)==0 && attacks_to(pos,61,WT)==0 && attacks_to(pos,62,WT)==0) {
                        moves[count++] = 60 | (62<<6);
                    }
                    if ((pos->castling & 8) && pos->board[59]==EMPTY && pos->board[58]==EMPTY && pos->board[57]==EMPTY &&
                        pos->board[56]==BROOK &&
                        attacks_to(pos,60,WT)==0 && attacks_to(pos,59,WT)==0 && attacks_to(pos,58,WT)==0) {
                        moves[count++] = 60 | (58<<6);
                    }
                }
            }
        }
    }
    /* queen as sliding already handled by queen case; but queen ti==4 handled above yes */
    return count;
}

/* Wait: piece type idx mapping: queen=4, bishop=2, rook=3, knight=1, king=5, pawn=0. In queen case ti==4 uses bb_queen. Good. But above bishop/queen condition includes ti==2 or 4; for ti==4 uses bb_queen? Need adjust: current sets bb for ti==2 bishop, else queen. Good. */



int make_move(Position *pos, Move m, Undo *u) {
    int from = MOVE_FROM(m);
    int to = MOVE_TO(m);
    int prom = MOVE_PROM(m);
    int side = pos->side;
    int piece = pos->board[from];
    u->move = m;
    u->piece = piece;
    u->side = side;
    u->castling = pos->castling;
    u->ep = pos->ep;
    u->halfmove = pos->halfmove;
    u->hash = pos->hash;
    u->fullmove = pos->fullmove;
    u->promoted_piece = EMPTY;
    u->capture_sq = to;
    int captured = pos->board[to];
    if (code_type_idx(piece)==0 && to == pos->ep && pos->board[to]==EMPTY) {
        int cap_sq = (side==WT)? to-8 : to+8;
        captured = pos->board[cap_sq];
        u->capture_sq = cap_sq;
    }
    u->captured = captured;

    /* remove piece from from */
    int col = code_color(piece);
    int ti = code_type_idx(piece);
    pos->pieces[col][ti] &= ~(1ULL<<from);
    pos->occ[col] &= ~(1ULL<<from);
    pos->occ_all &= ~(1ULL<<from);
    pos->board[from] = EMPTY;

    /* handle capture */
    if (captured != EMPTY) {
        int ccol = code_color(captured);
        int cti = code_type_idx(captured);
        int csq = u->capture_sq;
        pos->pieces[ccol][cti] &= ~(1ULL<<csq);
        pos->occ[ccol] &= ~(1ULL<<csq);
        pos->occ_all &= ~(1ULL<<csq);
        pos->board[csq] = EMPTY;
    }

    /* promotion */
    int new_piece = piece;
    if (code_type_idx(piece)==0 && ( (side==WT && to/8==7) || (side==BT && to/8==0) )) {
        int prom_code = piece; /* default queen */
        if (prom == 1) prom_code = (side==WT)? WQUEEN : BQUEEN;
        else if (prom == 2) prom_code = (side==WT)? WROOK : BROOK;
        else if (prom == 3) prom_code = (side==WT)? WBISHOP : BBISHOP;
        else if (prom == 4) prom_code = (side==WT)? WKNIGHT : BKNIGHT;
        else {
            /* if no prom flag, queen */
            prom_code = (side==WT)? WQUEEN : BQUEEN;
        }
        new_piece = prom_code;
        u->promoted_piece = new_piece;
    }

    /* place piece */
    int ncol = code_color(new_piece);
    int nti = code_type_idx(new_piece);
    pos->board[to] = new_piece;
    pos->pieces[ncol][nti] |= 1ULL<<to;
    pos->occ[ncol] |= 1ULL<<to;
    pos->occ_all |= 1ULL<<to;

    /* castling rook move handled below */
    /* We'll instead handle before toggling side: if king moved 2, move rook and update bitboards. */
    /* Fix: revert the messy block by handling now again? Let's not; instead restructure: handle castling earlier. 
       To keep code clean, we will move castling rook handling here explicitly. Since new_piece placed, king at to. */
    if (code_type_idx(piece)==5 && (to - from == 2 || from - to == 2)) {
        /* king-side */
        if (to > from) {
            int rook_from = (side==WT)? 7 : 63;
            int rook_to = to - 1;
            int rook_code = (side==WT)? WROOK : BROOK;
            if (pos->board[rook_to] == EMPTY) {
                pos->board[rook_from] = EMPTY;
                pos->board[rook_to] = rook_code;
                int rc = code_color(rook_code);
                int rti = code_type_idx(rook_code);
                pos->pieces[rc][rti] &= ~(1ULL<<rook_from);
                pos->pieces[rc][rti] |= 1ULL<<rook_to;
                pos->occ[rc] |= 0; /* ensure bits */
                pos->occ[rc] &= ~(1ULL<<rook_from);
                pos->occ[rc] |= 1ULL<<rook_to;
                pos->occ_all &= ~(1ULL<<rook_from);
                pos->occ_all |= 1ULL<<rook_to;
            }
        } else {
            int rook_from = (side==WT)? 0 : 56;
            int rook_to = to + 1;
            int rook_code = (side==WT)? WROOK : BROOK;
            if (pos->board[rook_to] == EMPTY) {
                pos->board[rook_from] = EMPTY;
                pos->board[rook_to] = rook_code;
                int rc = code_color(rook_code);
                int rti = code_type_idx(rook_code);
                pos->pieces[rc][rti] &= ~(1ULL<<rook_from);
                pos->pieces[rc][rti] |= 1ULL<<rook_to;
                pos->occ[rc] &= ~(1ULL<<rook_from);
                pos->occ[rc] |= 1ULL<<rook_to;
                pos->occ_all &= ~(1ULL<<rook_from);
                pos->occ_all |= 1ULL<<rook_to;
            }
        }
    }

    /* update castling rights */
    if (piece == WKING) { pos->castling &= ~(1|2); }
    if (piece == BKING) { pos->castling &= ~(4|8); }
    if (from == 7 || to == 7) pos->castling &= ~1;
    if (from == 0 || to == 0) pos->castling &= ~2;
    if (from == 63 || to == 63) pos->castling &= ~4;
    if (from == 56 || to == 56) pos->castling &= ~8;

    /* ep square update */
    pos->ep = -1;
    if (code_type_idx(piece)==0) {
        if ((side==WT && from/8==1 && to/8==3) || (side==BT && from/8==6 && to/8==4)) {
            pos->ep = (from + to) / 2;
        }
    }

    /* halfmove clock */
    if (code_type_idx(piece)==0 || captured != EMPTY) pos->halfmove = 0;
    else pos->halfmove++;

    /* fullmove */
    if (side == BT) pos->fullmove++;

    /* side toggle */
    pos->side ^= 1;

    /* recompute hash */
    pos->hash = compute_hash(pos);

    /* repetition history */
    if (pos->hist_count < HISTORY_SIZE) {
        pos->hist[pos->hist_count++] = pos->hash;
    }

    pos->ply++;

    /* occ consistency fix for castling already; ensure occ arrays correct: rebuild occ from pieces? */
    /* For safety, recompute occ[side] maybe */
    return 0;
}

void unmake_move(Position *pos, Undo *u) {
    /* remove pos from history first */
    if (pos->hist_count > 0) pos->hist_count--;
    pos->ply--;

    /* toggle side back */
    pos->side = u->side;

    /* fullmove/halfmove restore */
    pos->castling = u->castling;
    pos->ep = u->ep;
    pos->halfmove = u->halfmove;
    pos->fullmove = u->fullmove;

    int from = MOVE_FROM(u->move);
    int to = MOVE_TO(u->move);

    /* remove piece at to (and castling rook if needed) */
    /* handle castling rook unmove */
    if (code_type_idx(u->piece)==5 && (to - from == 2 || from - to == 2)) {
        int side = u->side;
        if (to > from) {
            int rook_from = (side==WT)? 7 : 63;
            int rook_to = to - 1;
            int rook_code = (side==WT)? WROOK : BROOK;
            pos->board[rook_to] = EMPTY;
            pos->board[rook_from] = rook_code;
            int rc = code_color(rook_code);
            int rti = code_type_idx(rook_code);
            pos->pieces[rc][rti] &= ~(1ULL<<rook_to);
            pos->pieces[rc][rti] |= 1ULL<<rook_from;
            pos->occ[rc] &= ~(1ULL<<rook_to);
            pos->occ[rc] |= 1ULL<<rook_from;
            pos->occ_all &= ~(1ULL<<rook_to);
            pos->occ_all |= 1ULL<<rook_from;
        } else {
            int rook_from = (side==WT)? 0 : 56;
            int rook_to = to + 1;
            int rook_code = (side==WT)? WROOK : BROOK;
            pos->board[rook_to] = EMPTY;
            pos->board[rook_from] = rook_code;
            int rc = code_color(rook_code);
            int rti = code_type_idx(rook_code);
            pos->pieces[rc][rti] &= ~(1ULL<<rook_to);
            pos->pieces[rc][rti] |= 1ULL<<rook_from;
            pos->occ[rc] &= ~(1ULL<<rook_to);
            pos->occ[rc] |= 1ULL<<rook_from;
            pos->occ_all &= ~(1ULL<<rook_to);
            pos->occ_all |= 1ULL<<rook_from;
        }
    }

    /* remove new piece at to */
    int piece_at_to = pos->board[to];
    /* if promotion, piece_at_to is promoted; remove promoted */
    int ncol = code_color(piece_at_to);
    int nti = code_type_idx(piece_at_to);
    pos->pieces[ncol][nti] &= ~(1ULL<<to);
    pos->occ[ncol] &= ~(1ULL<<to);
    pos->occ_all &= ~(1ULL<<to);
    pos->board[to] = EMPTY;

    /* restore captured piece */
    if (u->captured != EMPTY) {
        int csq = u->capture_sq;
        int ccol = code_color(u->captured);
        int cti = code_type_idx(u->captured);
        pos->board[csq] = u->captured;
        pos->pieces[ccol][cti] |= 1ULL<<csq;
        pos->occ[ccol] |= 1ULL<<csq;
        pos->occ_all |= 1ULL<<csq;
    }

    /* restore original piece at from */
    int orig = u->piece;
    int ocol = code_color(orig);
    int oti = code_type_idx(orig);
    pos->board[from] = orig;
    pos->pieces[ocol][oti] |= 1ULL<<from;
    pos->occ[ocol] |= 1ULL<<from;
    pos->occ_all |= 1ULL<<from;

    pos->hash = u->hash;
}

int generate_legal_moves(Position *pos, Move *moves) {
    Move pseudo[MAX_MOVES];
    int n = generate_pseudo_moves(pos, pseudo);
    int count = 0;
    for (int i=0;i<n;i++) {
        Undo u;
        make_move(pos, pseudo[i], &u);
        int prev_side = u.side;
        int legal = !in_check(pos, prev_side);
        unmake_move(pos, &u);
        if (legal) moves[count++] = pseudo[i];
    }
    return count;
}

int move_is_legal(Position *pos, Move m) {
    Undo u;
    make_move(pos, m, &u);
    int prev = u.side;
    int ok = !in_check(pos, prev);
    unmake_move(pos, &u);
    return ok;
}

int is_terminal(Position *pos, int *result) {
    /* result: 0 draw, 1 side-to-move win? We'll define returns outcome for game flow externally */
    /* Check 50-move */
    if (pos->halfmove >= 100) { if (result) *result = 0; return 1; }
    /* insufficient material */
    if (insufficient_material(pos)) { if (result) *result = 0; return 1; }
    /* repetition */
    int occ=0;
    for (int i=0;i<pos->hist_count;i++) if (pos->hist[i]==pos->hash) occ++;
    if (occ >= 3) { if (result) *result = 0; return 1; }
    /* legal moves */
    Move mvs[MAX_MOVES];
    int n = generate_legal_moves(pos, mvs);
    if (n == 0) {
        if (in_check(pos, pos->side)) {
            /* checkmate: side to move loses */
            if (result) *result = 1; /* loss for side to move */
        } else {
            if (result) *result = 0;
        }
        return 1;
    }
    return 0;
}

int insufficient_material(Position *pos) {
    /* count pieces */
    int cnt[2][6] = {0};
    for (int c=0;c<2;c++) for (int t=0;t<6;t++) {
        cnt[c][t] = bb_popcount(pos->pieces[c][t]);
    }
    /* remove kings (1 each assumed) */
    /* total non-king material count */
    int total = 0;
    for (int c=0;c<2;c++) for (int t=0;t<5;t++) total += cnt[c][t];
    if (total == 0) return 1;
    /* king + single minor vs king */
    if (total == 1) {
        for (int c=0;c<2;c++) {
            if (cnt[c][1]+cnt[c][2] == 1) {
                /* other side no pieces */
                int other = (c^1);
                int ototal = 0;
                for (int t=0;t<5;t++) ototal += cnt[other][t];
                if (ototal == 0) return 1;
            }
        }
    }
    return 0;
}
