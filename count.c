/**
 * Enumerate all possible games of Kalah(6,3) with empty capture.
 *
 * Phase 1: total game count (memoized by pit config, very fast).
 * Phase 2: score-contribution distribution for W/D/L breakdown.
 * Phase 3: perfect-play game count.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "solve.c"

/* ── Phase 1: total game count ──────────────────────────────────────── */

#define CT_SIZE (1 << 22)
typedef struct { uint64_t key, count; } CTEntry;
static CTEntry ct[CT_SIZE];
static bool ct_valid[CT_SIZE];

static uint64_t count_all(Board board) {
    if (is_game_over(&board)) return 1;

    uint64_t h = hash_board(&board);
    uint32_t idx = (h >> 40) & (CT_SIZE - 1);
    if (ct_valid[idx] && ct[idx].key == h) return ct[idx].count;

    uint64_t total = 0;
    for (int p = 0; p < PITS; p++) {
        if (!board.pit[p]) continue;
        Board after = board;
        bool again = sow(&after, p);
        if (!again) flip(&after);
        total += count_all(after);
    }

    ct[idx] = (CTEntry){h, total};
    ct_valid[idx] = true;
    return total;
}

/* ── Phase 2: score distribution ────────────────────────────────────── */

#define SCORE_RANGE 73
#define SCORE_OFF   36

typedef struct { uint64_t d[SCORE_RANGE]; } Dist;

#define DT_SIZE (1 << 20)
static Dist     *dt;        /* heap-allocated: DT_SIZE entries */
static uint64_t *dt_key;
static bool     *dt_valid;

static Dist compute_dist(Board board) {
    Dist result;
    memset(&result, 0, sizeof(result));

    if (is_game_over(&board)) {
        int bal = 0;
        for (int i = 0; i < PITS; i++) bal += board.pit[i] - board.opp_pit[i];
        result.d[bal + SCORE_OFF] = 1;
        return result;
    }

    uint64_t h = hash_board(&board);
    uint32_t idx = (h >> 40) & (DT_SIZE - 1);
    if (dt_valid[idx] && dt_key[idx] == h) return dt[idx];

    int sd = board.store - board.opp_store;

    for (int p = 0; p < PITS; p++) {
        if (!board.pit[p]) continue;
        Board after = board;
        bool again = sow(&after, p);
        int delta = (after.store - after.opp_store) - sd;

        if (again) {
            Dist cd = compute_dist(after);
            for (int s = 0; s < SCORE_RANGE; s++) {
                if (!cd.d[s]) continue;
                int mi = s + delta;
                if (mi >= 0 && mi < SCORE_RANGE)
                    result.d[mi] += cd.d[s];
            }
        } else {
            flip(&after);
            Dist cd = compute_dist(after);
            for (int s = 0; s < SCORE_RANGE; s++) {
                if (!cd.d[s]) continue;
                int mi = delta - s + 2 * SCORE_OFF;
                if (mi >= 0 && mi < SCORE_RANGE)
                    result.d[mi] += cd.d[s];
            }
        }
    }

    dt_key[idx] = h;
    dt[idx] = result;
    dt_valid[idx] = true;
    return result;
}

/* ── Phase 3: perfect-play counting ─────────────────────────────────── */

/* Reuse ct/ct_valid after clearing */
static uint64_t count_optimal(Board board) {
    if (is_game_over(&board)) return 1;

    uint64_t h = hash_board(&board);
    uint32_t idx = (h >> 40) & (CT_SIZE - 1);
    if (ct_valid[idx] && ct[idx].key == h) return ct[idx].count;

    int best = search_full(&board).score;

    uint64_t total = 0;
    for (int p = 0; p < PITS; p++) {
        if (!board.pit[p]) continue;
        Board after = board;
        bool again = sow(&after, p);
        if (!again) flip(&after);
        int v = again ? search_full(&after).score
                      : -search_full(&after).score;
        if (v == best)
            total += count_optimal(after);
    }

    ct[idx] = (CTEntry){h, total};
    ct_valid[idx] = true;
    return total;
}

int main(void) {
    Board board = (Board)START_BOARD;

    /* Phase 1 */
    printf("Counting total games...\n"); fflush(stdout);
    uint64_t total = count_all(board);
    printf("Total games: %llu\n\n", (unsigned long long)total);
    fflush(stdout);

    /* Phase 2 — allocate distribution table on heap */
    printf("Computing outcome distribution...\n"); fflush(stdout);
    dt      = calloc(DT_SIZE, sizeof *dt);
    dt_key  = calloc(DT_SIZE, sizeof *dt_key);
    dt_valid = calloc(DT_SIZE, sizeof *dt_valid);
    if (!dt || !dt_key || !dt_valid) { perror("calloc"); return 1; }

    Dist d = compute_dist(board);

    uint64_t p1w = 0, draws = 0, p2w = 0;
    for (int s = 0; s < SCORE_RANGE; s++) {
        int score = s - SCORE_OFF;
        if      (score > 0) p1w   += d.d[s];
        else if (score == 0) draws += d.d[s];
        else                 p2w   += d.d[s];
    }
    printf("  P1 wins:  %14llu  (%5.2f%%)\n",
           (unsigned long long)p1w, 100.0 * p1w / total);
    printf("  Draws:    %14llu  (%5.2f%%)\n",
           (unsigned long long)draws, 100.0 * draws / total);
    printf("  P2 wins:  %14llu  (%5.2f%%)\n\n",
           (unsigned long long)p2w, 100.0 * p2w / total);
    fflush(stdout);

    free(dt); free(dt_key); free(dt_valid);

    /* Phase 3 */
    printf("Counting perfect-play games...\n"); fflush(stdout);
    memset(ct, 0, sizeof(ct));
    memset(ct_valid, 0, sizeof(ct_valid));
    Result r = search_full(&board);
    uint64_t perfect = r.score > 0 ? count_optimal(board) : 0;

    printf("Perfect-play score: %+d\n", r.score);
    printf("Perfect-play P1 wins: %llu\n", (unsigned long long)perfect);
    if (perfect)
        printf("  = 1 in every %llu games\n",
               (unsigned long long)(total / perfect));

    /* Score histogram */
    printf("\nScore distribution:\n");
    for (int s = 0; s < SCORE_RANGE; s++) {
        if (d.d[s])
            printf("  %+3d: %llu\n", s - SCORE_OFF,
                   (unsigned long long)d.d[s]);
    }

    return 0;
}
