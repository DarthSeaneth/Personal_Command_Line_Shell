#ifndef _ARRAYLIST_H
#define _ARRAYLIST_H

typedef struct{
    unsigned int size;
    unsigned int capacity;
    char **data;
} array_list;

int init(array_list *list, unsigned int capacity);
void destroy(array_list *list);
unsigned int get_length(array_list *list);
int search(char *dest, array_list *list, unsigned int index);
int insert(array_list *list, unsigned int index, char *src);
int push(array_list *list, char *src);
int pop(char *dest, array_list *list);

#endif