#include "board.h"
#include "eval.h"
#include "search.h"
#include "movegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void move_to_uci(int m, char *out) {
    int from = m & 63;
    int to = (m >> 6) & 63;
    int promo = (m >> 12) & 7;
    out[0] = 'a' + (from % 8);
    out[1] = '1' + (from / 8);
    out[2] = 'a' + (to % 8);
    out[3] = '1' + (to / 8);
    if (promo) {
        char pc = 'q';
        if (promo == PT_KNIGHT) pc = 'n';
        else if (promo == PT_BISHOP) pc = 'b';
        else if (promo == PT_ROOK) pc = 'r';
        else if (promo == PT_QUEEN) pc = 'q';
        out[4] = pc;
        out[5] = '\0';
    } else {
        out[4] = '\0';
    }
}

static void load_weights(WeightSet *ws) {
    const char *path = getenv("CHESS_WEIGHTS");
    if (!path || !*path) path = "weights.bin";
    if (weights_load(ws, path) != 0) {
        weights_defaults(ws);
    }
}

static int apply_uci_move(Position *p, const char *s) {
    if (strlen(s) < 4) return -1;
    int from = (s[0]-'a') + (s[1]-'1')*8;
    int to = (s[2]-'a') + (s[3]-'1')*8;
    int promo = 0;
    if (s[4]) {
        switch (s[4]) {
            case 'n': promo = PT_KNIGHT; break;
            case 'b': promo = PT_BISHOP; break;
            case 'r': promo = PT_ROOK; break;
            case 'q': promo = PT_QUEEN; break;
        }
    }
    if (!pos_square_ok(from) || !pos_square_ok(to)) return -1;
    int moves[512];
    int n = movegen_legal_moves(p, moves, 512);
    for (int i = 0; i < n; i++) {
        if (moves[i] == (from | (to<<6) | (promo<<12))) {
            Undo u;
            pos_make_move(p, from, to, promo, &u);
            return 0;
        }
    }
    return -1;
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--incheck") == 0) {
        Position p;
        if (argc < 3 || pos_from_fen(&p, argv[2]) != 0) pos_init_startpos(&p);
        printf("%d\n", pos_in_check(&p, p.side) ? 1 : 0);
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "--terminal") == 0) {
        Position p;
        if (argc < 3 || pos_from_fen(&p, argv[2]) != 0) pos_init_startpos(&p);
        printf("%d\n", pos_game_result(&p));
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "--apply") == 0) {
        Position p;
        if (argc < 3 || pos_from_fen(&p, argv[2]) != 0) { printf("ERROR\n"); return 1; }
        if (argc < 4 || apply_uci_move(&p, argv[3]) != 0) { printf("ERROR\n"); return 1; }
        char fbuf[256];
        if (pos_to_fen(&p, fbuf, sizeof(fbuf)) != 0) { printf("ERROR\n"); return 1; }
        printf("%s\n", fbuf);
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "--random") == 0) {
        Position p;
        int seed = 1;
        const char *fen = NULL;
        if (argc >= 3) fen = argv[2];
        if (argc >= 4) seed = atoi(argv[3]);
        if (!fen || pos_from_fen(&p, fen) != 0) pos_init_startpos(&p);
        int ms[512];
        int mn = movegen_legal_moves(&p, ms, 512);
        if (mn == 0) { printf("0000\n"); return 0; }
        srand((unsigned)seed);
        int idx = rand() % mn;
        char out[8];
        move_to_uci(ms[idx], out);
        printf("%s\n", out);
        return 0;
    }
    if (argc >= 2) {
        WeightSet ws;
        load_weights(&ws);
        Position p;
        if (pos_from_fen(&p, argv[1]) != 0) pos_init_startpos(&p);
        int bud = 5000;
        const char *et = getenv("CHESS_TIME_MS");
        int md = 6;
        const char *ed = getenv("CHESS_MAX_DEPTH");
        if (et) bud = atoi(et);
        if (ed) md = atoi(ed);
        int sc = 0;
        int m = search_best_move(&p, &ws, md, bud, &sc);
        char out[8];
        if (m < 0) strcpy(out, "0000");
        else move_to_uci(m, out);
        printf("%s\n", out);
        return 0;
    }
    /* UCI loop */
    WeightSet ws;
    load_weights(&ws);
    Position p;
    pos_init_startpos(&p);
    char line[4096];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strncmp(line, "uci", 3)) {
            printf("id name beamchess\nid author opencode\nuciok\n");
            fflush(stdout);
        } else if (!strncmp(line, "isready", 7)) {
            printf("readyok\n");
            fflush(stdout);
        } else if (!strncmp(line, "ucinewgame", 10)) {
            pos_init_startpos(&p);
        } else if (!strncmp(line, "position", 8)) {
            char *ptr = line + 8;
            while (*ptr == ' ') ptr++;
            if (!strncmp(ptr, "startpos", 8)) {
                pos_init_startpos(&p);
                ptr += 8;
                while (*ptr == ' ') ptr++;
            } else if (!strncmp(ptr, "fen", 3)) {
                ptr += 3;
                while (*ptr == ' ') ptr++;
                /* read 6 fields */
                char fen[512]; fen[0]='\0';
                /* collect until moves or end */
                char tmp[4096]; strcpy(tmp, ptr);
                char *movesp = strstr(tmp, " moves");
                if (movesp) *movesp = '\0';
                /* trim newline */
                tmp[strcspn(tmp, "\n")] = '\0';
                strncpy(fen, tmp, sizeof(fen)-1); fen[sizeof(fen)-1]='\0';
                if (pos_from_fen(&p, fen) != 0) pos_init_startpos(&p);
                ptr = strstr(ptr, "moves");
            }
            if (ptr) {
                char *mv = strstr(line, "moves");
                if (mv) {
                    mv += 5;
                    char *tok = strtok(mv, " \n");
                    while (tok) {
                        apply_uci_move(&p, tok);
                        tok = strtok(NULL, " \n");
                    }
                }
            }
        } else if (!strncmp(line, "go", 2)) {
            int bud = 5000;
            const char *et = getenv("CHESS_TIME_MS");
            int md = 6;
            const char *ed = getenv("CHESS_MAX_DEPTH");
            if (et) bud = atoi(et);
            if (ed) md = atoi(ed);
            int sc = 0;
            int m = search_best_move(&p, &ws, md, bud, &sc);
            char out[8];
            if (m < 0) strcpy(out, "0000");
            else move_to_uci(m, out);
            printf("bestmove %s\n", out);
            fflush(stdout);
        } else if (!strncmp(line, "quit", 4)) {
            break;
        }
    }
    return 0;
}
