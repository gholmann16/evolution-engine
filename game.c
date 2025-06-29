#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "turn.h"

char board[9] = {'-', '-', '-', '-', '-', '-', '-', '-', '-'}; // our "map"

void drawBoard()
{
    printf("\n");
    printf("|%c|%c|%c|\n", board[0], board[1], board[2]);
    printf("\n");
    printf("|%c|%c|%c|\n", board[3], board[4], board[5]);
    printf("\n");
    printf("|%c|%c|%c|\n", board[6], board[7], board[8]);
    printf("\n");
}

int checkIfWon()
{
    if (board[0] == board[1] && board[1] == board[2] && board[0] != '-')
        return 1;
    else if (board[3] == board[4] && board[4] == board[5] && board[3] != '-')
        return 1;
    else if (board[6] == board[7] && board[7] == board[8] && board[6] != '-')
        return 1;
    else if (board[0] == board[3] && board[3] == board[6] && board[0] != '-')
        return 1;
    else if (board[1] == board[4] && board[4] == board[7] && board[1] != '-')
        return 1;
    else if (board[2] == board[5] && board[5] == board[8] && board[2] != '-')
        return 1;
    else if (board[0] == board[4] && board[4] == board[8] && board[0] != '-')
        return 1;
    else if (board[2] == board[4] && board[4] == board[6] && board[2] != '-')
        return 1;

    return -1; // win check failed, still playing
}

int game(char * code) {

    if (code == NULL)
        return 0;

    int choice, player = 1, i = -1;
    int turns = 0;
    char mark;
    while (i == -1) {
        drawBoard();          // draw board 
        player = (player % 2) ? 1 : 2; // change player each run
        
        if (player == 1) {
            mark = 'X';
            choice = bf_turn(code, board);
            printf("Robot chose %d", choice);
            if (choice > 9)
                choice = random_turn();
        }
        else {
            mark = 'O';
            choice = random_turn();
        }

        while (board[choice] != '-') {
            choice = random_turn(); // If either value is taken just randomize till you get a free space
        }

        board[choice] = mark;

        i = checkIfWon();
        player++;
        turns++;
        if (turns == 9)
            i = 0;
    }

    drawBoard();
    player--;

    // Announce and score attempts
    if (i == 1) {
        printf("Player %d won!\n", --player);
        return turns + (player % 2) * 100; // How many turns they lasted + 100 if they won
    }
    else {
        printf("Game draw!\n");
        return 50;
    }
    
}
