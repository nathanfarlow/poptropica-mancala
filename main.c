#include <stdio.h>
#include "solve.c"

int main(void) {
    Board board = (Board)START_BOARD;
    Result r = search_full(&board);
    printf("move=%d  score=%+d  nodes_searched=%lld\n", r.move, r.score, nodes_searched);
}
