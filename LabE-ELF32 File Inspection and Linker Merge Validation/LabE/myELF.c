#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <elf.h>
#include <string.h>

#define MAX_ELF_FILES 2

typedef struct {
    char debug_mode;
    int elf_fd[MAX_ELF_FILES];
    void *elf_map[MAX_ELF_FILES];
    size_t elf_size[MAX_ELF_FILES];
    char elf_name[MAX_ELF_FILES][4096];
    int elf_count;
} state;

void toggle_debug_mode(state* s);
void examine_elf_file(state* s);
void print_section_names(state* s);
void print_symbols(state* s);
void quit(state* s);
/* Check every mapped range before interpreting headers or strings. */
static int in_range(size_t size, size_t offset, size_t length) {
    return offset <= size && length <= size - offset;
}

static int valid_string(const char *map, const Elf32_Shdr *table, size_t offset) {
    return offset < table->sh_size &&
        memchr(map + table->sh_offset + offset, 0, table->sh_size - offset) != NULL;
}

static int validate_elf(const char *map, size_t size) {
    if (size < sizeof(Elf32_Ehdr)) return 0;
    const Elf32_Ehdr *h = (const Elf32_Ehdr *)map;
    const unsigned short endian = 1;
    if (memcmp(h->e_ident, ELFMAG, SELFMAG) ||
        h->e_ident[EI_CLASS] != ELFCLASS32 ||
        h->e_ident[EI_DATA] != (*(const unsigned char *)&endian ? ELFDATA2LSB : ELFDATA2MSB) ||
        h->e_ident[EI_VERSION] != EV_CURRENT || h->e_version != EV_CURRENT ||
        h->e_ehsize != sizeof(*h)) return 0;
    if (h->e_phnum && (h->e_phentsize != sizeof(Elf32_Phdr) ||
        !in_range(size, h->e_phoff, (size_t)h->e_phnum * sizeof(Elf32_Phdr)))) return 0;
    if (!h->e_shnum) return h->e_shoff == 0 && h->e_shstrndx == SHN_UNDEF;
    if (h->e_shentsize != sizeof(Elf32_Shdr) ||
        h->e_shoff % _Alignof(Elf32_Shdr) ||
        !in_range(size, h->e_shoff, (size_t)h->e_shnum * sizeof(Elf32_Shdr)) ||
        h->e_shstrndx >= h->e_shnum) return 0;
    const Elf32_Shdr *sh = (const Elf32_Shdr *)(map + h->e_shoff);
    for (unsigned i = 0; i < h->e_shnum; ++i)
        if (sh[i].sh_type != SHT_NOBITS && !in_range(size, sh[i].sh_offset, sh[i].sh_size)) return 0;
    if (h->e_shstrndx != SHN_UNDEF && sh[h->e_shstrndx].sh_type != SHT_STRTAB) return 0;
    for (unsigned i = 0; i < h->e_shnum; ++i) {
        if (h->e_shstrndx != SHN_UNDEF && !valid_string(map, &sh[h->e_shstrndx], sh[i].sh_name)) return 0;
        if (sh[i].sh_type != SHT_SYMTAB && sh[i].sh_type != SHT_DYNSYM) continue;
        if (sh[i].sh_entsize != sizeof(Elf32_Sym) || sh[i].sh_size % sizeof(Elf32_Sym) ||
            sh[i].sh_offset % _Alignof(Elf32_Sym) || sh[i].sh_link >= h->e_shnum ||
            sh[sh[i].sh_link].sh_type != SHT_STRTAB) return 0;
        const Elf32_Sym *syms = (const Elf32_Sym *)(map + sh[i].sh_offset);
        for (size_t j = 0; j < sh[i].sh_size / sizeof(*syms); ++j)
            if (!valid_string(map, &sh[sh[i].sh_link], syms[j].st_name) ||
                (syms[j].st_shndx < SHN_LORESERVE && syms[j].st_shndx >= h->e_shnum) ||
                syms[j].st_shndx == SHN_XINDEX) return 0;
    }
    return 1;
}

static int input_line(char *buffer, size_t size) {
    if (!fgets(buffer, size, stdin)) return 0;
    char *newline = strchr(buffer, '\n');
    if (newline) *newline = 0;
    else if (!feof(stdin)) {
        int c;
        while ((c = getchar()) != '\n' && c != EOF) {}
        buffer[0] = 0;
        puts("Error: Input line too long.");
    }
    return 1;
}

void toggle_debug_mode(state* s) {
    if (s->debug_mode) {
        s->debug_mode = 0;
        printf("Debug mode off\n");
    } else {
        s->debug_mode = 1;
        printf("Debug mode on\n");
    }
}

void examine_elf_file(state* s) {
    if (s->elf_count >= MAX_ELF_FILES) {
        printf("Error: Can only handle up to %d ELF files.\n", MAX_ELF_FILES);
        return;
    }

    char file_name[4096];
    printf("Enter ELF file name: ");
    if (!input_line(file_name, sizeof(file_name)) || !file_name[0]) return;

    int fd = open(file_name, O_RDONLY);
    if (fd < 0) {
        perror("Error opening file");
        return;
    }

    off_t size = lseek(fd, 0, SEEK_END);
    if (size <= 0) {
        fprintf(stderr, "Error: Cannot examine an empty or unreadable file.\n");
        close(fd);
        return;
    }

    void *map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) {
        perror("Error mmapping file");
        close(fd);
        return;
    }

    Elf32_Ehdr *header = (Elf32_Ehdr *)map;
    if (!validate_elf(map, (size_t)size)) {
        printf("Error: Invalid or unsupported ELF32 file (requires native byte order and valid tables).\n");
        munmap(map, size);
        close(fd);
        return;
    }

    printf("Magic: %c%c%c\n", header->e_ident[EI_MAG1], header->e_ident[EI_MAG2], header->e_ident[EI_MAG3]);
    printf("Data: %s\n", header->e_ident[EI_DATA] == ELFDATA2LSB ? "2's complement, little endian" : "2's complement, big endian");
    printf("Entry point address: 0x%x\n", header->e_entry);
    printf("Start of section headers: %u (bytes into file)\n", header->e_shoff);
    printf("Number of section headers: %u\n", header->e_shnum);
    printf("Size of section headers: %u (bytes)\n", header->e_shentsize);
    printf("Start of program headers: %u (bytes into file)\n", header->e_phoff);
    printf("Number of program headers: %u\n", header->e_phnum);
    printf("Size of program headers: %u (bytes)\n", header->e_phentsize);

    if (s->debug_mode) {
        printf("Debug: Mapped ELF file %s, size %ld\n", file_name, size);
    }

    s->elf_fd[s->elf_count] = fd;
    s->elf_map[s->elf_count] = map;
    s->elf_size[s->elf_count] = size;
    strcpy(s->elf_name[s->elf_count], file_name);
    s->elf_count++;
}

void print_section_names(state* s) {
    if(s->elf_count == 0){
        printf("Error: No ELF files examined.\n");
        return;
    }

    for(int i =0;i<s->elf_count;i++){
        Elf32_Ehdr *header = (Elf32_Ehdr *)s->elf_map[i];
        Elf32_Shdr *sections = (Elf32_Shdr *)((char *)s->elf_map[i] + header->e_shoff);
        char *strtab = header->e_shstrndx == SHN_UNDEF ? NULL :
            (char *)s->elf_map[i] + sections[header->e_shstrndx].sh_offset;
        printf("File %s\n", s->elf_name[i]);
        for(int j = 0; j < header->e_shnum; j++){
            static const char *types[] = {"NULL", "PROGBITS", "SYMTAB", "STRTAB", "RELA", "HASH", "DYNAMIC", "NOTE", "NOBITS", "REL", "SHLIB", "DYNSYM", "UNKNOWN", "UNKNOWN", "INIT_ARRAY", "FINI_ARRAY", "PREINIT_ARRAY", "GROUP", "SYMTAB_SHNDX"};
            unsigned type = sections[j].sh_type;
            printf("[%2d] %s %08x %06x %06x %s (%u)\n", j,
                strtab ? &strtab[sections[j].sh_name] : "<no name>", sections[j].sh_addr,
                sections[j].sh_offset, sections[j].sh_size,
                type < sizeof(types)/sizeof(types[0]) ? types[type] : "UNKNOWN", type);
            if (s->debug_mode) printf("Debug: section[%d].sh_name=%u\n", j, sections[j].sh_name);
        }
        if (s->debug_mode) {
            printf("Debug: e_shoff=%u, e_shnum=%u, e_shstrndx=%u\n", header->e_shoff, header->e_shnum, header->e_shstrndx);
        }
    }
}

const char* get_section_name(state* s, int file_index, int section_index) {
    Elf32_Ehdr *header = (Elf32_Ehdr *)s->elf_map[file_index];
    Elf32_Shdr *sections = (Elf32_Shdr *)((char *)s->elf_map[file_index] + header->e_shoff);
    if (section_index == SHN_UNDEF) return "UND";
    if (section_index == SHN_ABS) return "ABS";
    if (section_index == SHN_COMMON) return "COMMON";
    if (section_index >= header->e_shnum) return "RESERVED";
    if (header->e_shstrndx == SHN_UNDEF) return "<no name>";
    Elf32_Shdr *strtab_section = &sections[header->e_shstrndx];
    if (strtab_section->sh_type != SHT_STRTAB) {
        return "<no name>";
    }
    char *strtab = (char *)s->elf_map[file_index] + strtab_section->sh_offset;
    return &strtab[sections[section_index].sh_name];
}

void print_symbols(state* s) {
    if (s->elf_count == 0) {
        printf("Error: No ELF files examined.\n");
        return;
    }

    for (int i = 0; i < s->elf_count; i++) {
        Elf32_Ehdr *header = (Elf32_Ehdr *)s->elf_map[i];
        Elf32_Shdr *sections = (Elf32_Shdr *)((char *)s->elf_map[i] + header->e_shoff);
        int found = 0;
        printf("File %s\n", s->elf_name[i]);
        for (int k = 0; k < header->e_shnum; ++k) {
            if (sections[k].sh_type != SHT_SYMTAB && sections[k].sh_type != SHT_DYNSYM) continue;
            found = 1;
            const char *strtab = (char *)s->elf_map[i] + sections[sections[k].sh_link].sh_offset;
            const Elf32_Sym *symtab = (Elf32_Sym *)((char *)s->elf_map[i] + sections[k].sh_offset);
            unsigned count = sections[k].sh_size / sizeof(*symtab);
            if (s->debug_mode)
                printf("Debug: symtab_offset=%u, symtab_size=%u, sym_count=%u, sh_link=%u\n",
                       sections[k].sh_offset, sections[k].sh_size, count, sections[k].sh_link);
            puts("[index] value section_index section_name symbol_name");
            for (unsigned j = 0; j < count; ++j)
                printf("[%2u] %08x %u %s %s\n", j, symtab[j].st_value, symtab[j].st_shndx,
                       get_section_name(s, i, symtab[j].st_shndx), strtab + symtab[j].st_name);
        }
        if (!found) puts("Error: No symbol table found.");
    }
}

void check_merge(state* s) {
    if (s->elf_count != 2) {
        printf("Error: Two ELF files must be opened and examined.\n");
        return;
    }

    Elf32_Ehdr *headers[MAX_ELF_FILES];
    Elf32_Shdr *sections[MAX_ELF_FILES];
    Elf32_Shdr *symtab_sections[MAX_ELF_FILES];
    Elf32_Sym *symtabs[MAX_ELF_FILES];
    int symtab_counts[MAX_ELF_FILES] = {0};

    for (int i = 0; i < MAX_ELF_FILES; i++) {
        headers[i] = (Elf32_Ehdr *)s->elf_map[i];
        sections[i] = (Elf32_Shdr *)((char *)s->elf_map[i] + headers[i]->e_shoff);

        int symtab_index = -1;
        for (int j = 0; j < headers[i]->e_shnum; j++) {
            if (sections[i][j].sh_type == SHT_SYMTAB) {
                if (symtab_index != -1) {
                    printf("Feature not supported: ELF file %d contains more than one symbol table.\n", i);
                    return;
                }
                symtab_index = j;
            }
        }

        if (symtab_index == -1) {
            printf("Feature not supported: ELF file %d contains no symbol table.\n", i);
            return;
        }

        symtab_sections[i] = &sections[i][symtab_index];
        symtabs[i] = (Elf32_Sym *)((char *)s->elf_map[i] + symtab_sections[i]->sh_offset);
        symtab_counts[i] = symtab_sections[i]->sh_size / sizeof(Elf32_Sym);
    }

    for (int i = 0; i < symtab_counts[0]; i++) {
        if (i == 0) continue; // Skip dummy symbol
        Elf32_Sym *sym = &symtabs[0][i];
        const char *sym_name = (char *)s->elf_map[0] + sections[0][symtab_sections[0]->sh_link].sh_offset + sym->st_name;

        if (ELF32_ST_BIND(sym->st_info) == STB_LOCAL || !*sym_name) continue;

        if (sym->st_shndx == SHN_UNDEF) {
            int found = 0;
            for (int j = 0; j < symtab_counts[1]; j++) {
                if (j == 0) continue; // Skip dummy symbol
                Elf32_Sym *sym2 = &symtabs[1][j];
                const char *sym2_name = (char *)s->elf_map[1] + sections[1][symtab_sections[1]->sh_link].sh_offset + sym2->st_name;

                if (ELF32_ST_BIND(sym2->st_info) != STB_LOCAL && sym2->st_shndx != SHN_UNDEF && strcmp(sym_name, sym2_name) == 0) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                printf("Symbol %s undefined\n", sym_name);
            }
        } else {
            for (int j = 0; j < symtab_counts[1]; j++) {
                if (j == 0) continue; // Skip dummy symbol
                Elf32_Sym *sym2 = &symtabs[1][j];
                const char *sym2_name = (char *)s->elf_map[1] + sections[1][symtab_sections[1]->sh_link].sh_offset + sym2->st_name;

                if (ELF32_ST_BIND(sym2->st_info) != STB_LOCAL && strcmp(sym_name, sym2_name) == 0 && sym2->st_shndx != SHN_UNDEF) {
                    printf("Symbol %s multiply defined\n", sym_name);
                }
            }
        }
    }

    for (int i = 0; i < symtab_counts[1]; i++) {
        if (i == 0) continue; // Skip dummy symbol
        Elf32_Sym *sym = &symtabs[1][i];
        const char *sym_name = (char *)s->elf_map[1] + sections[1][symtab_sections[1]->sh_link].sh_offset + sym->st_name;

        if (ELF32_ST_BIND(sym->st_info) == STB_LOCAL || !*sym_name) continue;

        if (sym->st_shndx == SHN_UNDEF) {
            int found = 0;
            for (int j = 0; j < symtab_counts[0]; j++) {
                if (j == 0) continue; // Skip dummy symbol
                Elf32_Sym *sym2 = &symtabs[0][j];
                const char *sym2_name = (char *)s->elf_map[0] + sections[0][symtab_sections[0]->sh_link].sh_offset + sym2->st_name;

                if (ELF32_ST_BIND(sym2->st_info) != STB_LOCAL && sym2->st_shndx != SHN_UNDEF && strcmp(sym_name, sym2_name) == 0) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                printf("Symbol %s undefined\n", sym_name);
            }
        }
    }
}

void quit(state* s) {
    for (int i = 0; i < s->elf_count; i++) {
        munmap(s->elf_map[i], s->elf_size[i]);
        close(s->elf_fd[i]);
    }
    exit(0);
}

static void merge_files(state *s) {
    (void)s;
    puts("Not implemented yet: part 3.2 is an optional bonus.");
}

int main(void) {
    state s = {.elf_fd = {-1, -1}};
    const struct { const char *name; void (*action)(state *); } menu[] = {
        {"Toggle Debug Mode", toggle_debug_mode}, {"Examine ELF File", examine_elf_file},
        {"Print Section Names", print_section_names}, {"Print Symbols", print_symbols},
        {"Check Files for Merge", check_merge}, {"Merge ELF Files", merge_files}, {"Quit", quit}
    };
    for (;;) {
        puts("Choose action:");
        for (unsigned i = 0; i < sizeof(menu)/sizeof(menu[0]); ++i)
            printf("%u-%s\n", i, menu[i].name);
        printf("Option: ");
        char line[128], extra;
        int option;
        if (!input_line(line, sizeof(line))) quit(&s);
        if (sscanf(line, "%d %c", &option, &extra) != 1 || option < 0 || option > 6) {
            puts("Invalid option");
            continue;
        }
        menu[option].action(&s);
    }
}
