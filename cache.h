// vim: set et ts=4 sts=4 sw=4:
#ifndef CACHE_H
#define CACHE_H

#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

struct cache;
struct web_object;

struct cache {
    int size;
    struct web_object *first;
};

struct web_object {
    int size; // i.e. strlen(buf)
    char *URL;
    char *buf;
    time_t timestamp;
    struct web_object *next;
    struct web_object *prev;
};

typedef struct cache * cache;
typedef struct web_object * web_object;

void print_cache(cache C);

cache new_cache();
web_object new_webobj(char *URL, char *buf, int len);
void insert_webobj(cache C, char *URL, char *buf, int len);
int read_cache(cache C, char *URL, char *buf);
web_object getLRU(cache C);
void free_webobj(web_object W);
void evict_webobj(cache C, web_object W1, web_object W2);
void remove_webobj(cache C, web_object W);

#endif
