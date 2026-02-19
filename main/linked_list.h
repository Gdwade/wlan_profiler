typedef struct node {
   uint64_t mac;
   struct node* next;
} node;

typedef struct {
    node * head;
    uint8_t curr_size;
    uint8_t max_size;
} list;

// Initialize list structure
list * init_list(uint8_t size);

// Deinitialize list structure
void free_list(list * l);

// Adds node with key: key to list l
uint8_t add_node(list * l, uint64_t key);

// Removes tail from list l
uint8_t remove_tail(list * l);

// Prints the contents of list from head to tail
void print_list(list * l);

uint8_t get_list_size (list * l);
