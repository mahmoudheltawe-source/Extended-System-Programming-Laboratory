#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct multi {
    uint16_t size;
    unsigned char num[];
};

extern struct multi *add_multi(struct multi *, struct multi *);
extern struct multi *getmulti(void);
extern struct multi *PRmulti(void);
extern unsigned int rand_num(void);
extern uint16_t STATE;

/* Guard each assembly allocation and account for every free, including errors. */
void *__real_malloc(size_t);
void __real_free(void *);
static struct { unsigned char *raw; size_t size; } blocks[32];
static int live_blocks;
static int fail_allocation;

void *__wrap_malloc(size_t size)
{
    if (fail_allocation)
        return NULL;
    for (unsigned i = 0; i < 32; ++i) {
        if (blocks[i].raw)
            continue;
        unsigned char *raw = __real_malloc(size + 32);
        assert(raw);
        memset(raw, 0xa5, size + 32);
        blocks[i].raw = raw;
        blocks[i].size = size;
        ++live_blocks;
        return raw + 16;
    }
    abort();
}

void __wrap_free(void *pointer)
{
    if (!pointer)
        return;
    for (unsigned i = 0; i < 32; ++i) {
        unsigned char *raw = blocks[i].raw;
        if (!raw || raw + 16 != pointer)
            continue;
        for (unsigned j = 0; j < 16; ++j) {
            assert(raw[j] == 0xa5);
            assert(raw[16 + blocks[i].size + j] == 0xa5);
        }
        __real_free(raw);
        blocks[i].raw = NULL;
        --live_blocks;
        return;
    }
    assert(!"free called on an unowned pointer");
}

static struct multi *number(unsigned size, unsigned salt)
{
    struct multi *p = malloc(sizeof(*p) + size);
    assert(p);
    p->size = size;
    for (unsigned i = 0; i < size; ++i)
        p->num[i] = salt ? (i * 73 + salt) & 255 : 255;
    return p;
}

static void check_maxmin(struct multi *p, struct multi *q)
{
    struct multi *longer = p, *shorter = q;
    __asm__ volatile("call MaxMin"
                     : "+a"(longer), "+b"(shorter)
                     : : "ecx", "cc", "memory");
    assert(longer == (p->size >= q->size ? p : q));
    assert(shorter == (p->size >= q->size ? q : p));
}

static void check_addition(unsigned a, unsigned b, unsigned salt)
{
    struct multi *p = number(a, salt), *q = number(b, salt ? salt + 3 : 0);
    unsigned char before_p[602], before_q[602];
    memcpy(before_p, p, sizeof(*p) + a);
    memcpy(before_q, q, sizeof(*q) + b);
    check_maxmin(p, q);
    struct multi *r = add_multi(p, q);
    assert(r && r != p && r != q);
    unsigned max = a > b ? a : b;
    assert(r->size == max + 1);
    unsigned carry = 0;
    for (unsigned i = 0; i < max; ++i) {
        unsigned sum = carry;
        if (i < a) sum += p->num[i];
        if (i < b) sum += q->num[i];
        assert(r->num[i] == (sum & 255));
        carry = sum >> 8;
    }
    assert(r->num[max] == carry);
    assert(memcmp(before_p, p, sizeof(*p) + a) == 0);
    assert(memcmp(before_q, q, sizeof(*q) + b) == 0);
    free(r);
    free(q);
    free(p);
    assert(live_blocks == 0);
}

static uint16_t reference_step(uint16_t state)
{
    unsigned feedback = ((state >> 0) ^ (state >> 2) ^ (state >> 3) ^ (state >> 5)) & 1;
    return (state >> 1) | (feedback << 15);
}

static unsigned reference_byte(uint16_t *state)
{
    unsigned result = 0;
    for (unsigned i = 0; i < 8; ++i) {
        *state = reference_step(*state);
        result = (result << 1) | (*state & 1);
    }
    return result;
}

static void check_prng(void)
{
    static unsigned char seen[65536];
    STATE = 0xace1;
    uint16_t expected = STATE;
    for (unsigned i = 0; i < 65535; ++i) {
        expected = reference_step(expected);
        unsigned value = rand_num();
        assert(value == expected && value == STATE);
        assert(value && !seen[value]);
        seen[value] = 1;
    }
    assert(STATE == 0xace1);

    unsigned zero_lengths = 0;
    for (unsigned seed = 1; seed <= 256; ++seed) {
        STATE = seed;
        expected = seed;
        unsigned length;
        while (!(length = reference_byte(&expected)))
            ++zero_lengths;
        unsigned char bytes[255];
        for (unsigned i = 0; i < length; ++i)
            bytes[i] = reference_byte(&expected);
        struct multi *p = PRmulti();
        assert(p && p->size == length);
        assert(memcmp(p->num, bytes, length) == 0);
        assert(STATE == expected);
        free(p);
    }
    assert(zero_lengths > 0); /* Explicitly cover rejection of a zero length. */
    assert(live_blocks == 0);
}

static void check_input(void)
{
    FILE *saved = stdin;
    const char input[] = "AbC\r\n1\n";
    stdin = fmemopen((void *)input, sizeof(input) - 1, "r");
    assert(stdin);
    struct multi *p = getmulti(), *q = getmulti();
    assert(p && p->size == 2 && p->num[0] == 0xbc && p->num[1] == 0x0a);
    assert(q && q->size == 1 && q->num[0] == 1);
    assert(getmulti() == NULL);
    free(p);
    free(q);
    fclose(stdin);

    const char *bad[] = {"\n", "1g\n", "!1\n", "1 \n", "-1\n", "0x1\n"};
    for (unsigned i = 0; i < sizeof(bad) / sizeof(*bad); ++i) {
        stdin = fmemopen((void *)bad[i], strlen(bad[i]), "r");
        assert(stdin);
        assert(getmulti() == NULL);
        assert(live_blocks == 0);
        fclose(stdin);
    }
    stdin = saved;
}

static void check_failures_and_capacity(void)
{
    struct multi *p = number(255, 0), *q = number(1, 1);
    FILE *saved = stdin;
    char input[] = "abc\n";
    stdin = fmemopen(input, strlen(input), "r");
    assert(stdin);
    fail_allocation = 1;
    assert(add_multi(p, q) == NULL);
    assert(PRmulti() == NULL);
    assert(getmulti() == NULL);
    assert(live_blocks == 2);
    fail_allocation = 0;
    fclose(stdin);
    stdin = saved;
    free(p);
    free(q);

    p = number(65534, 0);
    q = number(1, 1);
    struct multi *r = add_multi(p, q);
    assert(r && r->size == 65535);
    for (unsigned i = 0; i < 65534; ++i)
        assert(r->num[i] == 0);
    assert(r->num[65534] == 1);
    assert(add_multi(r, q) == NULL); /* Reject a length that would wrap to zero. */
    free(r);
    free(q);
    free(p);
    assert(live_blocks == 0);
}

int main(void)
{
    const unsigned sizes[] = {1, 2, 5, 6, 31, 254, 255, 256, 299, 300};
    for (unsigned i = 0; i < sizeof(sizes) / sizeof(*sizes); ++i)
        for (unsigned j = 0; j < sizeof(sizes) / sizeof(*sizes); ++j) {
            check_addition(sizes[i], sizes[j], 0);
            check_addition(sizes[i], sizes[j], 19);
        }
    check_prng();
    check_input();
    check_failures_and_capacity();
    puts("Function checks passed: addition, MaxMin, full LFSR period, PRmulti, input, allocation failures and guards.");
    return 0;
}
