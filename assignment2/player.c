#include <stdio.h>
#include "player.h"
#include "deck.h"
#include "card.h"

/*
 * Contributor: Byung Woong Ko
 * Function: add_card
 * -------------------
 *  Add a new card to the player's hand. 
 *
 *  target: the target player
 *  new_card: pointer to the new card to add
 *
 *  returns: return 0 if no error, non-zero otherwise
 */
int add_card(struct player* target, struct card* new_card) {
    // Adds card to the hand of player by linking it to the hand LL. If player has no hands, set link to null. Increase hand size
    if(target->hand_size == 0) {
        target->card_list = (struct hand*)malloc(sizeof(struct hand));
        target->card_list->top = *new_card;
        target->card_list->next = NULL;
    } 
    else {
        int i=0;
        struct hand* iter = target->card_list;
        while(i < target->hand_size-1) {
            iter = iter->next;
            i++;
        }

	//Corner case to check if hand of player matches with number if iterations.
        if(i != target->hand_size-1)
            return -1;

        iter->next = (struct hand*)malloc(sizeof(struct hand));
        iter->next->top = *new_card;
        iter->next->next = NULL;
    }
    target->hand_size++;

    //Check if 4 ranks is found everytime we add a card to the player's hand
    check_add_book(target);

    return 0;
}

/*
 * Contributor: Jeffrey Song
 * Function: remove_card
 * ---------------------
 *  Remove a card from the player's hand. 
 *
 *  target: the target player
 *  old_card: pointer to the old card to remove
 *
 *  returns: return 0 if no error, non-zero otherwise
 */
int remove_card(struct player* target, struct card* old_card) {

    if(target->hand_size <= 0) // return -1 if hand is empty
        return -1;

    int i = 0;
    int found = 0;
    struct hand* current = target->card_list;
    struct hand* prev = NULL;

    while(i < target->hand_size){ // find card, if not found return -1
        if(current != NULL &&(current->top.suit == old_card->suit && current->top.rank == old_card->rank)) {
            found = 1;
            break;
        }
        prev = current;
        current = current->next;
        i++;
    }

    if(found == 0) { // return -1 if not found
        return -1;
    }

    if(prev == NULL || &(prev->top.suit) == NULL || prev->top.suit == '\0' || prev->top.suit == 0) { // removing card from beginning
        target->card_list = target->card_list->next;
        free(current);
        target->hand_size--;

        return 0;
    } else if(current == NULL || &(current->top.suit) == NULL || current->top.suit == '\0') { // null from pointer
        return -1;
    } else { // removing from middle
        prev->next = current->next;
        free(current);
        target->hand_size--;

        return 0;
    }

    return -1;
}

/*
 * Contributor: Byung Woong Ko
 * Function: check_add_book
 * ------------------------
 * Check if a player has all 4 cards of the same rank.
 * If so, remove those cards from the hand, and add the rank to the book.
 * Returns after finding one matching set of 4, so should be called after adding each a new card.
 * 
 * target: pointer to the player to check
 * 
 * Return: a char that indicates the book that was added; return 0 if no book added.
 */
char check_add_book(struct player* target) {
    if(target->card_list == NULL) {
        return 0;
    }

    int count = 1;
    struct hand* hand1;
    struct hand* hand2;
    struct hand* hand3;
    struct hand* hand4;
    struct hand* iter;
    hand4 = target->card_list;
    iter = target->card_list;

    // set the last hand to the end of the player's hand LL.
    // Because we check book each time we add card to player, we only need to check if there is 3 other matching ranks with the last card's rank in the player's hand.
    for(int i=0; i < target->hand_size && hand4->next != NULL; i++) {
        hand4 = hand4->next;
    }

    // Iterate through the player's hand, add count if a rank match is found and save the matched hand references to be used later.
    for(int j = 0; j < target->hand_size - 1 && iter != NULL; j++) {
        if(iter->top.rank == hand4->top.rank) {
            if(count == 1) {
                hand1 = iter;
            } 
            else if(count == 2) {
                hand2 = iter;
            } 
            else if(count == 3) {
                hand3 = iter;
                count++;
                break;
            } 
            else {
                return 0;
            }
            count++;
        }
        iter = iter->next;
    }
    
    // If 4 rank matches are found, add the value of rank to the book, and remove the matched rank saved above from the player's hand. Return rank added to the book or 0 if no match is found.
    if(count == 4) {
        char rankB = hand1->top.rank;

        for(int k = 0; k < 7; k++) {
            if(target->book[k] == 0 || target->book[k] == '\0') {
                target->book[k] = rankB;
                break;
            }
        }

        remove_card(target, &hand1->top);
        remove_card(target, &hand2->top);
        remove_card(target, &hand3->top);
        remove_card(target, &hand4->top);

        return rankB;
    }

    return 0;
}

/*
 * Contributor: Jeffrey Song
 * Function: search
 * ----------------
 *  Search a player's hand for a requested rank.
 *  
 *  rank: the rank to search for
 *  target: the player (and their hand) to search
 *
 *  Return: If the player has a card of that rank, return 1, else return 0
 */
int search(struct player* target, char rank) {
    int i = 0;
    struct hand* current_hand = target->card_list;

    while(i < target->hand_size && current_hand != NULL) { // run until NULL or empty hand
        if(current_hand->top.rank == rank)
            return 1;

        current_hand = current_hand->next;
        i++;
    }

    return 0;
}

/*
 * Contributor: Byung Woong Ko
 * Function: transfer_cards
 * ------------------------
 * Transfer cards of a given rank from the source player's 
 * hand to the destination player's hand. Remove transferred 
 * cards from the source player's hand. Add transferred cards 
 * to the destination player's hand.
 * 
 * src: a pointer to the source player
 * dest: a pointer to the destination player
 * rank: the rank to transfer
 * 
 * Return: 0 if no cards found/transferred, <0 if error, otherwise 
 * return value indicates number of cards transferred.
 */
int transfer_cards(struct player* src, struct player* dest, char rank) {
    int i=0;
    int counter = 0;
    struct hand* iter = src->card_list;
    // Iterate through the hand of the player. If rank match is found, it will remove the card from the targeted player's hand and add it to the targeted player's hand
    while(iter != NULL) {
        if(iter->top.rank == rank) {
            add_card(dest, &iter->top);
            remove_card(src, &iter->top);
            iter = iter->next;
            counter++;
        } 
        else {
            iter = iter->next;
        }
        i++;
    }
    // Corner case to check if the handsize of target player matches the iteration.
    if(i != src->hand_size) {
        return -1;
    }

    return counter;
}

/*
 * Contributor: Byung Woong Ko
 * Function: game_over
 * -------------------
 * Boolean function to check if a player has 7 books yet 
 * and the game is over
 * 
 * target: the player to check
 * 
 * Return: 1 if game is over, 0 if game is not over
 */
int game_over(struct player* target) {
        // Case to check if the last cell of array is empty. If empty, game is not over. If not, game is over. 
        if(target->book[6] == '\0' || target->book[6] == 0)
            return 0;
    return 1;
}

/* 
 * Contributor: Jeffrey Song
 * Function: reset_player
 * ----------------------
 *
 *   Reset player by free'ing any memory of cards remaining in hand,
 *   and re-initializes the book.  Used when playing a new game.
 * 
 *   target: player to reset
 * 
 *   Return: 0 if no error, and non-zero on error.
 */
int reset_player(struct player* target) {

    while(target->card_list != NULL) { // go through player hand and remove top card
        remove_card(target, &target->card_list->top);
    }

    if(target->hand_size != 0 || target->card_list != NULL) { // error
        return -1;
    }

    
    int i = 0;
    while(i < 7) { // reset books
        target->book[i] = 0;
        i++;
    }

    if(i != 7 || target->hand_size != 0) { // error
        return -1;
    }
    return 0;
}

/*
 * Contributor: Byung Woong Ko
 * Function: computer_play
 * -----------------------
 * 
 * Select a rank randomly to play this turn. The player must have at least
 * one card of the selected rank in their hand.
 * 
 * target: the player's hand to select from
 * 
 * Rank: return a valid selected rank
 */
char computer_play(struct player* target) {
    int pick;
    struct hand* iter = target->card_list;
    pick = rand() % target->hand_size; 	// A random number between 0 to number of cards computer has

    // Iterate to the random position defined by pick
    for(int i = 0; i < pick; i++) {
        iter = iter->next;
    }
    // Case if 'T' is selected. we convert T to 10. If not, print character referenced by rank array
    if(iter->top.rank == 'T'){
        fprintf(stdout, "Player 2's turn, enter a Rank: 10\n");
    }
    else{
        fprintf(stdout, "Player 2's turn, enter a Rank: %c\n", iter->top.rank);
    }

    return iter->top.rank;
}

/* 
 * Contributor: Jeffrey Song
 * Function: user_play
 * -------------------
 *
 *   Read standard input to get rank user wishes to play.  Must perform error
 *   checking to make sure at least one card in the player's hand is of the 
 *   requested rank.  If not, print out "Error - must have at least one card from rank to play"
 *   and then re-prompt the user.
 *
 *   target: the player's hand to check
 * 
 *   returns: return a valid selected rank
 */
char user_play(struct player* target) {
    char rank;
    do {
        fprintf(stdout, "Player 1's turn, enter a Rank: ");
        char irank[4] = "";
        scanf("%3s", irank); // user input rank
        while(getchar() != '\n'); // gets rid of unwanted characters and \n

        if(irank[1] == '\0') // one character input
            rank = irank[0];
        else if(irank[0] == '1' && irank[1] == '0' && irank[2] == '\0') // when input is 10
            rank = 'T';
        else { // error in input
            fprintf(stdout, "Error - must have at least one card from rank to play\n");
            continue;
        }
        
        if(search(target, rank) && irank[0] != 'T') // search if card is in hand
            break;

        fprintf(stdout, "Error - must have at least one card from rank to play\n");
    }while(1);

    return rank;
}