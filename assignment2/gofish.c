#include <stdio.h>
#include <time.h>
#include "gofish.h"

int main(int args, char* argv[]) {
    srand(time(NULL));

    do {
        game_start();
        do {
            /* Play a round */
            if(game_loop())
                break; /* If there is a winner, go to game_end */
            current = next_player;
        } while(1);
    } while(game_end());
    fprintf(stdout, "Exiting\n");
    return 0;
}

/*
 * Contributor: Jeffrey Song
 * Function: game_start
 * --------------------
 * Starts a new game
 * resets players, shuffles deck, and deals cards
 * 
 * 
 */
void game_start() {
    reset_player(&user);
    reset_player(&computer);

    fprintf(stdout, "Shuffling deck...\n");
    shuffle();
    deal_player_cards(&user);
    deal_player_cards(&computer);

    /* It is possible to be dealt, at most, one book */
    check_add_book(&user);
    check_add_book(&computer);
    if(user.book[0] != 0)
        fprintf(stdout, "Player 1 books %s", pR(user.book[0]));
    if(computer.book[0] != 0)
        fprintf(stdout, "Player 2 books %s", pR(computer.book[0]));

    current = &user;
    next_player = &computer;
}

/*
 * Function: game_loop
 * -------------------
 * Called after game_start.
 * Handles each round of game play.
 * 
 * Return: 1 if there is a winner, 0 otherwise
 */
int game_loop() {
    fprintf(stdout, "\n");

    /* Print hand and book statuses */
    fprintf(stdout, "Player 1's Hand - ");
    print_hand(&user);
    fprintf(stdout, "Player 1's Book - ");
    print_book(&user);
    fprintf(stdout, "Player 2's Book - ");
    print_book(&computer);

    struct player* other_player = (current == &user) ? &computer : &user;

    if(game_over(current)) {
        return 1;
    }
    if(game_over(other_player)) { /* Shouldn't happen */
        current = other_player; /* Signify the correct winner */
        return 1;
    }

    /*
    "If a player runs out of cards, then they have to draw a card on their next turn.
    It does not end the game." -Irwin (piazza)
    */
    char r;
    if(current->hand_size > 0) { /* Non-empty hand */
        /* Get rank guess input */
        if(current == &user) { /* User's turn */
            r = user_play(current);
        } else { /* Computer's turn */
            r = computer_play(current);
        }
    } else /* Empty hand */
        r = 'X'; /* Invalid rank, so search will always fail and the player will Go Fish */

    /* Handle input */
    if(search(other_player, r) == 0) { /* Go Fish */
        if(r != 'X') /* Non-empty hand */
            fprintf(stdout, "    - Player %d has no %s's\n", ((current == &user) ? 2 : 1), pR(r));

        struct card* fished_card = next_card();
        int next_book_i = 0;
        if (fished_card != NULL) {
            if(current == &user)
                fprintf(stdout, "    - Go Fish, Player 1 draws %s%c\n", pR(fished_card->rank), fished_card->suit);
            else {
                if (fished_card->rank == r) {
                    fprintf(stdout, "    - Go Fish, Player 2 draws %s%c\n", pR(fished_card->rank), fished_card->suit);
                }
                else {
                    fprintf(stdout, "    - Go Fish, Player 2 draws a card\n");
                }
            }

           while(current->book[next_book_i] != 0) {
                next_book_i++;
            }
            add_card(current, fished_card);
            if(current->book[next_book_i] != 0)
                fprintf(stdout, "    - Player %d books %s\n", ((current == &user) ? 1 : 2), pR(fished_card->rank));
        }
        /* If a book was added or the asked rank was drawn, play again */
        if(fished_card != NULL && (
            current->book[next_book_i] != 0 || 
            fished_card->rank == r)
          ) {
            next_player = current;
            fprintf(stdout, "    - Player %d gets another turn\n", ((current == &user) ? 1 : 2));
        } else { /* Otherwise, switch players' turns */
            next_player = other_player;
            fprintf(stdout, "    - Player %d's turn\n", ((next_player == &user) ? 1 : 2));
        }
    } else { /* Transfer cards, play again */
        /* Print the cards of the guessed rank that each player has */
        struct player* print_player = current;
        int i;
        for(i = 0; i < 2; i++) {
            if (current == &computer && print_player == &computer) {
                print_player = (current == &user) ? &computer : &user; /* Switch to other player */
                continue;
            }
            fprintf(stdout, "    - Player %d has ", ((print_player == &user) ? 1 : 2));
            int j;
            struct hand* h = print_player->card_list;
            int rank_count = 0;
            for(j = 0; j < print_player->hand_size && h != NULL; j++) {
                if(h->top.rank == r) {
                    if(rank_count++ > 0)
                        fprintf(stdout, ", ");
                    fprintf(stdout, "%s%c", pR(r), h->top.suit);
                }

                h = h->next;
            }
            fprintf(stdout, "\n");
            print_player = (current == &user) ? &computer : &user; /* Switch to other player */
        }

        int next_book_i = 0;
        while(current->book[next_book_i] != 0) {
            next_book_i++;
        }
        transfer_cards(other_player, current, r);
        if(current->book[next_book_i] != 0)
            fprintf(stdout, "    - Player %d books %s\n", ((current == &user) ? 1 : 2), pR(r));

        /* If a book was added, play again */
        if(current->book[next_book_i] != 0) {
            next_player = current;
            fprintf(stdout, "    - Player %d gets another turn\n", ((current == &user) ? 1 : 2));
        } else { /* Otherwise, switch players' turns */
            next_player = other_player;
            fprintf(stdout, "    - Player %d's turn\n", ((next_player == &user) ? 1 : 2));
        }
    }
    return 0;
}

/*
 * Function: game_end
 * ------------------
 * Called after someone wins in 
 * GoFish from game_loop.
 * Declares the winner and asks the human 
 * if s/he wants to play again.
 * If Y is entered, go to game_start.
 * Else if N is entered, end game and close program.
 * 
 * Return: 1 to play again, 0 to exit
 */
int game_end() {
    struct player* other_player = (current == &user) ? &computer : &user;
    
    /* Count books of loser */
    int count = 0;
    while(other_player->book[count] != 0 && count < 6) { // index will always be empty due to loss
        count++;
    }
    if(current == &user) { // decide winner
        fprintf(stdout, "Player 1 Wins! 7-%d\n", count);
    } else {
        fprintf(stdout, "Player 2 Wins! 7-%d\n", count);
    }

    char yn[3] = ""; // yes or no
    int tryAgain = 0;
    do {
        if(tryAgain) {
            fprintf(stdout, "Error - must enter \"Y\" or \"N\"");
        }
        fprintf(stdout, "\nDo you want to play again [Y/N]: ");
        scanf("%2s", yn);
        tryAgain = 1;

        while(getchar() != '\n');

        if(yn[1] != '\0')
            continue;

        if(yn[0] == 'Y')
            return 1;
        else if(yn[0] == 'N')
            return 0;
        else
            continue;
    }while(1);
}

/*
 * Contributor: Jeffrey Song
 * Function: pR
 * ------------
 * stands for Print_Rank
 * if the rank is 'T' return "10"
 * other input is same but in
 * string format
 * 
 * r: the rank to format
 * 
 * Return: String representing rank r
 */
const char* pR(char r) {
    if(r == 'T') { // if T change to 10
        static char ten[] = "10";
        return ten;
    }
    static char rS[2];
    rS[0] = r;
    rS[1] = '\0';
    return rS;
}

/*
 * Contributor: Jeffrey Song
 * Function: print_hand
 * --------------------
 * Prints the hand of the
 * target player in the format
 * of rank and suit seperated by
 * a space
 * 
 * 
 * 
 * target: the player to print the hand of
 */
void print_hand(struct player* target) {
    if(target->hand_size == 0) { // empty hand print new line
        fprintf(stdout, "\n");
        return;
    }

    struct hand* h = target->card_list; // h is current hand

    while(h != NULL) { // while h is not null iterate and print each card
      fprintf(stdout, " %s%c", pR(h->top.rank), h->top.suit);
      h = h->next;
    }

    fprintf(stdout, "\n");
}

/*
 * Contributor: Jeffrey Song
 * Function: print_book
 * --------------------
 * Prints the book of the target player 
 * in the format of the rank
 * representaion
 * 
 * 
 * target: the player to print the book of
 */
void print_book(struct player* target) {
    if(target == NULL || target->book == NULL || target->book[0] == '\0' || target->book[0] == 0) { // newline if no book
        fprintf(stdout, "\n");
        return;
    }

    int i = 0;
    while(i < 7 && target->book[i] != '\0' && target->book[i] != 0) {
        fprintf(stdout, " %s", pR(target->book[i++]));
    }

    fprintf(stdout, "\n");
}
