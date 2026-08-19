#include "hhms.h"

#include <string.h>

static const int DX[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
static const int DY[8] = {-1, -1, -1, 0, 0, 1, 1, 1};

static int is_mine(const HhmsTile *t)
{
    if (!t)
        return 0;
    return t->kind == HHMS_MINE || t->mark == HHMS_MARK_MINE;
}

static int is_clear(const HhmsTile *t)
{
    return t && t->kind == HHMS_CLEAR;
}

static int is_safe(const HhmsTile *t)
{
    if (!t)
        return 0;
    return t->kind == HHMS_CLEAR || t->kind == HHMS_OPEN || t->mark == HHMS_MARK_SAFE;
}



static int mark_safe(HhmsMap *m, int x, int y)
{
    HhmsTile *t = hhms_get_mut(m, x, y);
    if (t && t->kind == HHMS_MINE) {
        t->mark = HHMS_MARK_CONFLICT;
        m->contradiction = 1;
        return 0;
    }
    if (is_safe(t))
        return 0;
    if (t && t->mark == HHMS_MARK_CONFLICT) {
        m->contradiction = 1;
        return 0;
    }
    t = hhms_touch(m, x, y);
    if (!t)
        return 0;
    if (t->mark == HHMS_MARK_MINE) {
        t->mark = HHMS_MARK_CONFLICT;
        m->contradiction = 1;
        return 0;
    }
    if (t->mark == HHMS_MARK_CONFLICT) {
        m->contradiction = 1;
        return 0;
    }
    t->mark = HHMS_MARK_SAFE;
    t->p_mine = 0.f;
    return 1;
}

static int mark_mine(HhmsMap *m, int x, int y)
{
    HhmsTile *t = hhms_get_mut(m, x, y);
    if (t && (t->kind == HHMS_CLEAR || t->kind == HHMS_OPEN)) {
        t->mark = HHMS_MARK_CONFLICT;
        m->contradiction = 1;
        return 0;
    }
    if (t && t->kind == HHMS_MINE)
        return 0;
    if (t && t->mark == HHMS_MARK_MINE)
        return 0;
    if (t && t->mark == HHMS_MARK_CONFLICT) {
        m->contradiction = 1;
        return 0;
    }
    t = hhms_touch(m, x, y);
    if (!t)
        return 0;
    if (t->mark == HHMS_MARK_SAFE) {
        t->mark = HHMS_MARK_CONFLICT;
        m->contradiction = 1;
        return 0;
    }
    if (t->mark == HHMS_MARK_CONFLICT) {
        m->contradiction = 1;
        return 0;
    }
    t->mark = HHMS_MARK_MINE;
    t->p_mine = 1.f;
    return 1;
}

typedef struct {
    int x[8], y[8];
    int n;
    int need;
} Constr;

static int gather(const HhmsMap *m, const HhmsTile *c, Constr *out)
{
    out->n = 0;
    out->need = c->count;
    for (int i = 0; i < 8; i++) {
        int x = c->x + DX[i];
        int y = c->y + DY[i];
        const HhmsTile *t = hhms_get(m, x, y);
        if (is_clear(t))
            continue;
        if (is_mine(t)) {
            out->need--;
            continue;
        }
        if (is_safe(t))
            continue;
        if (out->n >= 8)
            return 0;
        out->x[out->n] = x;
        out->y[out->n] = y;
        out->n++;
    }
    return 1;
}

static int apply_simple(HhmsMap *m)
{
    int changed = 0;
    int n = m->ntiles;
    for (int i = 0; i < n; i++) {
        HhmsTile *c = &m->tiles[i];
        if (c->kind != HHMS_CLEAR)
            continue;
        Constr g;
        if (!gather(m, c, &g))
            continue;
        if (g.need < 0 || g.need > g.n) {
            c->mark = HHMS_MARK_CONFLICT;
            m->contradiction = 1;
            continue;
        }
        if (g.need == 0) {
            for (int k = 0; k < g.n; k++)
                changed |= mark_safe(m, g.x[k], g.y[k]);
        } else if (g.need == g.n) {
            for (int k = 0; k < g.n; k++)
                changed |= mark_mine(m, g.x[k], g.y[k]);
        }
    }
    return changed;
}

static int same_cell(int x1, int y1, int x2, int y2)
{
    return x1 == x2 && y1 == y2;
}

static int is_subset(const Constr *a, const Constr *b)
{
    for (int i = 0; i < a->n; i++) {
        int found = 0;
        for (int j = 0; j < b->n; j++) {
            if (same_cell(a->x[i], a->y[i], b->x[j], b->y[j])) {
                found = 1;
                break;
            }
        }
        if (!found)
            return 0;
    }
    return 1;
}

static int extra_cells(const Constr *big, const Constr *small, Constr *out)
{
    out->n = 0;
    for (int i = 0; i < big->n; i++) {
        int in_small = 0;
        for (int j = 0; j < small->n; j++) {
            if (same_cell(big->x[i], big->y[i], small->x[j], small->y[j])) {
                in_small = 1;
                break;
            }
        }
        if (!in_small) {
            out->x[out->n] = big->x[i];
            out->y[out->n] = big->y[i];
            out->n++;
        }
    }
    out->need = big->need - small->need;
    return 1;
}

#define MAX_CONSTR 2048

static int collect_constr(const HhmsMap *m, Constr *cs, int cap)
{
    int n = 0;
    for (int i = 0; i < m->ntiles && n < cap; i++) {
        if (m->tiles[i].kind != HHMS_CLEAR)
            continue;
        if (!gather(m, &m->tiles[i], &cs[n]))
            continue;
        if (cs[n].n > 0)
            n++;
    }
    return n;
}

static int apply_subset(HhmsMap *m)
{
    static Constr cs[MAX_CONSTR];
    int nc = collect_constr(m, cs, MAX_CONSTR);
    int changed = 0;
    for (int i = 0; i < nc; i++) {
        for (int j = 0; j < nc; j++) {
            if (i == j || cs[i].n == 0 || cs[j].n == 0)
                continue;
            if (!is_subset(&cs[i], &cs[j]))
                continue;
            Constr ex;
            extra_cells(&cs[j], &cs[i], &ex);
            if (ex.need < 0 || ex.need > ex.n) {
                m->contradiction = 1;
                continue;
            }
            if (ex.n == 0)
                continue;
            if (ex.need == 0) {
                for (int k = 0; k < ex.n; k++)
                    changed |= mark_safe(m, ex.x[k], ex.y[k]);
            } else if (ex.need == ex.n) {
                for (int k = 0; k < ex.n; k++)
                    changed |= mark_mine(m, ex.x[k], ex.y[k]);
            }
        }
    }
    return changed;
}

typedef struct {
    int x, y;
} Cell;

static int popcnt(unsigned v)
{
    v = v - ((v >> 1) & 0x55555555u);
    v = (v & 0x33333333u) + ((v >> 2) & 0x33333333u);
    return (int)((((v + (v >> 4)) & 0x0F0F0F0Fu) * 0x01010101u) >> 24);
}

typedef struct {
    unsigned mask;
    int need;
} BitC;

typedef struct {
    unsigned long long nvalid;
    unsigned long long minec[HHMS_ENUM_MAX];
    int n;
    int nc;
    BitC cs[256];
} EnumAcc;

static int partial_ok(const EnumAcc *e, int bit, unsigned assigned)
{
    unsigned decided = bit == 0 ? 0u : ((1u << bit) - 1u);
    for (int i = 0; i < e->nc; i++) {
        unsigned m = e->cs[i].mask;
        int have = popcnt(assigned & m & decided);
        int remain = popcnt(m & ~decided);
        if (have > e->cs[i].need || have + remain < e->cs[i].need)
            return 0;
    }
    return 1;
}

static void enum_rec(EnumAcc *e, int bit, unsigned assigned)
{
    if (bit == e->n) {
        e->nvalid++;
        for (int i = 0; i < e->n; i++) {
            if (assigned & (1u << i))
                e->minec[i]++;
        }
        return;
    }
    if (partial_ok(e, bit + 1, assigned))
        enum_rec(e, bit + 1, assigned);
    unsigned with = assigned | (1u << bit);
    if (partial_ok(e, bit + 1, with))
        enum_rec(e, bit + 1, with);
}

static int uf_find(int *p, int a)
{
    while (p[a] != a) {
        p[a] = p[p[a]];
        a = p[a];
    }
    return a;
}

static void uf_union(int *p, int a, int b)
{
    a = uf_find(p, a);
    b = uf_find(p, b);
    if (a != b)
        p[b] = a;
}

static int cell_id(Cell *cells, int *n, int cap, int x, int y)
{
    for (int i = 0; i < *n; i++) {
        if (cells[i].x == x && cells[i].y == y)
            return i;
    }
    if (*n >= cap)
        return -1;
    cells[*n].x = x;
    cells[*n].y = y;
    return (*n)++;
}

#define MAX_CELLS 1024

static int apply_enum(HhmsMap *m, int write_odds)
{
    static Constr cs[MAX_CONSTR];
    int nc = collect_constr(m, cs, MAX_CONSTR);
    if (nc == 0)
        return 0;

    static Cell cells[MAX_CELLS];
    int ncells = 0;
    static int cmap[MAX_CONSTR][8];
    memset(cmap, -1, sizeof(cmap));
    for (int i = 0; i < nc; i++) {
        for (int k = 0; k < cs[i].n; k++) {
            int id = cell_id(cells, &ncells, MAX_CELLS, cs[i].x[k], cs[i].y[k]);
            if (id < 0)
                return 0;
            cmap[i][k] = id;
        }
    }
    if (ncells == 0)
        return 0;

    int parent[MAX_CELLS];
    for (int i = 0; i < ncells; i++)
        parent[i] = i;
    for (int i = 0; i < nc; i++) {
        if (cs[i].n <= 0)
            continue;
        int a = cmap[i][0];
        for (int k = 1; k < cs[i].n; k++)
            uf_union(parent, a, cmap[i][k]);
    }

    int changed = 0;
    int seen[MAX_CELLS];
    memset(seen, 0, sizeof(seen));
    for (int root_i = 0; root_i < ncells; root_i++) {
        int root = uf_find(parent, root_i);
        if (seen[root])
            continue;
        seen[root] = 1;

        int local[HHMS_ENUM_MAX];
        int ln = 0;
        for (int i = 0; i < ncells; i++) {
            if (uf_find(parent, i) == root) {
                if (ln >= HHMS_ENUM_MAX) {
                    ln = -1;
                    break;
                }
                local[ln++] = i;
            }
        }
        if (ln < 0)
            continue;

        EnumAcc acc;
        memset(&acc, 0, sizeof(acc));
        acc.n = ln;
        int glob_to_loc[MAX_CELLS];
        memset(glob_to_loc, -1, sizeof(glob_to_loc));
        for (int i = 0; i < ln; i++)
            glob_to_loc[local[i]] = i;

        int overflow = 0;
        for (int i = 0; i < nc; i++) {
            if (cs[i].n <= 0)
                continue;
            if (uf_find(parent, cmap[i][0]) != root)
                continue;
            unsigned mask = 0;
            for (int k = 0; k < cs[i].n; k++) {
                int loc = glob_to_loc[cmap[i][k]];
                if (loc < 0)
                    continue;
                mask |= 1u << loc;
            }
            if (acc.nc < 256) {
                acc.cs[acc.nc].mask = mask;
                acc.cs[acc.nc].need = cs[i].need;
                acc.nc++;
            } else {
                overflow = 1;
            }
        }
        if (overflow || acc.nc == 0 || acc.n == 0)
            continue;

        enum_rec(&acc, 0, 0);
        if (acc.nvalid == 0) {
            m->contradiction = 1;
            continue;
        }
        for (int i = 0; i < ln; i++) {
            int x = cells[local[i]].x;
            int y = cells[local[i]].y;
            float p = (float)acc.minec[i] / (float)acc.nvalid;
            if (acc.minec[i] == acc.nvalid)
                changed |= mark_mine(m, x, y);
            else if (acc.minec[i] == 0)
                changed |= mark_safe(m, x, y);
            else if (write_odds) {
                HhmsTile *t = hhms_touch(m, x, y);
                if (t && t->mark == HHMS_MARK_NONE)
                    t->p_mine = p;
            }
        }
    }
    return changed;
}

void hhms_solve(HhmsMap *m)
{
    m->contradiction = 0;
    hhms_drop_ephemeral(m);

    for (int pass = 0; pass < 32; pass++) {
        int changed = 0;
        for (int i = 0; i < 32; i++) {
            int c = apply_simple(m);
            c |= apply_subset(m);
            changed |= c;
            if (!c)
                break;
        }
        changed |= apply_enum(m, 0);
        if (!changed)
            break;
    }
    apply_enum(m, 1);

    for (int i = 0; i < m->ntiles; i++) {
        HhmsTile *t = &m->tiles[i];
        if (t->kind == HHMS_MINE)
            t->p_mine = 1.f;
        else if (t->kind == HHMS_CLEAR || t->kind == HHMS_OPEN)
            t->p_mine = 0.f;
        else if (t->mark == HHMS_MARK_MINE)
            t->p_mine = 1.f;
        else if (t->mark == HHMS_MARK_SAFE)
            t->p_mine = 0.f;
    }
}
