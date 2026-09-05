#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int count_digits(char* str){
    int count = 0;
    while(*str != '\0'){
        if(*str >= '0' && *str <= '9'){
            count++;
        }
        str++;
    }
    return count;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <string>\n", argv[0]);
        return 1;
    }

    char* input_string = argv[1];
    int digit_count = count_digits(input_string);
    printf("String contains %d digits, right???\n", digit_count);

    return 0;
}