#include "solve.c"

typedef struct {
    Board board;     /* bytes 0..13 */
    uint8_t turn;    /* byte 14 */
    int8_t score;    /* byte 15 */
} State;

static State state;
#define EXPORT __attribute__((visibility("default")))

EXPORT State* init(int clear_tt) {
    if (clear_tt) for (int i = 0; i < TT_SIZE; i++) tt[i] = (TTEntry){0};
    nodes_searched = 0;
    state = (State){.board = (Board)START_BOARD};
    return &state;
}

EXPORT int move(int pit) {
    Board board = state.board;
    if (state.turn) flip(&board);
    if (pit < 0 || pit >= PITS || !board.pit[pit]) return -1;
    bool again = sow(&board, pit);
    if (state.turn) flip(&board);
    state.board = board;
    if (!again) state.turn ^= 1;
    return again ? 1 : 0;
}

EXPORT int solve(void) {
    Board board = state.board;
    if (state.turn) flip(&board);
    Result best = search_full(&board);
    state.score = (int8_t)(state.turn ? -best.score : best.score);
    return best.move;
}
