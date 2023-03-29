#include <stdio.h>
#include <assert.h>
#include <time.h>
#include "deck.h"

/*
 * Contributor: Byung Woong Ko
 * Function: shuffle
 * --------------------
 * Initializes deck_instance and shuffles it.
 * Resets the deck if a prior game has been played.
 * 
 * returns: 0 if no error, and non-zero on error
 */
int shuffle() {
    int i;
    int j;
    int k;
    int pos;
    // Create a new deck
    deck_instance = *(struct deck*)malloc(sizeof(struct deck));
  
    // Checks to make sure list is empty.
    if(&deck_instance.list[0].suit == NULL || deck_instance.list[0].suit == '\0') {
        char suits[4] = {'C', 'D', 'H', 'S'};
        char ranks[13] = {'2', '3', '4', '5', '6', '7', '8', '9', 'T', 'J', 'Q', 'K', 'A'};
        
        // 2 for loops, 13 ranks for each suit 
        for(i = 0; i < 4; i++) {
            for(j = 0; j < 13; j++) {
                struct card new_card = {.suit=suits[i], .rank=ranks[j]};
                deck_instance.list[deck_instance.top_card++] = new_card; 
            }
        }
    // Corner case to check if both for loops iterated to their respective ends
    if(i != 4 || j != 13)
        return -1;
    }


    // Shuffling portion portion of code after creating new deck
    // Each array position is swapped once with a randomized position(>0,<51) given by rand()
    for(k = 0; k < 52; k++) {
        pos = rand() % 51;
        struct card temp = deck_instance.list[k];
        deck_instance.list[k] = deck_instance.list[pos];
        deck_instance.list[pos] = temp;
    }
    // Corner case to check if loops iterated to their respective end.
    if(k != 52)
        return -1;

    /* Reset Top Card Counter */
    deck_instance.top_card = 52;
    return 0;
}

/*
 * Contributor: Byung Woong Ko
 * Function: deal_player_cards
 * ---------------------------
 * Deal 7 random cards to the player specified in the function.
 * Remove the dealt cards from the deck.
 * 
 * target: pointer to the player to be dealt cards
 * 
 * returns: 0 if no error, and non-zero on error
 */
int deal_player_cards(struct player* target) {
    int i;
    struct card* temp;
    // Add card to the target player from the deck
    for(i = 0; i < 7; i++) {
        // Corner case to check if deck does not have a card
        if(deck_size() <= 0) {
            return -1;
        }
        temp = next_card();
	// Corner case to check if the card retrieved from next_card() is not null.
        if(temp == NULL) {
            return -1;
        }
        add_card(target, temp);
    }

    if(i != 7) {
        return -1;
    }

    return 0;
}

/*
 * Contributor: Jeffrey Song
 * Function: next_card
 * -------------------
 * Return a pointer to the top card on the deck.
 * Removes that card from the deck.
 * 
 * returns: pointer to the top card on the deck.
 */
struct card* next_card() {
    if(deck_instance.top_card <= 0)
        return NULL;

    return &deck_instance.list[--deck_instance.top_card];
}

/*
 * Contributor: Jeffrey Song
 * Function: size
 * --------------
 * Return the number of cards left in the current deck.
 * 
 * returns: number of cards left in the deck.
 */
size_t deck_size() {
    if(deck_instance.list[0].suit == 0 || 
       deck_instance.list[0].rank == 0 || 
       deck_instance.top_card < 0) {
           return 0;
       } else {
           return deck_instance.top_card;
       }
}

