#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "hhms.h"

#include <errno.h>
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <wchar.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#define MAX_FILE ((size_t)8 * 1024 * 1024)
#define TM_TERRAIN 1u
#define TM_N 2u
#define SM_X_INT 1u
#define SM_Y_INT 2u
#define SM_ORIENT 4u

static HhmsIoResult fail(HhmsError *e, HhmsIoResult code, int line, int col,
                         int os_error, const char *detail)
{
    if (e) {
        e->code = code;
        e->line = line;
        e->column = col;
        e->os_error = os_error;
        snprintf(e->detail, sizeof(e->detail), "%s", detail ? detail : "");
    }
    return code;
}

void hhms_error_clear(HhmsError *e)
{
    if (e) memset(e, 0, sizeof(*e));
}

void hhms_view_init(HhmsView *v)
{
    if (!v) return;
    memset(v, 0, sizeof(*v));
    v->cell = 32.0f;
    v->ui_scale = 1.0f;
}

const char *hhms_io_result_name(HhmsIoResult r)
{
    switch (r) {
    case HHMS_IO_OK: return "ok";
    case HHMS_IO_OPEN_FAILED: return "open failed";
    case HHMS_IO_READ_FAILED: return "read failed";
    case HHMS_IO_TOO_LARGE: return "file too large";
    case HHMS_IO_OUT_OF_MEMORY: return "out of memory";
    case HHMS_IO_PARSE_FAILED: return "parse failed";
    case HHMS_IO_UNSUPPORTED_VERSION: return "unsupported version";
    case HHMS_IO_SCHEMA_FAILED: return "schema failed";
    case HHMS_IO_RANGE_FAILED: return "value out of range";
    case HHMS_IO_WRITE_FAILED: return "write failed";
    case HHMS_IO_FLUSH_FAILED: return "flush failed";
    case HHMS_IO_REPLACE_FAILED: return "replace failed";
    }
    return "unknown I/O result";
}

#ifdef _WIN32
static int to_wide(const char *s, wchar_t **out, int *oserr)
{
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, NULL, 0);
    wchar_t *w;
    *out = NULL;
    if (n <= 0) { if (oserr) *oserr = (int)GetLastError(); return 0; }
    if ((size_t)n > SIZE_MAX / sizeof(*w)) return -1;
    w = (wchar_t *)malloc((size_t)n * sizeof(*w));
    if (!w) return -1;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, w, n) != n) {
        if (oserr) *oserr = (int)GetLastError();
        free(w);
        return 0;
    }
    *out = w;
    return 1;
}

static FILE *open_utf8(const char *path, const wchar_t *mode, int *oserr,
                       int *oom)
{
    wchar_t *w = NULL;
    FILE *f;
    int r = to_wide(path, &w, oserr);
    *oom = 0;
    if (r < 0) { *oom = 1; return NULL; }
    if (!r) return NULL;
    errno = 0;
    f = _wfopen(w, mode);
    if (!f && oserr) *oserr = errno;
    free(w);
    return f;
}
#else
static FILE *open_utf8(const char *path, const char *mode, int *oserr,
                       int *oom)
{
    FILE *f;
    *oom = 0;
    errno = 0;
    f = fopen(path, mode);
    if (!f && oserr) *oserr = errno;
    return f;
}
#endif

static HhmsIoResult read_file(const char *path, char **data, size_t *size,
                              HhmsError *e)
{
    FILE *f;
    char *buf;
    size_t used = 0, cap = 4096;
    int oserr = 0, oom = 0;
#ifdef _WIN32
    f = open_utf8(path, L"rb", &oserr, &oom);
#else
    f = open_utf8(path, "rb", &oserr, &oom);
#endif
    if (!f)
        return fail(e, oom ? HHMS_IO_OUT_OF_MEMORY : HHMS_IO_OPEN_FAILED,
                    0, 0, oserr, oom ? "could not allocate the file path" :
                    "could not open the map file");
    buf = (char *)malloc(cap);
    if (!buf) {
        fclose(f);
        return fail(e, HHMS_IO_OUT_OF_MEMORY, 0, 0, 0,
                    "could not allocate the read buffer");
    }
    for (;;) {
        size_t got, available;
        if (used == cap) {
            size_t next;
            char *grown;
            if (cap == MAX_FILE + 1) break;
            next = cap * 2;
            if (next > MAX_FILE + 1) next = MAX_FILE + 1;
            grown = (char *)realloc(buf, next);
            if (!grown) {
                free(buf); fclose(f);
                return fail(e, HHMS_IO_OUT_OF_MEMORY, 0, 0, 0,
                            "could not grow the read buffer");
            }
            buf = grown; cap = next;
        }
        available = cap - used;
        got = fread(buf + used, 1, available, f);
        used += got;
        if (used > MAX_FILE) break;
        if (got < available) {
            if (ferror(f)) {
                oserr = errno; free(buf); fclose(f);
                return fail(e, HHMS_IO_READ_FAILED, 0, 0, oserr,
                            "could not read the complete map file");
            }
            if (feof(f)) break;
            free(buf); fclose(f);
            return fail(e, HHMS_IO_READ_FAILED, 0, 0, 0,
                        "the map file stopped producing data");
        }
    }
    if (used > MAX_FILE) {
        free(buf); fclose(f);
        return fail(e, HHMS_IO_TOO_LARGE, 0, 0, 0,
                    "map files are limited to 8 MiB");
    }
    errno = 0;
    if (fclose(f) != 0) {
        oserr = errno; free(buf);
        return fail(e, HHMS_IO_READ_FAILED, 0, 0, oserr,
                    "could not close the map file after reading");
    }
    if (used == cap) {
        char *grown = (char *)realloc(buf, used + 1);
        if (!grown) {
            free(buf);
            return fail(e, HHMS_IO_OUT_OF_MEMORY, 0, 0, 0,
                        "could not terminate the read buffer");
        }
        buf = grown;
    }
    buf[used] = '\0';
    *data = buf; *size = used;
    return HHMS_IO_OK;
}

typedef struct {
    const char *p, *end;
    int line, col;
    HhmsError *error;
    HhmsIoResult result;
} Parser;

typedef struct {
    unsigned char tiles[HHMS_MAX_TILES];
    unsigned char supports[HHMS_MAX_SUPPORTS];
    int ntiles, nsupports, have_view;
} Meta;

static int p_fail(Parser *p, HhmsIoResult r, const char *detail)
{
    if (p->result == HHMS_IO_OK) {
        p->result = r;
        fail(p->error, r, p->line, p->col, 0, detail);
    }
    return 0;
}

static void advance(Parser *p)
{
    if (p->p >= p->end) return;
    if (*p->p++ == '\n') { p->line++; p->col = 1; }
    else p->col++;
}

static void space(Parser *p)
{
    while (p->p < p->end && (*p->p == ' ' || *p->p == '\t' ||
           *p->p == '\r' || *p->p == '\n')) advance(p);
}

static int peek(Parser *p, char c)
{
    space(p);
    return p->p < p->end && *p->p == c;
}

static int take(Parser *p, char c, const char *detail)
{
    space(p);
    if (p->p >= p->end || *p->p != c)
        return p_fail(p, HHMS_IO_PARSE_FAILED, detail);
    advance(p);
    return 1;
}

static int hex(unsigned char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int hex4(Parser *p, unsigned *out)
{
    int i; unsigned v = 0;
    for (i = 0; i < 4; i++) {
        int d;
        if (p->p >= p->end || (d = hex((unsigned char)*p->p)) < 0)
            return p_fail(p, HHMS_IO_PARSE_FAILED, "invalid Unicode escape");
        v = v * 16u + (unsigned)d; advance(p);
    }
    *out = v; return 1;
}

static void put_byte(char *out, size_t cap, size_t *n, unsigned char c,
                     int *long_string)
{
    if (*n + 1 < cap) out[(*n)++] = (char)c;
    else *long_string = 1;
}

static void put_cp(char *out, size_t cap, size_t *n, unsigned v, int *too_long)
{
    if (v <= 0x7f) put_byte(out, cap, n, (unsigned char)v, too_long);
    else if (v <= 0x7ff) {
        put_byte(out, cap, n, (unsigned char)(0xc0 | (v >> 6)), too_long);
        put_byte(out, cap, n, (unsigned char)(0x80 | (v & 63)), too_long);
    } else if (v <= 0xffff) {
        put_byte(out, cap, n, (unsigned char)(0xe0 | (v >> 12)), too_long);
        put_byte(out, cap, n, (unsigned char)(0x80 | ((v >> 6) & 63)), too_long);
        put_byte(out, cap, n, (unsigned char)(0x80 | (v & 63)), too_long);
    } else {
        put_byte(out, cap, n, (unsigned char)(0xf0 | (v >> 18)), too_long);
        put_byte(out, cap, n, (unsigned char)(0x80 | ((v >> 12) & 63)), too_long);
        put_byte(out, cap, n, (unsigned char)(0x80 | ((v >> 6) & 63)), too_long);
        put_byte(out, cap, n, (unsigned char)(0x80 | (v & 63)), too_long);
    }
}

static int raw_utf8(Parser *p, char *out, size_t cap, size_t *n, int *too_long)
{
    const unsigned char *s = (const unsigned char *)p->p;
    size_t left = (size_t)(p->end - p->p);
    unsigned v; int count, i;
    if (s[0] >= 0xc2 && s[0] <= 0xdf) { count = 2; v = s[0] & 31; }
    else if (s[0] >= 0xe0 && s[0] <= 0xef) { count = 3; v = s[0] & 15; }
    else if (s[0] >= 0xf0 && s[0] <= 0xf4) { count = 4; v = s[0] & 7; }
    else return p_fail(p, HHMS_IO_PARSE_FAILED, "invalid UTF-8 in string");
    if (left < (size_t)count)
        return p_fail(p, HHMS_IO_PARSE_FAILED, "incomplete UTF-8 in string");
    for (i = 1; i < count; i++) {
        if ((s[i] & 0xc0) != 0x80)
            return p_fail(p, HHMS_IO_PARSE_FAILED,
                          "invalid UTF-8 continuation byte");
        v = (v << 6) | (s[i] & 63);
    }
    if ((count == 3 && v < 0x800) || (count == 4 && v < 0x10000) ||
        (v >= 0xd800 && v <= 0xdfff) || v > 0x10ffff)
        return p_fail(p, HHMS_IO_PARSE_FAILED, "invalid UTF-8 code point");
    for (i = 0; i < count; i++) { put_byte(out, cap, n, s[i], too_long); advance(p); }
    return 1;
}

static int string(Parser *p, char *out, size_t cap)
{
    size_t n = 0; int too_long = 0;
    space(p);
    if (p->p >= p->end || *p->p != '"')
        return p_fail(p, HHMS_IO_PARSE_FAILED, "expected a JSON string");
    advance(p);
    while (p->p < p->end) {
        unsigned char c = (unsigned char)*p->p;
        if (c == '"') {
            advance(p); out[n] = 0;
            return too_long ? p_fail(p, HHMS_IO_SCHEMA_FAILED,
                                     "JSON string is too long") : 1;
        }
        if (c < 0x20)
            return p_fail(p, HHMS_IO_PARSE_FAILED,
                          "unescaped control character in string");
        if (c >= 0x80) {
            if (!raw_utf8(p, out, cap, &n, &too_long)) return 0;
            continue;
        }
        if (c != '\\') { put_byte(out, cap, &n, c, &too_long); advance(p); continue; }
        advance(p);
        if (p->p >= p->end)
            return p_fail(p, HHMS_IO_PARSE_FAILED, "incomplete string escape");
        c = (unsigned char)*p->p; advance(p);
        switch (c) {
        case '"': case '\\': case '/': put_byte(out, cap, &n, c, &too_long); break;
        case 'b': put_byte(out, cap, &n, '\b', &too_long); break;
        case 'f': put_byte(out, cap, &n, '\f', &too_long); break;
        case 'n': put_byte(out, cap, &n, '\n', &too_long); break;
        case 'r': put_byte(out, cap, &n, '\r', &too_long); break;
        case 't': put_byte(out, cap, &n, '\t', &too_long); break;
        case 'u': {
            unsigned v;
            if (!hex4(p, &v)) return 0;
            if (v >= 0xd800 && v <= 0xdbff) {
                unsigned low;
                if (p->end - p->p < 2 || p->p[0] != '\\' || p->p[1] != 'u')
                    return p_fail(p, HHMS_IO_PARSE_FAILED,
                                  "high surrogate lacks a low surrogate");
                advance(p); advance(p);
                if (!hex4(p, &low)) return 0;
                if (low < 0xdc00 || low > 0xdfff)
                    return p_fail(p, HHMS_IO_PARSE_FAILED, "invalid low surrogate");
                v = 0x10000 + ((v - 0xd800) << 10) + low - 0xdc00;
            } else if (v >= 0xdc00 && v <= 0xdfff) {
                return p_fail(p, HHMS_IO_PARSE_FAILED, "unexpected low surrogate");
            }
            if(v==0)return p_fail(p,HHMS_IO_SCHEMA_FAILED,
                                  "NUL is not permitted in schema strings");
            put_cp(out, cap, &n, v, &too_long); break;
        }
        default: return p_fail(p, HHMS_IO_PARSE_FAILED, "invalid string escape");
        }
    }
    return p_fail(p, HHMS_IO_PARSE_FAILED, "unterminated JSON string");
}

typedef struct { const char *start, *end; int integer; } Num;

static int number_token(Parser *p, Num *n)
{
    const char *start;
    int integer = 1;
    space(p); start = p->p;
    if (p->p < p->end && *p->p == '-') advance(p);
    if (p->p >= p->end)
        return p_fail(p, HHMS_IO_PARSE_FAILED, "expected a JSON number");
    if (*p->p == '0') {
        advance(p);
        if (p->p < p->end && *p->p >= '0' && *p->p <= '9')
            return p_fail(p, HHMS_IO_PARSE_FAILED, "leading zero in JSON number");
    } else if (*p->p >= '1' && *p->p <= '9') {
        do advance(p); while (p->p < p->end && *p->p >= '0' && *p->p <= '9');
    } else return p_fail(p, HHMS_IO_PARSE_FAILED, "expected a JSON number");
    if (p->p < p->end && *p->p == '.') {
        integer = 0; advance(p);
        if (p->p >= p->end || *p->p < '0' || *p->p > '9')
            return p_fail(p, HHMS_IO_PARSE_FAILED, "fraction has no digits");
        do advance(p); while (p->p < p->end && *p->p >= '0' && *p->p <= '9');
    }
    if (p->p < p->end && (*p->p == 'e' || *p->p == 'E')) {
        integer = 0; advance(p);
        if (p->p < p->end && (*p->p == '+' || *p->p == '-')) advance(p);
        if (p->p >= p->end || *p->p < '0' || *p->p > '9')
            return p_fail(p, HHMS_IO_PARSE_FAILED, "exponent has no digits");
        do advance(p); while (p->p < p->end && *p->p >= '0' && *p->p <= '9');
    }
    n->start = start; n->end = p->p; n->integer = integer;
    return 1;
}

static int integer(Parser *p, int *out)
{
    Num n; const char *s; unsigned long v = 0, limit; int neg;
    if (!number_token(p, &n)) return 0;
    if (!n.integer) return p_fail(p, HHMS_IO_SCHEMA_FAILED, "expected an integer");
    s = n.start; neg = *s == '-'; if (neg) s++;
    limit = neg ? (unsigned long)INT_MAX + 1ul : (unsigned long)INT_MAX;
    while (s < n.end) {
        unsigned d = (unsigned)(*s++ - '0');
        if (v > (limit - d) / 10ul)
            return p_fail(p, HHMS_IO_RANGE_FAILED, "integer is outside the C int range");
        v = v * 10ul + d;
    }
    if (neg && v == (unsigned long)INT_MAX + 1ul) *out = INT_MIN;
    else *out = neg ? -(int)v : (int)v;
    return 1;
}

static int real_number(Parser *p, double *out, int *is_integer)
{
    Num n; const char *s, *mant_end, *dot;
    int neg, decimal, index = 0, first = -1, collected = 0;
    int exponent = 0, exp_neg = 0;
    long double mant = 0.0L, result;
    double d;
    if (!number_token(p, &n)) return 0;
    if (is_integer) *is_integer = n.integer;
    s = n.start; neg = *s == '-'; if (neg) s++;
    mant_end = s; while (mant_end < n.end && *mant_end != 'e' && *mant_end != 'E') mant_end++;
    dot = s; while (dot < mant_end && *dot != '.') dot++;
    decimal = (int)(dot - s);
    while (s < mant_end) {
        int digit;
        if (*s == '.') { s++; continue; }
        digit = *s++ - '0';
        if (first < 0 && digit) first = index;
        if (first >= 0 && collected < 19) { mant = mant * 10.0L + digit; collected++; }
        index++;
    }
    if (mant_end < n.end) {
        s = mant_end + 1;
        if (s < n.end && (*s == '+' || *s == '-')) { exp_neg = *s == '-'; s++; }
        while (s < n.end) { int digit = *s++ - '0'; if (exponent < 100000) exponent = exponent * 10 + digit; }
        if (exp_neg) exponent = -exponent;
    }
    if (first < 0) { *out = neg ? -0.0 : 0.0; return 1; }
    exponent += decimal - first - collected;
    if (exponent < -5000 || exponent > 5000)
        return p_fail(p, HHMS_IO_RANGE_FAILED, "floating-point number is out of range");
    result = mant * powl(10.0L, (long double)exponent);
    if (neg) result = -result;
    d = (double)result;
    if (!isfinite(d) || d == 0.0)
        return p_fail(p, HHMS_IO_RANGE_FAILED, "floating-point number is out of range");
    *out = d; return 1;
}

static int object_next(Parser *p, int *done)
{
    space(p);
    if (p->p < p->end && *p->p == '}') { advance(p); *done = 1; return 1; }
    if (p->p < p->end && *p->p == ',') {
        advance(p); space(p);
        if (p->p < p->end && *p->p == '}')
            return p_fail(p, HHMS_IO_PARSE_FAILED, "trailing comma in object");
        *done = 0; return 1;
    }
    return p_fail(p, HHMS_IO_PARSE_FAILED, "expected ',' or '}' in object");
}

static int array_next(Parser *p, int *done)
{
    space(p);
    if (p->p < p->end && *p->p == ']') { advance(p); *done = 1; return 1; }
    if (p->p < p->end && *p->p == ',') {
        advance(p); space(p);
        if (p->p < p->end && *p->p == ']')
            return p_fail(p, HHMS_IO_PARSE_FAILED, "trailing comma in array");
        *done = 0; return 1;
    }
    return p_fail(p, HHMS_IO_PARSE_FAILED, "expected ',' or ']' in array");
}

static int tile_kind(const char *s, HhmsKind *k)
{
    if (!strcmp(s, "unknown")) *k = HHMS_UNKNOWN;
    else if (!strcmp(s, "clear")) *k = HHMS_CLEAR;
    else if (!strcmp(s, "mine")) *k = HHMS_MINE;
    else if (!strcmp(s, "open")) *k = HHMS_OPEN;
    else return 0;
    return 1;
}

static const char *tile_name(HhmsKind k)
{
    switch (k) {
    case HHMS_UNKNOWN: return "unknown";
    case HHMS_CLEAR: return "clear";
    case HHMS_MINE: return "mine";
    case HHMS_OPEN: return "open";
    }
    return NULL;
}

static int terrain_kind(const char *s, HhmsTerrain *t)
{
    if (!strcmp(s, "rock")) *t = HHMS_TERRAIN_ROCK;
    else if (!strcmp(s, "water")) *t = HHMS_TERRAIN_WATER;
    else return 0;
    return 1;
}

static const char *terrain_name(HhmsTerrain t)
{
    switch (t) {
    case HHMS_TERRAIN_ROCK: return "rock";
    case HHMS_TERRAIN_WATER: return "water";
    }
    return NULL;
}

static int orient_kind(const char *s, HhmsOrientation *o)
{
    if (!strcmp(s, "north")) *o = HHMS_ORIENT_NORTH;
    else if (!strcmp(s, "east")) *o = HHMS_ORIENT_EAST;
    else if (!strcmp(s, "south")) *o = HHMS_ORIENT_SOUTH;
    else if (!strcmp(s, "west")) *o = HHMS_ORIENT_WEST;
    else return 0;
    return 1;
}

static const char *orient_name(HhmsOrientation o)
{
    switch (o) {
    case HHMS_ORIENT_NORTH: return "north";
    case HHMS_ORIENT_EAST: return "east";
    case HHMS_ORIENT_SOUTH: return "south";
    case HHMS_ORIENT_WEST: return "west";
    }
    return NULL;
}

static int int_coord(Parser *p, int v)
{
    return (v >= -HHMS_COORD_LIMIT && v <= HHMS_COORD_LIMIT) ? 1 :
           p_fail(p, HHMS_IO_RANGE_FAILED, "tile coordinate exceeds HHMS_COORD_LIMIT");
}

static int real_coord(Parser *p, double v)
{
    return (isfinite(v) && v >= -(double)HHMS_COORD_LIMIT &&
            v <= (double)HHMS_COORD_LIMIT) ? 1 :
           p_fail(p, HHMS_IO_RANGE_FAILED, "coordinate exceeds HHMS_COORD_LIMIT");
}

static int parse_tile(Parser *p, HhmsMap *m, unsigned char *meta)
{
    enum { X=1, Y=2, KIND=4, N=8, TERRAIN=16 };
    unsigned fields = 0, field; int x=0,y=0,n=0,done=0;
    HhmsKind kind=HHMS_UNKNOWN; HhmsTerrain terrain=HHMS_TERRAIN_ROCK;
    if (!take(p, '{', "expected a tile object")) return 0;
    if (peek(p, '}')) { advance(p); done=1; }
    while (!done) {
        char key[32];
        if (!string(p,key,sizeof(key)) || !take(p,':',"expected ':' after tile field")) return 0;
        if (!strcmp(key,"x")) field=X; else if (!strcmp(key,"y")) field=Y;
        else if (!strcmp(key,"kind")) field=KIND; else if (!strcmp(key,"n")) field=N;
        else if (!strcmp(key,"terrain")) field=TERRAIN;
        else return p_fail(p,HHMS_IO_SCHEMA_FAILED,"unknown tile field");
        if (fields & field) return p_fail(p,HHMS_IO_SCHEMA_FAILED,"duplicate tile field");
        fields |= field;
        if (field==X) { if (!integer(p,&x)||!int_coord(p,x)) return 0; }
        else if (field==Y) { if (!integer(p,&y)||!int_coord(p,y)) return 0; }
        else if (field==N) {
            if (!integer(p,&n)) return 0;
            if (n<0||n>8) return p_fail(p,HHMS_IO_RANGE_FAILED,"clear count must be between 0 and 8");
        } else {
            char value[48]; if (!string(p,value,sizeof(value))) return 0;
            if (field==KIND && !tile_kind(value,&kind)) return p_fail(p,HHMS_IO_SCHEMA_FAILED,"unknown tile kind");
            if (field==TERRAIN && !terrain_kind(value,&terrain)) return p_fail(p,HHMS_IO_SCHEMA_FAILED,"unknown terrain kind");
        }
        if (!object_next(p,&done)) return 0;
    }
    if ((fields&(X|Y|KIND))!=(X|Y|KIND)) return p_fail(p,HHMS_IO_SCHEMA_FAILED,"tile requires x, y, and kind");
    if (hhms_get(m,x,y)) return p_fail(p,HHMS_IO_SCHEMA_FAILED,"duplicate tile coordinates");
    {
        HhmsEditResult r=hhms_set_tile(m,x,y,kind,n);
        if (r!=HHMS_EDIT_OK && r!=HHMS_EDIT_NO_CHANGE)
            return p_fail(p,r==HHMS_EDIT_MAP_FULL?HHMS_IO_RANGE_FAILED:HHMS_IO_SCHEMA_FAILED,
                          r==HHMS_EDIT_MAP_FULL?"map has too many tiles":"tile is not valid for the map");
    }
    if (fields&TERRAIN) {
        HhmsEditResult r=hhms_set_terrain(m,x,y,terrain);
        if (r!=HHMS_EDIT_OK && r!=HHMS_EDIT_NO_CHANGE)
            return p_fail(p,HHMS_IO_SCHEMA_FAILED,"terrain is not valid for the tile");
    }
    *meta=(unsigned char)(((fields&TERRAIN)?TM_TERRAIN:0)|((fields&N)?TM_N:0));
    return 1;
}

static int parse_tiles(Parser *p,HhmsMap *m,Meta *meta)
{
    int done=0;
    if (!take(p,'[',"expected the tiles array")) return 0;
    if (peek(p,']')) { advance(p); return 1; }
    while (!done) {
        if (meta->ntiles>=HHMS_MAX_TILES) return p_fail(p,HHMS_IO_RANGE_FAILED,"map has too many tiles");
        if (!parse_tile(p,m,&meta->tiles[meta->ntiles])) return 0;
        meta->ntiles++;
        if (!array_next(p,&done)) return 0;
    }
    return 1;
}

static int parse_support(Parser *p,HhmsMap *m,unsigned char *meta)
{
    enum { X=1,Y=2,KIND=4,ORIENT=8 };
    unsigned fields=0,field; double x=0,y=0; int xi=0,yi=0,done=0;
    HhmsSupportKind kind=HHMS_SUP_WOOD; HhmsOrientation orient=HHMS_ORIENT_NORTH;
    if (!take(p,'{',"expected a support object")) return 0;
    if (peek(p,'}')) { advance(p); done=1; }
    while (!done) {
        char key[48];
        if (!string(p,key,sizeof(key))||!take(p,':',"expected ':' after support field")) return 0;
        if (!strcmp(key,"x")) field=X; else if (!strcmp(key,"y")) field=Y;
        else if (!strcmp(key,"kind")) field=KIND; else if (!strcmp(key,"orientation")) field=ORIENT;
        else return p_fail(p,HHMS_IO_SCHEMA_FAILED,"unknown support field");
        if (fields&field) return p_fail(p,HHMS_IO_SCHEMA_FAILED,"duplicate support field");
        fields|=field;
        if (field==X) { if (!real_number(p,&x,&xi)||!real_coord(p,x)) return 0; }
        else if (field==Y) { if (!real_number(p,&y,&yi)||!real_coord(p,y)) return 0; }
        else {
            char value[48]; if (!string(p,value,sizeof(value))) return 0;
            if (field==KIND && !hhms_support_kind_from_name(value,&kind)) return p_fail(p,HHMS_IO_SCHEMA_FAILED,"unknown support kind");
            if (field==ORIENT && !orient_kind(value,&orient)) return p_fail(p,HHMS_IO_SCHEMA_FAILED,"unknown support orientation");
        }
        if (!object_next(p,&done)) return 0;
    }
    if ((fields&(X|Y|KIND))!=(X|Y|KIND)) return p_fail(p,HHMS_IO_SCHEMA_FAILED,"support requires x, y, and kind");
    for (int i = 0; i < m->nsupports; i++) {
        if (m->supports[i].x == x && m->supports[i].y == y)
            return p_fail(p, HHMS_IO_SCHEMA_FAILED, "duplicate support coordinates");
    }
    {
        uint32_t id; HhmsEditResult r=hhms_add_support(m,x,y,kind,orient,&id);
        if (r!=HHMS_EDIT_OK) return p_fail(p,r==HHMS_EDIT_SUPPORTS_FULL?HHMS_IO_RANGE_FAILED:HHMS_IO_SCHEMA_FAILED,
            r==HHMS_EDIT_SUPPORTS_FULL?"map has too many supports":"support is not valid for the map");
    }
    *meta=(unsigned char)((xi?SM_X_INT:0)|(yi?SM_Y_INT:0)|((fields&ORIENT)?SM_ORIENT:0));
    return 1;
}

static int parse_supports(Parser *p,HhmsMap *m,Meta *meta)
{
    int done=0;
    if (!take(p,'[',"expected the supports array")) return 0;
    if (peek(p,']')) { advance(p); return 1; }
    while (!done) {
        if (meta->nsupports>=HHMS_MAX_SUPPORTS) return p_fail(p,HHMS_IO_RANGE_FAILED,"map has too many supports");
        if (!parse_support(p,m,&meta->supports[meta->nsupports])) return 0;
        meta->nsupports++;
        if (!array_next(p,&done)) return 0;
    }
    return 1;
}

static int parse_view(Parser *p,HhmsView *v)
{
    enum { CAMX=1,CAMY=2,CELL=4,SCALE=8 };
    unsigned fields=0,field; double camx=0,camy=0,cell=0,scale=0,value; int done=0;
    if (!take(p,'{',"expected a view object")) return 0;
    if (peek(p,'}')) { advance(p); done=1; }
    while (!done) {
        char key[32];
        if (!string(p,key,sizeof(key))||!take(p,':',"expected ':' after view field")) return 0;
        if (!strcmp(key,"camx")) field=CAMX; else if (!strcmp(key,"camy")) field=CAMY;
        else if (!strcmp(key,"cell")) field=CELL; else if (!strcmp(key,"ui_scale")) field=SCALE;
        else return p_fail(p,HHMS_IO_SCHEMA_FAILED,"unknown view field");
        if (fields&field) return p_fail(p,HHMS_IO_SCHEMA_FAILED,"duplicate view field");
        fields|=field;
        if (!real_number(p,&value,NULL)) return 0;
        if (field==CAMX) camx=value; else if (field==CAMY) camy=value;
        else if (field==CELL) cell=value; else scale=value;
        if (!object_next(p,&done)) return 0;
    }
    if (fields!=(CAMX|CAMY|CELL|SCALE)) return p_fail(p,HHMS_IO_SCHEMA_FAILED,"view requires camx, camy, cell, and ui_scale");
    if (!real_coord(p,camx)||!real_coord(p,camy)) return 0;
    if (cell < 14.0 || cell > 72.0 || scale < 0.75 || scale > 2.0 ||
        cell > FLT_MAX || scale > FLT_MAX)
        return p_fail(p, HHMS_IO_RANGE_FAILED,
                      "view cell must be 14-72 and ui_scale 0.75-2.0");
    v->camx=camx;v->camy=camy;v->cell=(float)cell;v->ui_scale=(float)scale;v->has_view=1;
    return 1;
}

static int validate_schema(Parser *p,int version,const HhmsMap *m,const Meta *meta)
{
    int i;
    if (m->ntiles!=meta->ntiles||m->nsupports!=meta->nsupports)
        return p_fail(p,HHMS_IO_SCHEMA_FAILED,"duplicate or unrepresentable map entries");
    for (i=0;i<meta->ntiles;i++) {
        const HhmsTile *t=&m->tiles[i]; int terrain=(meta->tiles[i]&TM_TERRAIN)!=0;
        int n=(meta->tiles[i]&TM_N)!=0;
        if (version==1) {
            if (terrain) return p_fail(p,HHMS_IO_SCHEMA_FAILED,"version 1 tiles cannot have terrain");
            if (t->kind==HHMS_UNKNOWN) return p_fail(p,HHMS_IO_SCHEMA_FAILED,"version 1 has no unknown tile kind");
        } else if (!terrain) return p_fail(p,HHMS_IO_SCHEMA_FAILED,"version 2 tiles require terrain");
        if ((t->kind==HHMS_CLEAR)!=n) return p_fail(p,HHMS_IO_SCHEMA_FAILED,"n is required only for clear tiles");
        if (t->terrain==HHMS_TERRAIN_WATER&&(t->kind==HHMS_CLEAR||t->kind==HHMS_OPEN))
            return p_fail(p,HHMS_IO_SCHEMA_FAILED,"water cannot be clear or open");
    }
    for (i=0;i<meta->nsupports;i++) {
        const HhmsSupport *s=&m->supports[i]; unsigned f=meta->supports[i];
        if (version==1) {
            if ((f&(SM_X_INT|SM_Y_INT))!=(SM_X_INT|SM_Y_INT)) return p_fail(p,HHMS_IO_SCHEMA_FAILED,"version 1 support positions are integers");
            if (f&SM_ORIENT) return p_fail(p,HHMS_IO_SCHEMA_FAILED,"version 1 supports cannot have orientation");
            if (s->kind<HHMS_SUP_WOOD||s->kind>HHMS_SUP_MONUMENT) return p_fail(p,HHMS_IO_SCHEMA_FAILED,"version 1 supports must be radial kinds");
        } else if (!(f&SM_ORIENT)) return p_fail(p,HHMS_IO_SCHEMA_FAILED,"version 2 supports require orientation");
    }
    if (version==1&&meta->have_view) return p_fail(p,HHMS_IO_SCHEMA_FAILED,"version 1 files cannot contain a view");
    return 1;
}

static int document(Parser *p,HhmsMap *m,HhmsView *v,Meta *meta)
{
    enum { VERSION=1,TILES=2,SUPPORTS=4,VIEW=8 };
    unsigned fields=0,field; int version=0,vline=0,vcol=0,done=0;
    if (!take(p,'{',"expected a root object")) return 0;
    if (peek(p,'}')) { advance(p); done=1; }
    while (!done) {
        char key[32];
        if (!string(p,key,sizeof(key))||!take(p,':',"expected ':' after root field")) return 0;
        if (!strcmp(key,"version")) field=VERSION; else if (!strcmp(key,"tiles")) field=TILES;
        else if (!strcmp(key,"supports")) field=SUPPORTS; else if (!strcmp(key,"view")) field=VIEW;
        else return p_fail(p,HHMS_IO_SCHEMA_FAILED,"unknown root field");
        if (fields&field) return p_fail(p,HHMS_IO_SCHEMA_FAILED,"duplicate root field");
        fields|=field;
        if (field==VERSION) { space(p);vline=p->line;vcol=p->col;if(!integer(p,&version))return 0; }
        else if (field==TILES) { if(!parse_tiles(p,m,meta))return 0; }
        else if (field==SUPPORTS) { if(!parse_supports(p,m,meta))return 0; }
        else { if(!parse_view(p,v))return 0;meta->have_view=1; }
        if (!object_next(p,&done)) return 0;
    }
    space(p);
    if (p->p!=p->end) return p_fail(p,HHMS_IO_PARSE_FAILED,"trailing content after root object");
    if ((fields&(VERSION|TILES|SUPPORTS))!=(VERSION|TILES|SUPPORTS)) return p_fail(p,HHMS_IO_SCHEMA_FAILED,"root requires version, tiles, and supports");
    if (version!=1&&version!=HHMS_FILE_VERSION) { p->line=vline;p->col=vcol;return p_fail(p,HHMS_IO_UNSUPPORTED_VERSION,"only map versions 1 and 2 are supported"); }
    return validate_schema(p,version,m,meta);
}

HhmsIoResult hhms_load(HhmsMap *m,HhmsView *v,const char *path,HhmsError *e)
{
    char *data=NULL;size_t size=0;HhmsMap *tmp=NULL;HhmsView tv;Meta *meta=NULL;Parser p;HhmsIoResult r;
    hhms_error_clear(e);
    if (!m) return fail(e,HHMS_IO_SCHEMA_FAILED,0,0,0,"destination map is null");
    if (!path||!path[0]) return fail(e,HHMS_IO_OPEN_FAILED,0,0,EINVAL,"map path is empty");
    r=read_file(path,&data,&size,e);if(r!=HHMS_IO_OK)return r;
    tmp=(HhmsMap*)malloc(sizeof(*tmp));meta=(Meta*)calloc(1,sizeof(*meta));
    if(!tmp||!meta){free(meta);free(tmp);free(data);return fail(e,HHMS_IO_OUT_OF_MEMORY,0,0,0,"could not allocate temporary load state");}
    hhms_init(tmp);hhms_view_init(&tv);
    p.p=data;p.end=data+size;p.line=1;p.col=1;p.error=e;p.result=HHMS_IO_OK;
    if(!document(&p,tmp,&tv,meta)){r=p.result;free(meta);free(tmp);free(data);return r;}
    hhms_history_clear(tmp);*m=*tmp;if(v)*v=tv;
    free(meta);free(tmp);free(data);return HHMS_IO_OK;
}

typedef struct { FILE *f; int failed,oserr; } Output;

static int out_text(Output *o,const char *s)
{
    if(o->failed)return 0;errno=0;if(fputs(s,o->f)==EOF){o->failed=1;o->oserr=errno;return 0;}return 1;
}

static int out_fmt(Output *o,const char *fmt,...)
{
    va_list ap;int n;if(o->failed)return 0;errno=0;va_start(ap,fmt);n=vfprintf(o->f,fmt,ap);va_end(ap);
    if(n<0){o->failed=1;o->oserr=errno;return 0;}return 1;
}

static int number_text(char *buf,size_t cap,double value,int precision)
{
    char raw[128],*at,*digits,*write;
    int n=snprintf(raw,sizeof(raw),"%.*g",precision,value);
    const struct lconv *lc;
    const char *dp;
    if(n<0||(size_t)n>=sizeof(raw))return 0;
    lc=localeconv();dp=lc?lc->decimal_point:".";
    if(dp&&dp[0]&&strcmp(dp,".")){
        size_t dl=strlen(dp);
        at=strstr(raw,dp);
        if(at){size_t suffix=strlen(at+dl);memmove(at+1,at+dl,suffix+1);*at='.';}
    }
    at=strchr(raw,'e');if(!at)at=strchr(raw,'E');
    if(at){
        int negative;
        *at='e';digits=at+1;negative=*digits=='-';
        if(*digits=='+'||*digits=='-')digits++;
        while(digits[0]=='0'&&digits[1]>='0'&&digits[1]<='9')digits++;
        write=at+1;if(negative)*write++='-';
        memmove(write,digits,strlen(digits)+1);
    }
    if(strlen(raw)+1>cap)return 0;
    memcpy(buf,raw,strlen(raw)+1);
    return 1;
}

static int out_num(Output *o,double v,int precision)
{
    char buf[128];if(!number_text(buf,sizeof(buf),v,precision)){o->failed=1;o->oserr=0;return 0;}return out_text(o,buf);
}

static HhmsIoResult validate_save(const HhmsMap *m,const HhmsView *v,HhmsError *e)
{
    int i,j;
    if(!m)return fail(e,HHMS_IO_SCHEMA_FAILED,0,0,0,"source map is null");
    if(m->ntiles<0||m->ntiles>HHMS_MAX_TILES||m->nsupports<0||m->nsupports>HHMS_MAX_SUPPORTS)
        return fail(e,HHMS_IO_RANGE_FAILED,0,0,0,"map entry count is out of range");
    for(i=0;i<m->ntiles;i++){
        const HhmsTile *t=&m->tiles[i];
        if(t->x< -HHMS_COORD_LIMIT||t->x>HHMS_COORD_LIMIT||t->y< -HHMS_COORD_LIMIT||t->y>HHMS_COORD_LIMIT)
            return fail(e,HHMS_IO_RANGE_FAILED,0,0,0,"tile coordinate exceeds HHMS_COORD_LIMIT");
        if(!tile_name(t->kind)||!terrain_name(t->terrain))return fail(e,HHMS_IO_SCHEMA_FAILED,0,0,0,"map contains an unknown tile enum");
        if(t->kind==HHMS_CLEAR&&(t->count<0||t->count>8))return fail(e,HHMS_IO_RANGE_FAILED,0,0,0,"clear count must be between 0 and 8");
        if(t->terrain==HHMS_TERRAIN_WATER&&(t->kind==HHMS_CLEAR||t->kind==HHMS_OPEN))return fail(e,HHMS_IO_SCHEMA_FAILED,0,0,0,"water cannot be clear or open");
        if(hhms_get(m,t->x,t->y)!=t)return fail(e,HHMS_IO_SCHEMA_FAILED,0,0,0,"map tile hash is inconsistent");
    }
    for(i=0;i<m->nsupports;i++){
        const HhmsSupport *s=&m->supports[i];
        if(!isfinite(s->x)||!isfinite(s->y)||s->x<-(double)HHMS_COORD_LIMIT||s->x>(double)HHMS_COORD_LIMIT||s->y<-(double)HHMS_COORD_LIMIT||s->y>(double)HHMS_COORD_LIMIT)
            return fail(e,HHMS_IO_RANGE_FAILED,0,0,0,"support position exceeds HHMS_COORD_LIMIT");
        if(s->kind<0||s->kind>=HHMS_SUP_COUNT||!hhms_support_name(s->kind)||!orient_name(s->orientation))return fail(e,HHMS_IO_SCHEMA_FAILED,0,0,0,"map contains an unknown support enum");
        if(!s->id)return fail(e,HHMS_IO_SCHEMA_FAILED,0,0,0,"support has no assigned ID");
        for(j=0;j<i;j++) {
            if(m->supports[j].id==s->id)
                return fail(e,HHMS_IO_SCHEMA_FAILED,0,0,0,"map contains duplicate support IDs");
            if(m->supports[j].x==s->x&&m->supports[j].y==s->y)
                return fail(e,HHMS_IO_SCHEMA_FAILED,0,0,0,"map contains duplicate support coordinates");
        }
    }
    if(v&&v->has_view&&(!isfinite(v->camx)||!isfinite(v->camy)||v->camx<-(double)HHMS_COORD_LIMIT||v->camx>(double)HHMS_COORD_LIMIT||v->camy<-(double)HHMS_COORD_LIMIT||v->camy>(double)HHMS_COORD_LIMIT||!isfinite(v->cell)||!isfinite(v->ui_scale)||v->cell<14.0f||v->cell>72.0f||v->ui_scale<0.75f||v->ui_scale>2.0f))
        return fail(e,HHMS_IO_RANGE_FAILED,0,0,0,"view cell must be 14-72 and ui_scale 0.75-2.0");
    return HHMS_IO_OK;
}
typedef int (*IndexCompare)(const HhmsMap *, int, int);

static int compare_tiles(const HhmsMap *m,int ai,int bi)
{
    const HhmsTile *a=&m->tiles[ai],*b=&m->tiles[bi];
    if(a->y!=b->y)return a->y<b->y?-1:1;
    if(a->x!=b->x)return a->x<b->x?-1:1;
    return 0;
}

static int compare_supports(const HhmsMap *m,int ai,int bi)
{
    const HhmsSupport *a=&m->supports[ai],*b=&m->supports[bi];
    if(a->y!=b->y)return a->y<b->y?-1:1;
    if(signbit(a->y)!=signbit(b->y))return signbit(a->y)?-1:1;
    if(a->x!=b->x)return a->x<b->x?-1:1;
    if(signbit(a->x)!=signbit(b->x))return signbit(a->x)?-1:1;
    if(a->kind!=b->kind)return a->kind<b->kind?-1:1;
    if(a->orientation!=b->orientation)return a->orientation<b->orientation?-1:1;
    if(a->id!=b->id)return a->id<b->id?-1:1;
    return 0;
}

static void sift_order(int *order,int root,int count,const HhmsMap *m,
                       IndexCompare compare)
{
    for(;;){
        int child=root*2+1,largest=root,temp;
        if(child<count&&compare(m,order[largest],order[child])<0)largest=child;
        if(child+1<count&&compare(m,order[largest],order[child+1])<0)largest=child+1;
        if(largest==root)return;
        temp=order[root];order[root]=order[largest];order[largest]=temp;root=largest;
    }
}

static void sort_order(int *order,int count,const HhmsMap *m,IndexCompare compare)
{
    int i;
    for(i=0;i<count;i++)order[i]=i;
    for(i=count/2;i>0;i--)sift_order(order,i-1,count,m,compare);
    for(i=count-1;i>0;i--){
        int temp=order[0];order[0]=order[i];order[i]=temp;
        sift_order(order,0,i,m,compare);
    }
}


static int write_doc(Output *o,const HhmsMap *m,const HhmsView *v,
                     const int *tile_order,const int *support_order)
{
    int i;
    if(!out_text(o,"{\n  \"version\": 2,\n  \"tiles\": ["))return 0;
    for(i=0;i<m->ntiles;i++){
        const HhmsTile *t=&m->tiles[tile_order[i]];
        if(!out_text(o,i?",\n    {\"x\": ":"\n    {\"x\": ")||!out_fmt(o,"%d, \"y\": %d, \"terrain\": \"%s\", \"kind\": \"%s\"",t->x,t->y,terrain_name(t->terrain),tile_name(t->kind)))return 0;
        if(t->kind==HHMS_CLEAR&&!out_fmt(o,", \"n\": %d",t->count))return 0;
        if(!out_text(o,"}"))return 0;
    }
    if(!out_text(o,"\n  ],\n  \"supports\": ["))return 0;
    for(i=0;i<m->nsupports;i++){
        const HhmsSupport *s=&m->supports[support_order[i]];
        if(!out_text(o,i?",\n    {\"x\": ":"\n    {\"x\": ")||!out_num(o,s->x,17)||!out_text(o,", \"y\": ")||!out_num(o,s->y,17)||!out_fmt(o,", \"kind\": \"%s\", \"orientation\": \"%s\"}",hhms_support_name(s->kind),orient_name(s->orientation)))return 0;
    }
    if(!out_text(o,"\n  ]"))return 0;
    if(v&&v->has_view){if(!out_text(o,",\n  \"view\": {\"camx\": ")||!out_num(o,v->camx,17)||!out_text(o,", \"camy\": ")||!out_num(o,v->camy,17)||!out_text(o,", \"cell\": ")||!out_num(o,v->cell,9)||!out_text(o,", \"ui_scale\": ")||!out_num(o,v->ui_scale,9)||!out_text(o,"}"))return 0;}
    return out_text(o,"\n}\n");
}

#ifdef _WIN32
static LONG temp_sequence;

static FILE *create_temp_file(const char *path, wchar_t **destination,
                              wchar_t **temporary, int *os_error, int *oom)
{
    wchar_t *dest = NULL;
    wchar_t *candidate = NULL;
    *destination = NULL;
    *temporary = NULL;
    *oom = 0;
    int converted = to_wide(path, &dest, os_error);
    if (converted < 0) {
        *oom = 1;
        return NULL;
    }
    if (!converted)
        return NULL;
    size_t capacity = wcslen(dest) + 64;
    candidate = (wchar_t *)malloc(capacity * sizeof(*candidate));
    if (!candidate) {
        free(dest);
        *oom = 1;
        return NULL;
    }
    for (int attempt = 0; attempt < 128; attempt++) {
        LONG sequence = InterlockedIncrement(&temp_sequence);
        int written = swprintf(candidate, capacity, L"%ls.tmp.%lu.%ld",
                               dest, (unsigned long)GetCurrentProcessId(),
                               (long)sequence);
        if (written < 0 || (size_t)written >= capacity) {
            free(candidate);
            free(dest);
            *oom = 1;
            return NULL;
        }
        HANDLE handle = CreateFileW(candidate, GENERIC_READ | GENERIC_WRITE, 0,
                                    NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (handle == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS)
                continue;
            *os_error = (int)error;
            free(candidate);
            free(dest);
            return NULL;
        }
        int descriptor = _open_osfhandle((intptr_t)handle,
                                         _O_BINARY | _O_RDWR | _O_NOINHERIT);
        if (descriptor < 0) {
            *os_error = errno;
            CloseHandle(handle);
            DeleteFileW(candidate);
            free(candidate);
            free(dest);
            return NULL;
        }
        FILE *file = _fdopen(descriptor, "w+b");
        if (!file) {
            *os_error = errno;
            _close(descriptor);
            DeleteFileW(candidate);
            free(candidate);
            free(dest);
            return NULL;
        }
        *destination = dest;
        *temporary = candidate;
        return file;
    }
    *os_error = ERROR_FILE_EXISTS;
    free(candidate);
    free(dest);
    return NULL;
}

static void remove_temp_file(const wchar_t *path)
{
    if (path)
        DeleteFileW(path);
}

static int replace_destination(const wchar_t *temporary,
                               const wchar_t *destination, int *os_error)
{
    DWORD attributes = GetFileAttributesW(destination);
    BOOL replaced;
    if (attributes != INVALID_FILE_ATTRIBUTES) {
        replaced = ReplaceFileW(destination, temporary, NULL,
                                REPLACEFILE_WRITE_THROUGH, NULL, NULL);
    } else {
        replaced = MoveFileExW(temporary, destination, MOVEFILE_WRITE_THROUGH);
    }
    if (!replaced) {
        *os_error = (int)GetLastError();
        return 0;
    }
    return 1;
}
#else
static FILE *create_temp_file(const char *path, char **temporary,
                              int *os_error, int *oom)
{
    *temporary = NULL;
    *oom = 0;
    size_t length = strlen(path);
    static const char suffix[] = ".tmp.XXXXXX";
    if (length > SIZE_MAX - sizeof(suffix)) {
        *oom = 1;
        return NULL;
    }
    char *candidate = (char *)malloc(length + sizeof(suffix));
    if (!candidate) {
        *oom = 1;
        return NULL;
    }
    memcpy(candidate, path, length);
    memcpy(candidate + length, suffix, sizeof(suffix));

    struct stat destination_stat;
    int preserve_mode = 0;
    if (stat(path, &destination_stat) == 0) {
        preserve_mode = 1;
    } else if (errno != ENOENT) {
        *os_error = errno;
        free(candidate);
        return NULL;
    }

    errno = 0;
    int descriptor = mkstemp(candidate);
    if (descriptor < 0) {
        *os_error = errno;
        free(candidate);
        return NULL;
    }
    if (preserve_mode &&
        fchmod(descriptor, destination_stat.st_mode & 07777) != 0) {
        *os_error = errno;
        close(descriptor);
        unlink(candidate);
        free(candidate);
        return NULL;
    }
    FILE *file = fdopen(descriptor, "w+b");
    if (!file) {
        *os_error = errno;
        close(descriptor);
        unlink(candidate);
        free(candidate);
        return NULL;
    }
    *temporary = candidate;
    return file;
}

static void remove_temp_file(const char *path)
{
    if (path)
        unlink(path);
}
#endif

HhmsIoResult hhms_save(const HhmsMap *m, const HhmsView *v,
                       const char *path, HhmsError *e)
{
    FILE *file = NULL;
    Output output;
    HhmsIoResult result;
    int os_error = 0;
    int oom = 0;
    int tile_order[HHMS_MAX_TILES];
    int support_order[HHMS_MAX_SUPPORTS];
#ifdef _WIN32
    wchar_t *temporary = NULL;
    wchar_t *destination = NULL;
#else
    char *temporary = NULL;
#endif

    hhms_error_clear(e);
    result = validate_save(m, v, e);
    if (result != HHMS_IO_OK)
        return result;
    if (!path || !path[0])
        return fail(e, HHMS_IO_OPEN_FAILED, 0, 0, EINVAL,
                    "map path is empty");
    sort_order(tile_order, m->ntiles, m, compare_tiles);
    sort_order(support_order, m->nsupports, m, compare_supports);

#ifdef _WIN32
    file = create_temp_file(path, &destination, &temporary, &os_error, &oom);
#else
    file = create_temp_file(path, &temporary, &os_error, &oom);
#endif
    if (!file) {
        return fail(e, oom ? HHMS_IO_OUT_OF_MEMORY : HHMS_IO_OPEN_FAILED,
                    0, 0, os_error, oom ? "could not allocate a unique temporary path"
                                        : "could not create a unique temporary map file");
    }

    output.f = file;
    output.failed = 0;
    output.oserr = 0;
    write_doc(&output, m, v, tile_order, support_order);
    if (output.failed) {
        fclose(file);
        remove_temp_file(temporary);
#ifdef _WIN32
        free(destination);
#endif
        free(temporary);
        return fail(e, HHMS_IO_WRITE_FAILED, 0, 0, output.oserr,
                    "could not write the complete map");
    }
    errno = 0;
    if (fflush(file) != 0) {
        os_error = errno;
        fclose(file);
        remove_temp_file(temporary);
#ifdef _WIN32
        free(destination);
#endif
        free(temporary);
        return fail(e, HHMS_IO_FLUSH_FAILED, 0, 0, os_error,
                    "could not flush the temporary map file");
    }
    errno = 0;
#ifdef _WIN32
    if (_commit(_fileno(file)) != 0)
#else
    if (fsync(fileno(file)) != 0)
#endif
    {
        os_error = errno;
        fclose(file);
        remove_temp_file(temporary);
#ifdef _WIN32
        free(destination);
#endif
        free(temporary);
        return fail(e, HHMS_IO_FLUSH_FAILED, 0, 0, os_error,
                    "could not durably flush the temporary map file");
    }
    errno = 0;
    if (fclose(file) != 0) {
        os_error = errno;
        remove_temp_file(temporary);
#ifdef _WIN32
        free(destination);
#endif
        free(temporary);
        return fail(e, HHMS_IO_FLUSH_FAILED, 0, 0, os_error,
                    "could not close the temporary map file");
    }

#ifdef _WIN32
    if (!replace_destination(temporary, destination, &os_error)) {
        remove_temp_file(temporary);
        free(destination);
        free(temporary);
        return fail(e, HHMS_IO_REPLACE_FAILED, 0, 0, os_error,
                    "could not replace the destination map");
    }
    free(destination);
#else
    errno = 0;
    if (rename(temporary, path) != 0) {
        os_error = errno;
        remove_temp_file(temporary);
        free(temporary);
        return fail(e, HHMS_IO_REPLACE_FAILED, 0, 0, os_error,
                    "could not replace the destination map");
    }
#endif
    free(temporary);
    return HHMS_IO_OK;
}
