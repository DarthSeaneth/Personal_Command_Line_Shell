#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <linux/limits.h>
#include "arraylist.h"
#ifndef BUFSIZE
#define BUFSIZE 500
#endif

int main(int argc, char** argv) {
    char buffer[BUFSIZE];
    read(STDIN_FILENO, buffer, BUFSIZE);
    printf("%s", buffer);
    return EXIT_SUCCESS;
}