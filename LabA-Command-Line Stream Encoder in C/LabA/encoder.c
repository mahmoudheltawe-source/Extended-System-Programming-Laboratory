#include <stdio.h>

/* Advance the key for every byte, including unchanged characters. */
static int encoding(FILE *input, FILE *output, const char *key, int direction)
{
    const char *digit = key;
    int c;

    while ((c = fgetc(input)) != EOF) {
        if (key != NULL) {
            int shift = direction * (*digit - '0');
            if (c >= 'a' && c <= 'z')
                c = 'a' + (c - 'a' + shift + 26) % 26;
            else if (c >= '0' && c <= '9')
                c = '0' + (c - '0' + shift + 10) % 10;
            if (*++digit == '\0')
                digit = key;
        }
        if (fputc(c, output) == EOF) {
            fprintf(stderr, "Cannot write output\n");
            return 1;
        }
    }
    if (ferror(input)) {
        fprintf(stderr, "Cannot read input\n");
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    FILE *input = stdin;
    FILE *output = stdout;
    const char *key = NULL;
    const char *input_name = NULL;
    const char *output_name = NULL;
    int direction = 1;
    int debug = 1;
    int status;
    int i;

    for (i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (debug)
            fprintf(stderr, "%s\n", arg);

        if ((arg[0] == '+' || arg[0] == '-') &&
            arg[1] == 'D' && arg[2] == '\0') {
            debug = arg[0] == '+';
        } else if ((arg[0] == '+' || arg[0] == '-') && arg[1] == 'e') {
            const char *digit = arg + 2;
            if (key != NULL || *digit == '\0') {
                fprintf(stderr, "Expected one non-empty encoding key\n");
                return 1;
            }
            for (; *digit != '\0'; ++digit) {
                if (*digit < '0' || *digit > '9') {
                    fprintf(stderr, "Encoding key must contain only decimal digits\n");
                    return 1;
                }
            }
            key = arg + 2;
            direction = arg[0] == '+' ? 1 : -1;
        } else if (arg[0] == '-' && arg[1] == 'I' && arg[2] != '\0' &&
                   input_name == NULL) {
            input_name = arg + 2;
        } else if (arg[0] == '-' && arg[1] == 'O' && arg[2] != '\0' &&
                   output_name == NULL) {
            output_name = arg + 2;
        } else {
            fprintf(stderr, "Invalid or repeated argument: %s\n", arg);
            return 1;
        }
    }

    if (input_name != NULL) {
        input = fopen(input_name, "rb");
        if (input == NULL) {
            perror(input_name);
            return 1;
        }
    }
    if (output_name != NULL) {
        output = fopen(output_name, "wb");
        if (output == NULL) {
            perror(output_name);
            if (input != stdin)
                fclose(input);
            return 1;
        }
    }

    status = encoding(input, output, key, direction);
    if (input != stdin && fclose(input) == EOF) {
        fprintf(stderr, "Cannot close input\n");
        status = 1;
    }
    if (fclose(output) == EOF) {
        fprintf(stderr, "Cannot finish writing output\n");
        status = 1;
    }
    return status;
}
