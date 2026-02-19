/* General Thoughts:
 * Since we dont actually care about WHO the devices are, just the count, we can insead create a
 * doubly linked list that starts evicting (FIFO) after reaching a max size. This should be way less
 * complex and make removing old ones make sense. The question is really will it be fast enough. Hopefully
 * the malloc calls wont be too awful since theyre small. Track time since the tail was changed, and
 * remove after a give timeout.
 * 
 * Struct: 16 bytes
 *  MAC: 8 bytes
 *  next *: 4 bytes
 * 
 * Globals:
 * Head: 4 bytes
 * Tail: 4 bytes
 * Size: 2 bytes
 * Tail_update_counter: 4-8 bytes?
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "linked_list.h"

// Helper fuction to create nodes internally
node * create_node(uint64_t mac, node * next);

list * init_list(uint8_t size){
    list * l = (list * ) malloc(sizeof(list));
    l->head = NULL;
    l->curr_size = 0;
    l->max_size = size;
    return l;
}

void free_list(list * l) {
    if (l->head == NULL) return;

    node * curr = l->head;
    while (curr != NULL) {
        node * tmp = curr;
        curr = curr->next;
        free(tmp);
    }

    free(l);
    return;
}

void print_list(list * l) {
    node * curr = l->head;
    printf("Current List of size %u: ", l->curr_size);
    while (curr != NULL) {
        printf("%llu ", curr->mac);
        curr = curr->next;
    };
    printf("\n");
    fflush(stdout);
    return;
}

node * create_node(uint64_t mac, node * next) {
    node * n = malloc(sizeof(node));
    n->mac = mac;
    n->next = next;
    return n;
}

// adds a node to the linked list, and returns the current size of the linked list. 
uint8_t add_node(list * l, uint64_t key){
    if (l == NULL) return 0;

    node * curr = l->head;
    node * prev = NULL;

    // Remove existing node if key found
    while (curr != NULL) {
        if (curr->mac == key) {
            if (prev == NULL)
                l->head = curr->next;
            else
                prev->next = curr->next;

            free(curr);
            l->curr_size--;
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    // Insert at head
    l->head = create_node(key, l->head);
    l->curr_size++;

    // If over max size, remove tail
    if (l->curr_size > l->max_size) {
        remove_tail(l);
    }

    return l->curr_size;
}

uint8_t remove_tail(list * l) {
    // double linking could have been clutch but oh well
    if (l == NULL || l->head == NULL) return 0;
    if (l->head->next == NULL) {
        free(l->head);
        l->head = NULL;
        l->curr_size = 0;
        return 0;
    }
    node * curr = l->head;
    for (int i = 1; i < l->curr_size - 1; i++) {
        curr = curr->next;
    }
    // Free the tail
    free(curr->next);
    curr->next = NULL;
    l->curr_size -= 1;
    return l->curr_size;
}

uint8_t get_list_size (list * l) {
    return l->curr_size;
}