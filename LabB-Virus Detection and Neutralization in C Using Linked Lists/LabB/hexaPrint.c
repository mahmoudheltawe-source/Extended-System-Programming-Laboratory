#include <stdio.h>

void PrintHex(const unsigned char *buffer, size_t length) {
    for (size_t i = 0; i < length; ++i)
        printf("%02X ", buffer[i]);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s FILE\n", argv[0]);
        return 1;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) { perror("Open file"); return 1; }
    unsigned char buffer[4096];
    size_t count;
    while ((count = fread(buffer, 1, sizeof(buffer), file)) != 0)
        PrintHex(buffer, count);
    int failed = ferror(file);
    fclose(file);
    if (failed) { fprintf(stderr, "Failed to read file.\n"); return 1; }
    putchar('\n');
    return 0;
}
