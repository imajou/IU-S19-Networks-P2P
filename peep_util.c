//
// Created by Gleb Petrakov on 2019-03-19.
//

#include "peep_util.h"

int count_words(const char *string) {
    int index = 0;
    int count = 0;
    while (string[index] != '\0') {
        if (string[index] == ' ')
            ++count;
        ++index;
    }
    return ++count;
}

void remove_all_chars(char *str, char c) {
    char *pr = str, *pw = str;
    while (*pr) {
        *pw = *pr++;
        pw += (*pw != c);
    }
    *pw = '\0';
}
