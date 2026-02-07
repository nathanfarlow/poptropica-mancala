/** Kalah(6, 3) with empty capture solver. */

#ifdef __wasm__
#include "wasm.h"
#else
#include <stdbool.h>
#include <stdint.h>
#endif

#define PITS 6
#define HALF (PITS + 1) // One side: 6 pits + 1 store
#define FULL (HALF * 2)

typedef union {
    struct {
        uint8_t pit[PITS];
        uint8_t store;
        uint8_t opp_pit[PITS];
        uint8_t opp_store;
    };
    uint8_t ring[FULL]; // Flat view for sowing and flipping
} Board;

#define START_BOARD {.pit = {3, 3, 3, 3, 3, 3}, .opp_pit = {3, 3, 3, 3, 3, 3}}

static void flip(Board *board) {
    for (int i = 0; i < HALF; i++) {
        uint8_t tmp = board->ring[i];
        board->ring[i] = board->ring[HALF + i];
        board->ring[HALF + i] = tmp;
    }
}

static bool is_game_over(const Board *board) {
    bool cur = false, opp = false;
    for (int i = 0; i < PITS; i++) {
        if (board->pit[i]) cur = true;
        if (board->opp_pit[i]) opp = true;
    }
    return !cur || !opp;
}

static int final_score(const Board *board) {
    int s = board->store - board->opp_store;
    for (int i = 0; i < PITS; i++) s += board->pit[i] - board->opp_pit[i];
    return s;
}

/** Returns true if player should move again */
static bool sow(Board *board, int pos) {
    int n = board->pit[pos];
    board->pit[pos] = 0;
    while (n--) {
        pos = (pos + 1) % (FULL - 1); // Skip opp's store
        board->ring[pos]++;
    }
    if (pos == PITS) return true;             // Landed in own store
    if (pos < PITS && board->pit[pos] == 1) { // Capture, even if opponent's pit is empty
        int opp = PITS - 1 - pos;
        board->store += board->pit[pos] + board->opp_pit[opp];
        board->pit[pos] = board->opp_pit[opp] = 0;
    }
    return false;
}

#define TT_SIZE (1 << 22)
enum { EXACT, LOWER, UPPER };

typedef struct {
    uint64_t key;
    int8_t score, depth, move, bound;
    bool solved;
} TTEntry;
static TTEntry tt[TT_SIZE];
static long long nodes_searched;

static uint64_t hash_board(const Board *board) {
    uint64_t h = 0x281dcf94d307a6b0ULL;
    for (int i = 0; i < PITS; i++) {
        h = (h ^ board->pit[i]) * 0x31d8b11cbabad6e3ULL;
        h = (h ^ board->opp_pit[i]) * 0x31d8b11cbabad6e3ULL;
    }
    return h;
}

typedef struct {
    int move, score;
    bool solved;
} Result;

static Result search(const Board *board, int depth, int alpha, int beta) {
    if (is_game_over(board)) return (Result){-1, final_score(board), true};

    uint64_t hash = hash_board(board);
    TTEntry *entry = &tt[(hash >> 40) & (TT_SIZE - 1)];
    bool hit = entry->key == hash;

    int hint = hit ? entry->move : -1;
    int score_diff = board->store - board->opp_store;

    // Is the TT entry sufficient to avoid searching this state again?
    if (hit && (entry->solved || entry->depth >= depth)) {
        int v = entry->score + score_diff;
        if (entry->bound == EXACT || (entry->bound == LOWER && v >= beta) ||
            (entry->bound == UPPER && v <= alpha))
            return (Result){hint, v, entry->solved};
    }

    if (depth <= 0) return (Result){-1, score_diff, false};
    nodes_searched++;

    bool solved = true;
    int best_score = -100, best_move = -1, orig_alpha = alpha;
    for (int i = -1; i < PITS && alpha < beta; i++) {
        // First, try the best move found by a shallower search
        int pit = i < 0 ? hint : PITS - 1 - i;
        if (pit < 0 || !board->pit[pit]) continue;
        if (i >= 0 && pit == hint) continue;

        Board after = *board;
        Result result;
        if (sow(&after, pit)) {
            // Player goes again
            result = search(&after, depth - 1, alpha, beta);
        } else {
            // Opponent's turn
            flip(&after);
            result = search(&after, depth - 1, -beta, -alpha);
            result.score *= -1;
        }
        if (!result.solved) solved = false;
        if (result.score > best_score) {
            best_score = result.score;
            best_move = pit;
            if (best_score > alpha) alpha = best_score;
        }
    }

    // Should we overwrite the TT entry with something better?
    if (solved || ((!hit || depth >= entry->depth) && !entry->solved)) {
        int8_t bound = best_score <= orig_alpha ? UPPER : best_score >= beta ? LOWER : EXACT;
        *entry = (TTEntry){hash, best_score - score_diff, depth, best_move, bound, solved};
    }
    return (Result){best_move, best_score, solved};
}

static Result search_full(const Board *board) {
    Result r = {-1, 0, false};
    for (int d = 1;; d++) {
        r = search(board, d, -36, 36);
        if (r.solved) break;
    }
    return r;
}
