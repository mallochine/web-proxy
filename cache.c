// vim: set et ts=4 sts=4 sw=4:

#include "cache.h"

// way to debug the cache
void print_cache(cache C) {
    printf("************* Printing Cache ************\n");
    printf("Cache size:%d\n\n", C->size);

    web_object cur = C->first;
    int count = 0;
    while (cur != NULL) {
        count++;
        printf("Web Object:%d\n", count);
        printf("URL:%s;\n", cur->URL);
        printf("Size:%d\n", cur->size);
        printf("Time:%d\n", (int) cur->timestamp);
        //printf("Buf:%s;\n", cur->buf);
        printf("\n");

        cur = cur->next;
    }

    printf("************* Done Printing **************\n");
}

cache new_cache() {
    cache C = (cache) malloc(sizeof(struct cache));
    C->size = 0;
    C->first = NULL;
    return C;
}

// Create a new web object
web_object new_webobj(char *URL, char *buf, int len) {
    // Malloc memory for the web object
    web_object W = (web_object) malloc(sizeof(struct web_object));
    W->buf = (char *) malloc( sizeof(char)*len );
    W->URL = (char *) malloc( sizeof(char)*strlen(URL)+1 );

    // Set the web object's size, timestamp, URL, and buf
    W->size = len;
    W->timestamp = time(NULL);
    memcpy(W->buf, buf, len);
    strcpy(W->URL, URL);

    // Set the web object's links
    W->next = NULL;
    W->prev = NULL;

    return W;
}

web_object getLRU(cache C) {
    // init the iterator
    web_object cur = C->first;

    // LRU tracks the least recently used web object found so far
    web_object LRU = NULL;

    // iterate through the cache
    while (cur != NULL) {
        // check for whether cur is used later than LRU
        if (LRU == NULL || cur->timestamp < LRU->timestamp)
            LRU = cur;

        // iterate cur
        cur = cur->next;
    }

    return LRU;
}

// doesn't deal with correcting W's links in the cache
void free_webobj(web_object W) {
    if (W == NULL) return;
    free(W->URL);
    free(W->buf);
    free(W);
}

// remove the web object from the cache
void remove_webobj(cache C, web_object W) {
    // Check for NULL
    if (C == NULL || W == NULL) return;

    // Adjust the cache size
    C->size -= W->size;

    // Adjust W's links
    W->prev->next = W->next;
    if (W->next != NULL)
        W->next->prev = W->prev;

    // Free resources used for W
    free_webobj(W);
}

// Assume new web object is less than MAX_OBJECT_SIZE
// Assume new web object is not in the cache already
void insert_webobj(cache C, char *URL, char *buf, int len) {
    // Create a new web_object
    web_object W = new_webobj(URL, buf, len);

    // if inserting the new object will exceed MAX_CACHE_SIZE, then
    // we need to evict LRU web_objects
    if (C->size + len > MAX_CACHE_SIZE) {
        // debug: indicate we're deleting items from linked list
        printf("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");

        // Continually evict LRU's until web object will fit into cache
        while (C->size + len > MAX_CACHE_SIZE) {
            printf("%%%%%%%%%%%%%%%%\n");
            remove_webobj(C, getLRU(C));
        }
    }

    // debug: indicate we're adding W to the linked list
    printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");

    // Adjust the links
    W->next = C->first;
    W->prev = C;
    if (W->next != NULL)
        W->next->prev = W;
    C->first = W;

    // Adjust cache's size
    C->size += len;
}

// Assume buf has MAX_OBJECT_SIZE allocated
int read_cache(cache C, char *URL, char *buf) {
    web_object cur = C->first;

    // loop through cache
    while (cur != NULL) {

        // check whether we have a hit
        if (!strcmp(URL, cur->URL)) {

            // change the access time
            cur->timestamp = time(NULL);

            // copy over the cached reply to buf
            memcpy(buf, cur->buf, cur->size);

            // return the size of the cached reply
            return cur->size;
        }
        
        // iterate cur
        cur = cur->next;
    }

    // return 0 if we didn't get a hit
    return 0;
}
