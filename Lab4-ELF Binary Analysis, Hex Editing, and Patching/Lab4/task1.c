#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>

typedef struct {
    char debug_mode;
    char file_name[128];
    int unit_size;
    unsigned char mem_buf[10000];
    size_t mem_count; /* Number of initialized bytes, independent of unit size. */
    int display_flag;
} state;

typedef struct { const char *name; void (*func)(state *); } menu;

static void input(char *line, size_t size) {
    if (!fgets(line, size, stdin)) exit(0);
    if (!strchr(line, '\n') && !feof(stdin)) {
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) {}
        line[0] = '\0';
    }
}

static int byte_count(state *s, int length, size_t *bytes) {
    if (length < 0 || (size_t)length > sizeof(s->mem_buf) / s->unit_size) {
        fprintf(stderr, "ERROR: invalid length (maximum 10000 bytes)\n");
        return 0;
    }
    *bytes = (size_t)length * s->unit_size;
    return 1;
}

/* Nonzero addresses are real process addresses, as required by the lab. */
static const unsigned char *source(state *s, uintptr_t address, size_t bytes) {
    uintptr_t base = (uintptr_t)s->mem_buf;
    if (!address) address = base;
    if (address > UINTPTR_MAX - bytes) return NULL;
    if (address >= base && address <= base + sizeof(s->mem_buf)) {
        size_t offset = address - base;
        if (offset > s->mem_count || bytes > s->mem_count - offset) {
            fprintf(stderr, "ERROR: range exceeds initialized buffer\n");
            return NULL;
        }
    }
    return (const unsigned char *)address;
}

static void toggle_debug_mode(state *s) {
    s->debug_mode = !s->debug_mode;
    printf("Debug flag now %s\n", s->debug_mode ? "on" : "off");
}
static void set_file_name(state *s) {
    char line[128];
    puts("Enter file name:");
    input(line, sizeof(line));
    line[strcspn(line, "\n")] = '\0';
    strcpy(s->file_name, line);
    if (s->debug_mode) fprintf(stderr, "Debug: file name set to '%s'\n", s->file_name);
}
static void set_unit_size(state *s) {
    char line[256], extra; int size;
    puts("Enter unit size (1, 2, or 4):"); input(line, sizeof(line));
    if (sscanf(line, "%d %c", &size, &extra) != 1 || (size != 1 && size != 2 && size != 4)) {
        puts("ERROR: invalid unit size"); return;
    }
    s->unit_size = size;
    if (s->debug_mode) fprintf(stderr, "Debug: set size to %d\n", size);
}
static void load_into_memory(state *s) {
    char line[256], extra; unsigned long location; int length; size_t bytes;
    if (!s->file_name[0]) { puts("ERROR: file name is empty"); return; }
    FILE *file = fopen(s->file_name, "rb");
    if (!file) { perror(s->file_name); return; }
    puts("Please enter <location> <length>"); input(line, sizeof(line));
    if (sscanf(line, "%lx %d %c", &location, &length, &extra) != 2 || location > LONG_MAX || !byte_count(s, length, &bytes)) {
        puts("ERROR: invalid input"); fclose(file); return;
    }
    if (s->debug_mode) fprintf(stderr, "Debug: file=%s location=%lx length=%d\n", s->file_name, location, length);
    if (fseek(file, (long)location, SEEK_SET)) { perror("seek"); fclose(file); return; }
    s->mem_count = fread(s->mem_buf, 1, bytes, file);
    if (ferror(file)) perror("read");
    fclose(file);
    printf("Loaded %zu units into memory\n", s->mem_count / s->unit_size);
    if (s->mem_count != bytes) printf("Short read: loaded %zu bytes\n", s->mem_count);
}
static void toggle_display_mode(state *s) {
    s->display_flag = !s->display_flag;
    printf("Display flag now %s, %s representation\n", s->display_flag ? "on" : "off", s->display_flag ? "hexadecimal" : "decimal");
}
static void memory_display(state *s) {
    char line[256], extra; uintptr_t address; int length; size_t bytes;
    puts("Enter address and length"); input(line, sizeof(line));
    if (sscanf(line, "%" SCNxPTR " %d %c", &address, &length, &extra) != 2 || !byte_count(s, length, &bytes)) { puts("ERROR: invalid input"); return; }
    const unsigned char *data = source(s, address, bytes);
    if (!data) return;
    puts(s->display_flag ? "Hexadecimal\n===========" : "Decimal\n=======");
    for (int i = 0; i < length; ++i) {
        uint32_t value = 0;
        memcpy(&value, data + (size_t)i * s->unit_size, s->unit_size);
        if (s->display_flag) printf("%" PRIx32 "\n", value);
        else {
            int32_t signed_value = s->unit_size == 1 ? (int8_t)value : s->unit_size == 2 ? (int16_t)value : (int32_t)value;
            printf("%" PRId32 "\n", signed_value);
        }
    }
}
static void save_into_file(state *s) {
    char line[256], extra; uintptr_t address; unsigned long target; int length; size_t bytes;
    if (!s->file_name[0]) { puts("ERROR: file name is empty"); return; }
    puts("Please enter <source-address> <target-location> <length>"); input(line, sizeof(line));
    if (sscanf(line, "%" SCNxPTR " %lx %d %c", &address, &target, &length, &extra) != 3 || target > LONG_MAX || !byte_count(s, length, &bytes)) { puts("ERROR: invalid input"); return; }
    const unsigned char *data = source(s, address, bytes);
    if (!data) return;
    FILE *file = fopen(s->file_name, "r+b");
    if (!file) { perror(s->file_name); return; }
    long end;
    if (fseek(file, 0, SEEK_END) || (end = ftell(file)) < 0) { perror("file size"); fclose(file); return; }
    if (target > (unsigned long)end) { puts("ERROR: target location exceeds file size"); fclose(file); return; }
    if (s->debug_mode) fprintf(stderr, "Debug: file=%s source=%" PRIxPTR " target=%lx length=%d\n", s->file_name, address, target, length);
    if (fseek(file, (long)target, SEEK_SET)) { perror("seek"); fclose(file); return; }
    size_t written = fwrite(data, 1, bytes, file);
    int failed = written != bytes;
    if (failed) perror("write");
    if (fclose(file)) { perror("close"); failed = 1; }
    if (!failed) printf("Saved %zu units into file\n", written / s->unit_size);
}
static void memory_modify(state *s) {
    char line[256], extra; unsigned int location; uint32_t value;
    puts("Please enter <location> <val>"); input(line, sizeof(line));
    if (sscanf(line, "%x %" SCNx32 " %c", &location, &value, &extra) != 2 || location > sizeof(s->mem_buf) - s->unit_size) { puts("ERROR: invalid buffer location or value"); return; }
    if (s->debug_mode) fprintf(stderr, "Debug: location=%x value=%" PRIx32 "\n", location, value);
    size_t end = location + s->unit_size;
    if (end > s->mem_count) {
        memset(s->mem_buf + s->mem_count, 0, end - s->mem_count);
        s->mem_count = end;
    }
    memcpy(s->mem_buf + location, &value, s->unit_size);
    printf("Modified memory at location %x with value %" PRIx32 "\n", location, value);
}
static void quit(state *s) {
    if (s->debug_mode) fprintf(stderr, "quitting\n");
    exit(0);
}
int main(void) {
    state s = { .unit_size = 1 };
    menu actions[] = {
        {"Toggle Debug Mode", toggle_debug_mode}, {"Set File Name", set_file_name},
        {"Set Unit Size", set_unit_size}, {"Load Into Memory", load_into_memory},
        {"Toggle Display Mode", toggle_display_mode}, {"Memory Display", memory_display},
        {"Save Into File", save_into_file}, {"Memory Modify", memory_modify},
        {"Quit", quit}, {NULL, NULL}
    };
    for (;;) {
        if (s.debug_mode) fprintf(stderr, "Debug: unit_size=%d file_name='%s' mem_count=%zu mem_buf=%p\n", s.unit_size, s.file_name, s.mem_count, (void *)s.mem_buf);
        puts("Choose action:");
        int count = 0;
        for (; actions[count].name; ++count) printf("%d-%s\n", count, actions[count].name);
        printf("> "); fflush(stdout);
        char line[256], extra; int choice;
        input(line, sizeof(line));
        if (sscanf(line, "%d %c", &choice, &extra) == 1 && choice >= 0 && choice < count) actions[choice].func(&s);
        else puts("Invalid choice");
    }
}
