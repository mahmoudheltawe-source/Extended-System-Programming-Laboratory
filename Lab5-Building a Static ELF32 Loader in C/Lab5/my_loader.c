/* Freestanding Linux/i386 static ELF loader. No libc is linked. */
#include <elf.h>

#define PAGE 4096u
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define MAP_PRIVATE 2
#define MAP_FIXED 16
#define MAP_ANONYMOUS 32
#define MAP_FIXED_NOREPLACE 0x100000

extern int system_call(int number, ...);
extern int startup(int argc, char **argv, void (*entry)(void));
static unsigned file_size, ph_count;
static void *file_map;
static int failed;

static unsigned length(const char *s) {
    unsigned n = 0;
    while (s[n]) ++n;
    return n;
}
static void out(const char *s) { system_call(4, 1, s, length(s)); }
static int error(const char *s) {
    system_call(4, 2, s, length(s));
    system_call(4, 2, "\n", 1);
    failed = 1;
    return -1;
}
static void number(unsigned n, unsigned base) {
    char b[33];
    unsigned i = sizeof(b);
    b[--i] = 0;
    do { b[--i] = "0123456789abcdef"[n % base]; n /= base; } while (n);
    out(b + i);
}
static void hex(unsigned n) { out("0x"); number(n, 16); }
static int equal(const char *a, const char *b) {
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}
static int bad_result(int r) { return (unsigned)r >= (unsigned)-4095; }
/* i386 old_mmap takes all six arguments through a pointer. */
static void *map(unsigned addr, unsigned size, int prot, int flags,
                 int fd, unsigned offset) {
    unsigned args[6] = {addr, size, (unsigned)prot, (unsigned)flags,
                        (unsigned)fd, offset};
    return (void *)system_call(90, args, 0, 0);
}
static int within(unsigned offset, unsigned size) {
    return offset <= file_size && size <= file_size - offset;
}
static int validate(void *base) {
    Elf32_Ehdr *h = base;
    if (file_size < sizeof(*h)) return error("File is shorter than an ELF32 header");
    if (h->e_ident[0] != 127 || h->e_ident[1] != 'E' ||
        h->e_ident[2] != 'L' || h->e_ident[3] != 'F' ||
        h->e_ident[EI_CLASS] != ELFCLASS32 ||
        h->e_ident[EI_DATA] != ELFDATA2LSB ||
        h->e_ident[EI_VERSION] != EV_CURRENT || h->e_version != EV_CURRENT)
        return error("Expected a little-endian ELF32 file");
    if (h->e_ehsize != sizeof(*h)) return error("Invalid ELF header size");
    ph_count = h->e_phnum;
    if (ph_count == PN_XNUM) {
        if (h->e_shentsize != sizeof(Elf32_Shdr) ||
            !within(h->e_shoff, sizeof(Elf32_Shdr)))
            return error("Invalid extended program-header count");
        ph_count = ((Elf32_Shdr *)((char *)base + h->e_shoff))->sh_info;
    }
    if (ph_count && (h->e_phentsize != sizeof(Elf32_Phdr) ||
        h->e_phoff > file_size ||
        ph_count > (file_size - h->e_phoff) / sizeof(Elf32_Phdr)))
        return error("Program-header table is outside the file");
    return 0;
}

/* Normalize big-endian ELF metadata in the private map for inspection only. */
static void reverse_bytes(unsigned char *p, unsigned n) {
    for (unsigned i = 0; i < n / 2; ++i) {
        unsigned char t = p[i]; p[i] = p[n - i - 1]; p[n - i - 1] = t;
    }
}
static int prepare_headers(int *big_endian) {
    Elf32_Ehdr *h = file_map;
    if (file_size < sizeof(*h)) return validate(file_map);
    *big_endian = h->e_ident[EI_DATA] == ELFDATA2MSB;
    if (*big_endian && h->e_ident[EI_CLASS] == ELFCLASS32) {
        unsigned char *b = file_map;
        reverse_bytes(b + 16, 2); reverse_bytes(b + 18, 2);
        for (unsigned i = 20; i < 40; i += 4) reverse_bytes(b + i, 4);
        for (unsigned i = 40; i < 52; i += 2) reverse_bytes(b + i, 2);
        h->e_ident[EI_DATA] = ELFDATA2LSB;
        if (h->e_phnum == PN_XNUM && within(h->e_shoff, sizeof(Elf32_Shdr)))
            reverse_bytes(b + h->e_shoff + 28, 4); /* section zero sh_info */
    }
    if (validate(file_map) < 0) return -1;
    if (*big_endian) {
        unsigned char *p = (unsigned char *)file_map + h->e_phoff;
        for (unsigned i = 0; i < ph_count * sizeof(Elf32_Phdr); i += 4)
            reverse_bytes(p + i, 4);
    }
    return 0;
}

int foreach_phdr(void *map_start, void (*func)(Elf32_Phdr *, int), int arg) {
    Elf32_Ehdr *h = map_start;
    if (validate(map_start) < 0) return -1;
    for (unsigned i = 0; i < ph_count; ++i) {
        func((Elf32_Phdr *)((char *)map_start + h->e_phoff + i * h->e_phentsize), arg);
        if (failed) return -1;
    }
    return 0;
}
static int protection(Elf32_Phdr *p) {
    return ((p->p_flags & PF_R) ? PROT_READ : 0) |
           ((p->p_flags & PF_W) ? PROT_WRITE : 0) |
           ((p->p_flags & PF_X) ? PROT_EXEC : 0);
}
static const char *type_name(unsigned type) {
    switch (type) {
    case PT_NULL: return "NULL";
    case PT_LOAD: return "LOAD";
    case PT_DYNAMIC: return "DYNAMIC";
    case PT_INTERP: return "INTERP";
    case PT_NOTE: return "NOTE";
    case PT_SHLIB: return "SHLIB";
    case PT_PHDR: return "PHDR";
    case PT_TLS: return "TLS";
    case PT_GNU_EH_FRAME: return "GNU_EH_FRAME";
    case PT_GNU_STACK: return "GNU_STACK";
    case PT_GNU_RELRO: return "GNU_RELRO";
    default: return "OTHER";
    }
}
static void print_phdr(Elf32_Phdr *p, int arg) {
    (void)arg;
    out(type_name(p->p_type)); out("("); hex(p->p_type); out(")\t");
    hex(p->p_offset); out("\t"); hex(p->p_vaddr); out("\t");
    hex(p->p_paddr); out("\t"); hex(p->p_filesz); out("\t");
    hex(p->p_memsz); out("\t");
    out((p->p_flags & PF_R) ? "R" : "-");
    out((p->p_flags & PF_W) ? "W" : "-");
    out((p->p_flags & PF_X) ? "E" : "-");
    out("\t"); hex(p->p_align); out("\n");
    if (p->p_type == PT_LOAD) {
        out("  mmap protection="); hex(protection(p));
        out(" mapping=MAP_PRIVATE|MAP_FIXED (0x12), anonymous pages for BSS\n");
    }
}
static unsigned header_index;
static void visit(Elf32_Phdr *p, int arg) {
    (void)arg;
    out("Program header number "); number(header_index++, 10);
    out(" at address "); hex((unsigned)p); out("\n");
}
static int executable_entry;
static void check_load(Elf32_Phdr *p, int arg) {
    (void)arg;
    if (p->p_type == PT_INTERP || p->p_type == PT_DYNAMIC) {
        error("Dynamic executables are unsupported; use --headers to inspect"); return;
    }
    if (p->p_type != PT_LOAD) return;
    if (p->p_filesz > p->p_memsz || !within(p->p_offset, p->p_filesz) ||
        p->p_memsz > 0xffffffffu - p->p_vaddr ||
        ((p->p_vaddr - p->p_offset) & (PAGE - 1)) ||
        (p->p_align > 1 && ((p->p_align & (p->p_align - 1)) ||
         ((p->p_vaddr - p->p_offset) & (p->p_align - 1))))) {
        error("Invalid LOAD segment bounds or alignment"); return;
    }
    unsigned entry = ((Elf32_Ehdr *)file_map)->e_entry;
    if ((p->p_flags & PF_X) && entry >= p->p_vaddr &&
        entry - p->p_vaddr < p->p_memsz) executable_entry = 1;
}
void load_phdr(Elf32_Phdr *p, int fd) {
    if (p->p_type != PT_LOAD || !p->p_memsz) return;
    unsigned base = p->p_vaddr & ~(PAGE - 1);
    unsigned padding = p->p_vaddr - base;
    unsigned size = padding + p->p_memsz;
    if (size > 0xffffffffu - (PAGE - 1)) { error("LOAD size overflow"); return; }
    size = (size + PAGE - 1) & ~(PAGE - 1);
    /* Reserve without replacing the loader, stack, file map, or another segment. */
    void *r = map(base, size, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (bad_result((int)r) || r != (void *)base) {
        if (!bad_result((int)r)) system_call(91, r, size, 0);
        error("Cannot reserve LOAD address (collision or unsupported kernel)"); return;
    }
    if (p->p_filesz) {
        r = map(base, padding + p->p_filesz, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_FIXED, fd, p->p_offset - padding);
        if (bad_result((int)r)) { error("Cannot map LOAD file bytes"); return; }
    }
    /* Includes the partial file page and all anonymous BSS pages. */
    unsigned char *bss = (unsigned char *)(p->p_vaddr + p->p_filesz);
    for (unsigned i = 0; i < p->p_memsz - p->p_filesz; ++i) bss[i] = 0;
    if (bad_result(system_call(125, base, size, protection(p)))) {
        error("Cannot set LOAD permissions"); return;
    }
    out("Mapped "); print_phdr(p, fd);
}
int main(int argc, char **argv) {
    int mode = 0, index = 1, big_endian = 0;
    if (argc > 1 && equal(argv[1], "--headers")) { mode = 1; index = 2; }
    if (argc > 1 && equal(argv[1], "--iterate")) { mode = 2; index = 2; }
    if (argc <= index) {
        error("Usage: my_loader [--headers|--iterate] ELF [program arguments ...]"); return 1;
    }
    int fd = system_call(5, argv[index], 0, 0);
    if (bad_result(fd)) { error("Cannot open ELF file"); return 1; }
    int size = system_call(19, fd, 0, 2);
    if (size <= 0) { error("Cannot read file size, or file is empty"); goto close_file; }
    file_size = (unsigned)size;
    file_map = map(0, file_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (bad_result((int)file_map)) { error("Cannot map ELF file"); goto close_file; }
    if (prepare_headers(&big_endian) < 0) goto unmap_file;
    if (foreach_phdr(file_map, mode == 2 ? visit : print_phdr, 0) < 0) goto unmap_file;
    if (!mode) {
        Elf32_Ehdr *h = file_map;
        if (big_endian || h->e_type != ET_EXEC || h->e_machine != EM_386) {
            error("Execution requires a static i386 ET_EXEC file"); goto unmap_file;
        }
        if (foreach_phdr(file_map, check_load, fd) < 0) goto unmap_file;
        if (!executable_entry) { error("Entry point is outside executable LOAD segments"); goto unmap_file; }
        if (foreach_phdr(file_map, load_phdr, fd) < 0) goto unmap_file;
        startup(argc - index, argv + index, (void (*)(void))h->e_entry);
    }
unmap_file:
    system_call(91, file_map, file_size, 0);
close_file:
    system_call(6, fd, 0, 0);
    return failed ? 1 : 0;
}
