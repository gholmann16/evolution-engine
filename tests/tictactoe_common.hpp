#pragma once
#include <state.hpp>
#include <cstring>
#include <string>

inline bool won(char board[256], char player) {
    return
        (board[0] == player && board[1] == player && board[2] == player) ||
        (board[3] == player && board[4] == player && board[5] == player) ||
        (board[6] == player && board[7] == player && board[8] == player) ||
        (board[0] == player && board[3] == player && board[6] == player) ||
        (board[1] == player && board[4] == player && board[7] == player) ||
        (board[2] == player && board[5] == player && board[8] == player) ||
        (board[0] == player && board[4] == player && board[8] == player) ||
        (board[2] == player && board[4] == player && board[6] == player);
}

inline bool place(char board[256], unsigned char spot, char player) {
    if (spot >= 9)
        return true;
    if (board[spot] == ' ') {
        board[spot] = player;
        return false;
    }
    return true;
}

inline void flip(char board[256]) {
    for (int i = 0; i < 9; i++) {
        switch (board[i]) {
            case 'X': board[i] = 'O'; break;
            case 'O': board[i] = 'X'; break;
        }
    }
}

// Every engine call goes through here so a genome only ever has to learn
// to play as X -- flips the board into X-perspective for an 'O' move and
// flips it back right after, regardless of which test is calling.
inline size_t run_as_x(Engine * engine, char board[256], char output[256], size_t max, char mark) {
    bool mirrored = (mark == 'O');
    if (mirrored) flip(board);
    size_t cost = engine->run(board, output, max);
    if (mirrored) flip(board);
    return cost;
}

inline int fight(Engine * first, Engine * second) {
    alignas(256) char board[256] = "         ";
    alignas(256) char output[256] = {0};
    for (int turn = 0; turn < 9; turn++) {
        Engine * mover = (turn % 2 == 0) ? first : second;
        char mark = (turn % 2 == 0) ? 'X' : 'O';
        if (run_as_x(mover, board, output, State::max_runtime, mark) == State::max_runtime || place(board, output[0], mark))
            return (turn % 2 == 0) ? -1 : 1;
        if (won(board, mark))
            return (turn % 2 == 0) ? 1 : -1;
    }
    return 0;
}

// Self-pairing plays out normally (decisive nets 1, same as any mixed
// record) -- only a true tie needs a special case, since neither direction
// alone can ever score a tie's full 2.
inline int hof_wins(const void * code) {
    State::engine->load(code);
    int wins = 0;
    for (size_t i = 0; i < State::total_famers; i++) {
        State::comp->load(State::hall_of_fame[i]);
        int a = fight(State::engine, State::comp);
        int b = fight(State::comp, State::engine);
        if (a == 0 && b == 0 && State::engine->equal(code, State::hall_of_fame[i])) {
            wins += 2;
        } else {
            if (a == 1) wins++;
            if (b == -1) wins++;
        }
    }
    return wins;
}

inline int hof_cost(const void * code) {
    return (int)(State::total_famers * 2) - hof_wins(code);
}

// Exhaustive minimax-style search: `player` is the mark under test, run
// through the engine every time; `other`'s moves are tried out directly,
// covering every possible reply rather than just one opponent's play.
inline thread_local size_t exhaustive_runtime;
inline thread_local size_t exhaustive_wins;

inline size_t exhaustive_play(char board[256], char output[256], int empty, char player, char other) {
    alignas(256) char save_board[256];
    size_t total_recursive_score = 0;
    size_t games_left[] = {1, 1, 1, 2, 3, 8, 15, 48, 105};

    for (int i = 0; i < 9 && exhaustive_runtime <= State::max_runtime; i++) {
        if (board[i] != ' ')
            continue;
        memcpy(save_board, board, 64);
        save_board[i] = other;
        if (won(save_board, other)) {
            total_recursive_score += games_left[empty - 1];
            continue;
        }

        exhaustive_runtime += run_as_x(State::engine, save_board, output, State::max_runtime - exhaustive_runtime, player);
        if (place(save_board, output[0], player)) {
            total_recursive_score += games_left[empty - 1];
            continue;
        }

        if (won(save_board, player))
            exhaustive_wins += games_left[empty - 1];
        else if (empty)
            total_recursive_score += exhaustive_play(save_board, output, empty - 2, player, other);
    }
    return total_recursive_score;
}

inline void first_turn_x(char board[256], char output[256]) {
    exhaustive_runtime = run_as_x(State::engine, board, output, State::max_runtime, 'X');
}
