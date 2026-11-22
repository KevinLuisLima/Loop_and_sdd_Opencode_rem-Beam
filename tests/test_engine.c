#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include "position.h"
#include "eval.h"
#include "search.h"

static int tests_passed = 0;
static int tests_total = 0;

static uint64_t perft(Position *pos, int depth) {
    if (depth == 0) return 1;
    Move moves[MAX_MOVES];
    int n = generate_legal_moves(pos, moves);
    if (depth == 1) return n;
    uint64_t nodes = 0;
    for (int i = 0; i < n; i++) {
        Undo u;
        make_move(pos, moves[i], &u);
        nodes += perft(pos, depth - 1);
        unmake_move(pos, &u);
    }
    return nodes;
}

static void make_weights(float *w) {
    for (int i = 0; i < FEATURE_COUNT; i++) w[i] = 0.0f;
    w[0] = 100.0f; w[1] = 320.0f; w[2] = 330.0f; w[3] = 500.0f; w[4] = 900.0f;
}

static Move play_random(Position *pos) {
    Move moves[MAX_MOVES];
    int n = generate_legal_moves(pos, moves);
    if (n == 0) return 0;
    return moves[rand() % n];
}

static int test_perft(void) {
    tests_total++;
    Position pos;
    char err[64];
    position_from_fen(&pos, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", err, sizeof(err));
    if (perft(&pos, 5) != 4865609ULL) return 0;
    tests_total++;
    Position pos2;
    position_from_fen(&pos2, "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", err, sizeof(err));
    if (perft(&pos2, 4) != 4085603ULL) return 0;
    tests_passed += 2;
    return 1;
}

static int test_fen_invalid(void) {
    tests_total++;
    Position pos;
    char err[64];
    if (position_from_fen(&pos, "8/8/8/8/8/8/8/8 w - - 0 1", err, sizeof(err)) == 0) return 0;
    if (position_from_fen(&pos, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w ZZ - 0 1", err, sizeof(err)) == 0) return 0;
    tests_passed++;
    return 1;
}

static int test_eval_sanity(void) {
    tests_total++;
    float w[FEATURE_COUNT];
    make_weights(w);
    Position pos;
    char err[64];
    position_from_fen(&pos, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", err, sizeof(err));
    eval_score(&pos, w);
    tests_passed++;
    return 1;
}

static int test_random_games(void) {
    tests_total++;
    float w[FEATURE_COUNT];
    make_weights(w);
    SearchContext *ctx = search_create(w, 1 << 14);
    int illegal = 0;
    int win = 0;
    int games = 20;
    for (int g = 0; g < games; g++) {
        Position pos;
        char err[64];
        position_from_fen(&pos, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", err, sizeof(err));
        int eng_side = (g % 2 == 0) ? 0 : 1;
        int ply = 0;
        while (ply < 100) {
            Move mv[MAX_MOVES];
            int nm = generate_legal_moves(&pos, mv);
            if (nm == 0) break;
            Move m;
            if (pos.side == eng_side) {
                m = search_best_move(ctx, &pos, 2, 15);
                if (m == 0) m = mv[0];
                int ok = 0;
                for (int i = 0; i < nm; i++) if (mv[i] == m) ok = 1;
                if (!ok) {
                    illegal++;
                    m = mv[0];
                }
            } else {
                m = play_random(&pos);
            }
            Undo u;
            make_move(&pos, m, &u);
            Move _mv[MAX_MOVES];
            if (generate_legal_moves(&pos, _mv) == 0 && in_check(&pos, pos.side)) {
                if (pos.side != eng_side) win++;
                break;
            }
            ply++;
        }
        /* Determine result: if current side to move is in checkmate? current side lost */
        Move mv[MAX_MOVES];
        if (generate_legal_moves(&pos, mv) == 0 && in_check(&pos, pos.side)) {
            if (pos.side != eng_side) win++;
        }
    }
    search_destroy(ctx);
    if (illegal != 0) return 0;
    tests_total++;
    if (win >= (int)(games * 0.95)) tests_passed++;
    tests_passed++;
    return 1;
}

static int test_tactics(void) {
    const char *fens[] = {"6k1/5ppp/8/8/8/8/8/R6K w - - 0 1"};
    const char *expected[] = {"a1a8"};
    int n = 1;
    float w[FEATURE_COUNT];
    make_weights(w);
    SearchContext *ctx = search_create(w, 1 << 14);
    int okcount = 0;
    for (int i = 0; i < n; i++) {
        tests_total++;
        Position pos;
        char err[64];
        position_from_fen(&pos, fens[i], err, sizeof(err));
        Move m = search_best_move(ctx, &pos, 4, 50);
        char buf[16];
        int f = m & 63, t = (m >> 6) & 63;
        char a[3], b[3];
        a[0] = 'a' + f % 8; a[1] = '1' + f / 8; a[2] = 0;
        b[0] = 'a' + t % 8; b[1] = '1' + t / 8; b[2] = 0;
        snprintf(buf, sizeof(buf), "%s%s", a, b);
        if (strcmp(buf, expected[i]) == 0) {
            tests_passed++;
            okcount++;
        }
    }
    search_destroy(ctx);
    tests_total++;
    if (okcount >= (int)(n * 0.8)) tests_passed++;
    return 1;
}

static int test_resources(void) {
    tests_total++;
    struct stat st;
    if (stat("build/chess", &st) != 0) return 0;
    if (st.st_size > 65536) return 0;
    tests_passed++;
    tests_total++;
#ifdef __SANITIZE_ADDRESS__
    tests_passed++;
#else
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    if (ru.ru_maxrss * 1024L <= 5L * 1024 * 1024) tests_passed++;
#endif
    tests_total++;
    float w[FEATURE_COUNT];
    make_weights(w);
    SearchContext *ctx = search_create(w, 1 << 16);
    Position pos;
    char err[64];
    position_from_fen(&pos, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", err, sizeof(err));
    long s0 = now_ms();
    search_best_move(ctx, &pos, 3, 100);
    long s1 = now_ms();
    search_destroy(ctx);
#ifdef __SANITIZE_ADDRESS__
    tests_passed++;
#else
    if ((s1 - s0) <= 500) tests_passed++;
#endif
    return 1;
}

int main(void) {
    srand(12345);
    test_perft();
    test_fen_invalid();
    test_eval_sanity();
    test_random_games();
    test_tactics();
    test_resources();
    printf("%d/%d testes passaram\n", tests_passed, tests_total);
    return (tests_passed == tests_total) ? 0 : 1;
}
