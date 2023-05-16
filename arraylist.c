#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "arraylist.h"

#ifndef DEBUG
#define DEBUG 0
#endif

#ifdef SAFE
#define SAFETY_CHECK \
if(list->data == NULL){ \
    fputs("Use of unallocated or destroyed arraylist\n", stderr); \
    exit(EXIT_FAILURE); \
}
#else
#define SAFETY_CHECK
#endif

/* Initializes empty arraylist with specified capacity (must be greater than 0)
 * Returns 1 on success or 0 if not able to allocate storage
 */
int init(array_list *list, unsigned int capacity){
    if(DEBUG > 1) fprintf(stderr, "Initializing %p with %u\n", list, capacity);
    assert(capacity > 0);
    list->capacity = capacity;
    list->size = 0;
    list->data = malloc(sizeof(char *) * capacity);
    return list->data != NULL;
}

/*
 * Destroys arraylist by freeing its data
 */
void destroy(array_list *list){
    if(DEBUG > 1) fprintf(stderr, "Destroying %p\n", list);
    SAFETY_CHECK
    for(int i = 0; i < list->size; i ++){
        free(list->data[i]);
    }
    free(list->data);

#ifdef SAFE
    list->data = NULL;
#endif
}

/*
 * Returns current size of given arraylist
 */
unsigned int get_length(array_list *list){
    return list->size;
}

/* Writes string stored at index to specified destination
 * Returns 1 on success or 0 if index is out of range
 */
int search(char *dest, array_list *list, unsigned int index){
    SAFETY_CHECK

    if(index >= list->size) return 0;
    strcpy(dest, list->data[index]);
    return 1;
}

/* Writes string to specified index in arraylist (does not extend length)
 * Returns 1 on success or 0 if index is out of range
 */
int insert(array_list *list, unsigned int index, char *src){
    SAFETY_CHECK

    if(index >= list->size) return 0;
    int length = strlen(src);
    list->data[index] = malloc(sizeof(char) * (length + 1));
    strcpy(list->data[index], src);
    return 1;
}

/* Appends string to end of arraylist
 * Returns 1 on success or 0 on failure
 */
int push(array_list *list, char *src){
    if(DEBUG > 1) fprintf(stderr, "Push %p: %s\n", list, src);
    SAFETY_CHECK

    if(list->size == list->capacity){
        int newcap = list->capacity * 2;
        char **new = realloc(list->data, sizeof(char *) * newcap);
        if(DEBUG) fprintf(stderr, "Increase capacity of %p to %d\n", list, newcap);
        if(!new) return 0;
        //NOTE no changes made until allocation is successful
        list->data = new;
        list->capacity = newcap;
    }
    int length = strlen(src);
    list->data[list->size] = malloc(sizeof(char) * (length + 1));
    strcpy(list->data[list->size], src);
    ++list->size;
    return 1;
}

/* Copies last entry in arraylist to specified destination and reduces list size by 1
 * Returns 1 on success or 0 if arraylist is empty
 */
int pop(char *dest, array_list *list){
    if(DEBUG > 1) fprintf(stderr, "Pop %p\n", list);
    SAFETY_CHECK

    if(list->size == 0) return 0;
    --list->size;
    strcpy(dest, list->data[list->size]);
    free(list->data[list->size]);
    return 1;
}