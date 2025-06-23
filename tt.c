#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "turn.h"

char board[10] = {'o', '-', '-', '-', '-', '-', '-', '-', '-', '-'}; // our "map"

void drawBoard()
{
    printf("\n");
    printf("|%c|%c|%c|\n", board[1], board[2], board[3]);
    printf("\n");
    printf("|%c|%c|%c|\n", board[4], board[5], board[6]);
    printf("\n");
    printf("|%c|%c|%c|\n", board[7], board[8], board[9]);
    printf("\n");
}

int checkIfWon()
{
    if (board[1] == board[2] && board[2] == board[3] && board[1] != '-')
        return 1;
    else if (board[4] == board[5] && board[5] == board[6] && board[4] != '-')
        return 1;
    else if (board[7] == board[8] && board[8] == board[9] && board[7] != '-')
        return 1;
    else if (board[1] == board[4] && board[4] == board[7] && board[1] != '-')
        return 1;
    else if (board[2] == board[5] && board[5] == board[8] && board[2] != '-')
        return 1;
    else if (board[3] == board[6] && board[6] == board[9] && board[3] != '-')
        return 1;
    else if (board[1] == board[5] && board[5] == board[9] && board[1] != '-')
        return 1;
    else if (board[3] == board[5] && board[5] == board[7] && board[3] != '-')
        return 1;

    return -1; // win check failed, still playing
}

int game(char * code) {
    int choice, player = 1, i = -1;
    int turns = 0;
    char mark;
    while (i == -1) {
        drawBoard();          // draw board 
        player = (player % 2) ? 1 : 2; // change player each run
        
        if (player == 1) {
            mark = 'X';
            choice = bf_turn(code, board);
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

    if (i == 1)
    {
        printf("Player %d won!\n", --player); // we need to substract 1 from player
        return --player;
    }
    else
    {
        printf("Game draw!\n");
        return 0;
    }
}
