#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 10240

char sigFileName[256] = "signatures-L";
static int big_endian;
static int read_error;

typedef struct virus {
    unsigned short SigSize;
    char virusName[16];
    unsigned char *sig;
} virus;

typedef struct link {
    struct link *nextVirus;
    virus *vir;
} link;

/* Read a complete input line, discarding an overlong line. */
static int read_line(char *buffer, size_t size) {
    if (!fgets(buffer, (int)size, stdin)) return 0;
    char *newline = strchr(buffer, '\n');
    if (newline) *newline = '\0';
    else if (!feof(stdin)) {
        int c;
        while ((c = getchar()) != '\n' && c != EOF) {}
        buffer[0] = '\0';
        fprintf(stderr, "Input too long.\n");
    }
    return 1;
}

void SetSigFileName(void) {
    char input[sizeof(sigFileName)];
    printf("Signature file (Enter keeps %s): ", sigFileName);
    if (read_line(input, sizeof(input)) && input[0])
        strcpy(sigFileName, input);
}

virus *readVirus(FILE *file) {
    /* The required on-disk header occupies the first 18 struct bytes. */
    _Static_assert(offsetof(virus, virusName) == 2, "Unexpected virus layout");
    virus *vir = malloc(sizeof(*vir));
    if (!vir) {
        perror("Allocate virus");
        read_error = 1;
        return NULL;
    }
    size_t count = fread(vir, 1, 18, file);
    if (count == 0 && !ferror(file)) {
        free(vir);
        return NULL;
    }
    if (count != 18) {
        fprintf(stderr, "Truncated or unreadable signature header.\n");
        free(vir);
        read_error = 1;
        return NULL;
    }
    unsigned char *raw = (unsigned char *)vir;
    unsigned short length = (unsigned short)(big_endian
        ? ((unsigned int)raw[0] << 8) | raw[1]
        : ((unsigned int)raw[1] << 8) | raw[0]);
    vir->SigSize = length;
    if (!length) {
        fprintf(stderr, "Zero-length signature is invalid.\n");
        free(vir);
        read_error = 1;
        return NULL;
    }
    vir->sig = malloc(length);
    if (!vir->sig) {
        perror("Allocate signature");
        free(vir);
        read_error = 1;
        return NULL;
    }
    if (fread(vir->sig, 1, length, file) != length) {
        fprintf(stderr, "Truncated or unreadable signature data.\n");
        free(vir->sig);
        free(vir);
        read_error = 1;
        return NULL;
    }
    return vir;
}

static void print_virus(FILE *stream, const virus *vir) {
    fprintf(stream, "Virus name: %.16s\nVirus signature length: %u\nVirus signature: ",
            vir->virusName, (unsigned int)vir->SigSize);
    for (unsigned int i = 0; i < vir->SigSize; ++i)
        fprintf(stream, "%02X ", vir->sig[i]);
    fputc('\n', stream);
}

void printVirus(virus *vir) { print_virus(stdout, vir); }

void list_print(link *virus_list, FILE *stream) {
    for (; virus_list; virus_list = virus_list->nextVirus) {
        print_virus(stream, virus_list->vir);
        fputc('\n', stream);
    }
}

/* On allocation failure, the caller retains ownership of data and the list. */
link *list_append(link *virus_list, virus *data) {
    link *node = malloc(sizeof(*node));
    if (!node) return NULL;
    node->vir = data;
    node->nextVirus = virus_list;
    return node;
}

void list_free(link *virus_list) {
    while (virus_list) {
        link *next = virus_list->nextVirus;
        free(virus_list->vir->sig);
        free(virus_list->vir);
        free(virus_list);
        virus_list = next;
    }
}

void detect_virus(char *buffer, unsigned int size, link *virus_list) {
    for (link *node = virus_list; node; node = node->nextVirus) {
        virus *vir = node->vir;
        if (!vir->SigSize || vir->SigSize > size) continue;
        for (unsigned int i = 0; i <= size - vir->SigSize; ++i) {
            if (memcmp(buffer + i, vir->sig, vir->SigSize) == 0)
                printf("Virus detected!\nStarting byte location: %u\n"
                       "Virus name: %.16s\nVirus signature size: %u\n",
                       i, vir->virusName, (unsigned int)vir->SigSize);
        }
    }
}

void neutralize_virus(char *fileName, int signatureOffset) {
    FILE *file = fopen(fileName, "r+b");
    if (!file) { perror("Open suspected file for repair"); return; }
    unsigned char ret = 0xC3;
    int ok = signatureOffset >= 0 &&
             fseek(file, signatureOffset, SEEK_SET) == 0 &&
             fwrite(&ret, 1, 1, file) == 1;
    if (fclose(file) != 0) ok = 0;
    if (ok) printf("Neutralized virus at location: %d\n", signatureOffset);
    else fprintf(stderr, "Failed to neutralize virus at location: %d\n", signatureOffset);
}

/* Keep the previous list if opening or parsing a replacement fails.
 * Invalid magic is fatal, as required by part 1a. */
static int load_signatures(link **list) {
    FILE *file = fopen(sigFileName, "rb");
    if (!file) { perror("Open signatures"); return 0; }
    char magic[4];
    if (fread(magic, 1, 4, file) != 4 ||
        (memcmp(magic, "VIRL", 4) && memcmp(magic, "VIRB", 4))) {
        fprintf(stderr, "Invalid signature file magic (expected VIRL or VIRB).\n");
        fclose(file);
        return -1;
    }
    big_endian = memcmp(magic, "VIRB", 4) == 0;
    read_error = 0;
    link *new_list = NULL;
    virus *vir;
    while ((vir = readVirus(file)) != NULL) {
        link *head = list_append(new_list, vir);
        if (!head) {
            perror("Allocate list node");
            free(vir->sig);
            free(vir);
            read_error = 1;
            break;
        }
        new_list = head;
    }
    fclose(file);
    if (read_error) { list_free(new_list); return 0; }
    list_free(*list);
    *list = new_list;
    puts("Signatures loaded successfully.");
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s FILE\n", argv[0]);
        return EXIT_FAILURE;
    }
    link *virus_list = NULL;
    int status = EXIT_SUCCESS;
    char choice[64];
    for (;;) {
        printf("0) Set signatures file name\n1) Load signatures\n"
               "2) Print signatures\n3) Detect viruses\n4) Fix file\n5) Quit\nEnter choice: ");
        if (!read_line(choice, sizeof(choice))) break;
        char option, extra;
        if (sscanf(choice, " %c %c", &option, &extra) != 1 || option < '0' || option > '5') {
            puts("INVALID OPTION!");
            continue;
        }
        if (option == '5') break;
        if (option == '0') SetSigFileName();
        else if (option == '1') {
            if (load_signatures(&virus_list) < 0) { status = EXIT_FAILURE; break; }
        } else if (option == '2') list_print(virus_list, stdout);
        else {
            FILE *file = fopen(argv[1], "rb");
            if (!file) { perror("Open suspected file"); continue; }
            char buffer[BUFFER_SIZE];
            size_t size = fread(buffer, 1, sizeof(buffer), file);
            int failed = ferror(file);
            fclose(file);
            if (failed) { fprintf(stderr, "Failed to read suspected file.\n"); continue; }
            if (option == '3') detect_virus(buffer, (unsigned int)size, virus_list);
            else {
                /* Scan the original snapshot so overlapping matches are all repaired. */
                for (link *node = virus_list; node; node = node->nextVirus) {
                    virus *vir = node->vir;
                    if (!vir->SigSize || vir->SigSize > size) continue;
                    for (size_t i = 0; i <= size - vir->SigSize; ++i)
                        if (memcmp(buffer + i, vir->sig, vir->SigSize) == 0)
                            neutralize_virus(argv[1], (int)i);
                }
            }
        }
    }
    list_free(virus_list);
    return status;
}
