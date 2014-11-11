

static bool checkmem(uchar* p, uint sz, uchar chk);


static bool checkmem(uchar* p, uint sz, uchar chk)
{
        uint i;
        uint end;
        uint start;

        if (sz <= 10) {
            end = sz;
            start = 10;
        } else {
            end = 10;
            start = sz - 10;
        }

        for (i = 0; i < end; i++) {
            if (p[i] != chk) {
                printf("%d (pos = %d, chk = %d, val = %d)\n", sz, i, chk, p[i]);
                return(FALSE);
            }
        }
        for (i = start; i < sz; i++) {
            if (p[i] != chk) {
                printf("%d (pos = %d, chk = %d, val = %d)\n", sz, i, chk, p[i]);
                return(FALSE);
            }
        }
        return(TRUE);
}

static void test_basic(void)
{
        uint i;
        char* p;

        printf("test_basic\n");

        for (i = 0; i < NBASIC; i++) {
            if (i % 100 == 0) {
                printf("%d\r", i);
            }
            p = SsQmemAlloc(i);
            ss_assert((ulong)p % SS_ALIGNMENT == 0);
            memset(p, FILLCH(i), i);
            SsQmemFree(p);
        }
        printf("%d\n", i);
        for (i = 0; i < NBASIC; i++) {
            if (i % 100 == 0) {
                printf("%d\r", i);
            }
            p = SsQmemAlloc(i);
            ss_assert((ulong)p % SS_ALIGNMENT == 0);
            memset(p, FILLCH(i+1), i);
            p = SsQmemRealloc(p, i + 10);
            ss_assert((ulong)p % SS_ALIGNMENT == 0);
            ss_assert(checkmem(p, i, FILLCH(i+1)));
            SsQmemFree(p);
        }
        printf("%d\n", i);
        for (i = 51; i < NBASIC; i++) {
            if (i % 100 == 0) {
                printf("%d\r", i);
            }
            p = SsQmemAlloc(i);
            ss_assert((ulong)p % SS_ALIGNMENT == 0);
            memset(p, FILLCH(i+2), i);
            p = SsQmemRealloc(p, i - 51);
            ss_assert((ulong)p % SS_ALIGNMENT == 0);
            ss_assert(checkmem(p, i - 51, FILLCH(i+2)));
            SsQmemFree(p);
        }
        printf("%d\n", i);
}

static char* get_time_in_string(double d)
{
        static char str[20];

        SsDoubleToAscii(d, str, 15);

        return(str);
}

