#include "hhms.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


static unsigned hash_xy(int x, int y)
{
    unsigned h = (unsigned)x * 0x9E3779B9u ^ (unsigned)y * 0x85EBCA6Bu;
    return h & (HHMS_HASH - 1);
}

static void hash_clear(HhmsMap *m)
{
    memset(m->hash, 0, sizeof(m->hash));
}

static int hash_find(const HhmsMap *m, int x, int y)
{
    unsigned h = hash_xy(x, y);
    for (int n = 0; n < HHMS_HASH; n++) {
        const HhmsSlot *s = &m->hash[h];
        if (s->occ == 0)
            return -1;
        if (s->occ == 1 && s->x == x && s->y == y)
            return s->ti;
        h = (h + 1) & (HHMS_HASH - 1);
    }
    return -1;
}

static int hash_put(HhmsMap *m, int x, int y, int ti)
{
    unsigned h = hash_xy(x, y);
    int tomb = -1;
    for (int n = 0; n < HHMS_HASH; n++) {
        HhmsSlot *s = &m->hash[h];
        if (s->occ == 1 && s->x == x && s->y == y) {
            s->ti = ti;
            return 0;
        }
        if (s->occ == 2 && tomb < 0)
            tomb = (int)h;
        if (s->occ == 0) {
            int at = tomb >= 0 ? tomb : (int)h;
            m->hash[at].occ = 1;
            m->hash[at].x = x;
            m->hash[at].y = y;
            m->hash[at].ti = ti;
            return 0;
        }
        h = (h + 1) & (HHMS_HASH - 1);
    }
    if (tomb >= 0) {
        m->hash[tomb].occ = 1;
        m->hash[tomb].x = x;
        m->hash[tomb].y = y;
        m->hash[tomb].ti = ti;
        return 0;
    }
    return -1;
}

static void hash_del(HhmsMap *m, int x, int y)
{
    unsigned h = hash_xy(x, y);
    for (int n = 0; n < HHMS_HASH; n++) {
        HhmsSlot *s = &m->hash[h];
        if (s->occ == 0)
            return;
        if (s->occ == 1 && s->x == x && s->y == y) {
            s->occ = 2;
            return;
        }
        h = (h + 1) & (HHMS_HASH - 1);
    }
}

static void rebuild_hash(HhmsMap *m)
{
    hash_clear(m);
    for (int i = 0; i < m->ntiles; i++)
        hash_put(m, m->tiles[i].x, m->tiles[i].y, i);
}

void hhms_init(HhmsMap *m)
{
    memset(m, 0, sizeof(*m));
}

const HhmsTile *hhms_get(const HhmsMap *m, int x, int y)
{
    int ti = hash_find(m, x, y);
    if (ti < 0)
        return NULL;
    return &m->tiles[ti];
}

HhmsTile *hhms_get_mut(HhmsMap *m, int x, int y)
{
    int ti = hash_find(m, x, y);
    if (ti < 0)
        return NULL;
    return &m->tiles[ti];
}

static HhmsTile *touch(HhmsMap *m, int x, int y)
{
    int ti = hash_find(m, x, y);
    if (ti >= 0)
        return &m->tiles[ti];
    if (m->ntiles >= HHMS_MAX_TILES)
        return NULL;
    ti = m->ntiles++;
    memset(&m->tiles[ti], 0, sizeof(m->tiles[ti]));
    m->tiles[ti].x = x;
    m->tiles[ti].y = y;
    m->tiles[ti].kind = HHMS_UNKNOWN;
    m->tiles[ti].p_mine = -1.f;
    if (hash_put(m, x, y, ti) != 0) {
        m->ntiles--;
        return NULL;
    }
    return &m->tiles[ti];
}

static void push_tile_undo(HhmsMap *m, int x, int y, const HhmsTile *old, int had)
{
    if (m->nundo >= HHMS_MAX_UNDO) {
        memmove(&m->undo[0], &m->undo[1], sizeof(m->undo[0]) * (HHMS_MAX_UNDO - 1));
        m->nundo = HHMS_MAX_UNDO - 1;
    }
    HhmsUndo *u = &m->undo[m->nundo++];
    u->op = HHMS_UNDO_TILE;
    u->x = x;
    u->y = y;
    u->had = had;
    if (had && old) {
        u->kind = old->kind;
        u->count = old->count;
        u->user = old->user;
    } else {
        u->kind = HHMS_UNKNOWN;
        u->count = 0;
        u->user = 0;
    }
}

static void drop_tile(HhmsMap *m, int ti)
{
    hash_del(m, m->tiles[ti].x, m->tiles[ti].y);
    m->tiles[ti] = m->tiles[m->ntiles - 1];
    m->ntiles--;
    if (ti < m->ntiles)
        hash_put(m, m->tiles[ti].x, m->tiles[ti].y, ti);
}

int hhms_set_tile(HhmsMap *m, int x, int y, HhmsKind kind, int count, int record_undo)
{
    if (kind == HHMS_CLEAR && (count < 0 || count > 8))
        return -1;
    HhmsTile *t = hhms_get_mut(m, x, y);
    int had = t != NULL;
    if (had && t->kind == kind && (kind != HHMS_CLEAR || t->count == count) && t->user)
        return 0;
    if (record_undo)
        push_tile_undo(m, x, y, t, had);
    if (!t) {
        t = touch(m, x, y);
        if (!t)
            return -1;
    }
    t->kind = kind;
    t->count = kind == HHMS_CLEAR ? count : 0;
    t->user = 1;
    t->mark = HHMS_MARK_NONE;
    t->p_mine = -1.f;
    return 0;
}

int hhms_erase_tile(HhmsMap *m, int x, int y, int record_undo)
{
    HhmsTile *t = hhms_get_mut(m, x, y);
    if (!t)
        return 0;
    if (record_undo)
        push_tile_undo(m, x, y, t, 1);
    drop_tile(m, (int)(t - m->tiles));
    return 0;
}

int hhms_support_index(const HhmsMap *m, int x, int y)
{
    for (int i = 0; i < m->nsupports; i++) {
        if (m->supports[i].x == x && m->supports[i].y == y)
            return i;
    }
    return -1;
}

float hhms_support_radius(HhmsSupportKind kind)
{
    switch (kind) {
    case HHMS_SUP_WOOD:     return 100.f / 11.f;
    case HHMS_SUP_STONE:    return 125.f / 11.f;
    case HHMS_SUP_BEAM:     return 150.f / 11.f;
    case HHMS_SUP_MONUMENT: return 30.f;
    }
    return 0.f;
}

const char *hhms_support_name(HhmsSupportKind kind)
{
    switch (kind) {
    case HHMS_SUP_WOOD:     return "wood";
    case HHMS_SUP_STONE:    return "stone";
    case HHMS_SUP_BEAM:     return "beam";
    case HHMS_SUP_MONUMENT: return "monument";
    }
    return "wood";
}

static HhmsSupportKind support_from_name(const char *s)
{
    if (strcmp(s, "stone") == 0) return HHMS_SUP_STONE;
    if (strcmp(s, "beam") == 0) return HHMS_SUP_BEAM;
    if (strcmp(s, "monument") == 0) return HHMS_SUP_MONUMENT;
    return HHMS_SUP_WOOD;
}

int hhms_covered(const HhmsMap *m, int x, int y)
{
    for (int i = 0; i < m->nsupports; i++) {
        const HhmsSupport *s = &m->supports[i];
        float r = hhms_support_radius(s->kind);
        float dx = (float)(x - s->x);
        float dy = (float)(y - s->y);
        if (dx * dx + dy * dy <= r * r + 1e-4f)
            return 1;
    }
    return 0;
}

static void push_sup_undo(HhmsMap *m, HhmsUndoOp op, int x, int y, HhmsSupportKind kind)
{
    if (m->nundo >= HHMS_MAX_UNDO) {
        memmove(&m->undo[0], &m->undo[1], sizeof(m->undo[0]) * (HHMS_MAX_UNDO - 1));
        m->nundo = HHMS_MAX_UNDO - 1;
    }
    HhmsUndo *u = &m->undo[m->nundo++];
    memset(u, 0, sizeof(*u));
    u->op = op;
    u->x = x;
    u->y = y;
    u->sup_kind = kind;
}

int hhms_add_support(HhmsMap *m, int x, int y, HhmsSupportKind kind, int record_undo)
{
    int i = hhms_support_index(m, x, y);
    if (i >= 0) {
        if (m->supports[i].kind == kind)
            return 0;
        if (record_undo) {
            push_sup_undo(m, HHMS_UNDO_SUP_DEL, x, y, m->supports[i].kind);
            push_sup_undo(m, HHMS_UNDO_SUP_ADD, x, y, kind);
        }
        m->supports[i].kind = kind;
        return 0;
    }
    if (m->nsupports >= HHMS_MAX_SUPPORTS)
        return -1;
    if (record_undo)
        push_sup_undo(m, HHMS_UNDO_SUP_ADD, x, y, kind);
    m->supports[m->nsupports].x = x;
    m->supports[m->nsupports].y = y;
    m->supports[m->nsupports].kind = kind;
    m->nsupports++;
    return 0;
}

int hhms_del_support_at(HhmsMap *m, int x, int y, int record_undo)
{
    int i = hhms_support_index(m, x, y);
    if (i < 0)
        return 0;
    if (record_undo)
        push_sup_undo(m, HHMS_UNDO_SUP_DEL, x, y, m->supports[i].kind);
    m->supports[i] = m->supports[m->nsupports - 1];
    m->nsupports--;
    return 0;
}

int hhms_undo(HhmsMap *m)
{
    if (m->nundo <= 0)
        return 0;
    HhmsUndo u = m->undo[--m->nundo];
    if (u.op == HHMS_UNDO_TILE) {
        if (!u.had)
            hhms_erase_tile(m, u.x, u.y, 0);
        else if (u.kind == HHMS_UNKNOWN && !u.user)
            hhms_erase_tile(m, u.x, u.y, 0);
        else
            hhms_set_tile(m, u.x, u.y, u.kind, u.count, 0);
        return 1;
    }
    if (u.op == HHMS_UNDO_SUP_ADD) {
        hhms_del_support_at(m, u.x, u.y, 0);
        return 1;
    }
    if (u.op == HHMS_UNDO_SUP_DEL) {
        hhms_add_support(m, u.x, u.y, u.sup_kind, 0);
        return 1;
    }
    return 0;
}

/* ---- JSON ---- */

typedef struct {
    const char *p;
    const char *end;
} Json;

static void jskip(Json *j)
{
    while (j->p < j->end && isspace((unsigned char)*j->p))
        j->p++;
}

static int jneed(Json *j, char c)
{
    jskip(j);
    if (j->p >= j->end || *j->p != c)
        return 0;
    j->p++;
    return 1;
}

static int jpeek(Json *j, char c)
{
    jskip(j);
    return j->p < j->end && *j->p == c;
}

static int jstring(Json *j, char *buf, int cap)
{
    jskip(j);
    if (j->p >= j->end || *j->p != '"')
        return 0;
    j->p++;
    int n = 0;
    while (j->p < j->end && *j->p != '"') {
        if (n + 1 >= cap)
            return 0;
        buf[n++] = *j->p++;
    }
    if (j->p >= j->end)
        return 0;
    j->p++;
    buf[n] = 0;
    return 1;
}

static int jint(Json *j, int *out)
{
    jskip(j);
    if (j->p >= j->end)
        return 0;
    char *end = NULL;
    long v = strtol(j->p, &end, 10);
    if (end == j->p)
        return 0;
    j->p = end;
    *out = (int)v;
    return 1;
}

static int parse_tile_obj(Json *j, HhmsMap *m)
{
    if (!jneed(j, '{'))
        return 0;
    int x = 0, y = 0, n = 0, have_x = 0, have_y = 0, have_kind = 0;
    char kind[16] = {0};
    while (!jpeek(j, '}')) {
        char key[16];
        if (!jstring(j, key, sizeof(key)) || !jneed(j, ':'))
            return 0;
        if (strcmp(key, "x") == 0) {
            if (!jint(j, &x)) return 0;
            have_x = 1;
        } else if (strcmp(key, "y") == 0) {
            if (!jint(j, &y)) return 0;
            have_y = 1;
        } else if (strcmp(key, "n") == 0) {
            if (!jint(j, &n)) return 0;
        } else if (strcmp(key, "kind") == 0) {
            if (!jstring(j, kind, sizeof(kind))) return 0;
            have_kind = 1;
        } else {
            return 0;
        }
        if (jpeek(j, ','))
            j->p++;
        else
            break;
    }
    if (!jneed(j, '}') || !have_x || !have_y || !have_kind)
        return 0;
    if (strcmp(kind, "clear") == 0)
        return hhms_set_tile(m, x, y, HHMS_CLEAR, n, 0) == 0;
    if (strcmp(kind, "mine") == 0)
        return hhms_set_tile(m, x, y, HHMS_MINE, 0, 0) == 0;
    if (strcmp(kind, "open") == 0)
        return hhms_set_tile(m, x, y, HHMS_OPEN, 0, 0) == 0;
    return 0;
}

static int parse_sup_obj(Json *j, HhmsMap *m)
{
    if (!jneed(j, '{'))
        return 0;
    int x = 0, y = 0, have_x = 0, have_y = 0;
    char kind[16] = "wood";
    while (!jpeek(j, '}')) {
        char key[16];
        if (!jstring(j, key, sizeof(key)) || !jneed(j, ':'))
            return 0;
        if (strcmp(key, "x") == 0) {
            if (!jint(j, &x)) return 0;
            have_x = 1;
        } else if (strcmp(key, "y") == 0) {
            if (!jint(j, &y)) return 0;
            have_y = 1;
        } else if (strcmp(key, "kind") == 0) {
            if (!jstring(j, kind, sizeof(kind))) return 0;
        } else {
            return 0;
        }
        if (jpeek(j, ','))
            j->p++;
        else
            break;
    }
    if (!jneed(j, '}') || !have_x || !have_y)
        return 0;
    return hhms_add_support(m, x, y, support_from_name(kind), 0) == 0;
}

int hhms_save(const HhmsMap *m, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f)
        return -1;
    fputs("{\n  \"version\": 1,\n  \"tiles\": [\n", f);
    int first = 1;
    for (int i = 0; i < m->ntiles; i++) {
        const HhmsTile *t = &m->tiles[i];
        if (!t->user)
            continue;
        if (t->kind == HHMS_UNKNOWN)
            continue;
        if (!first)
            fputs(",\n", f);
        first = 0;
        if (t->kind == HHMS_CLEAR)
            fprintf(f, "    {\"x\": %d, \"y\": %d, \"kind\": \"clear\", \"n\": %d}", t->x, t->y, t->count);
        else if (t->kind == HHMS_OPEN)
            fprintf(f, "    {\"x\": %d, \"y\": %d, \"kind\": \"open\"}", t->x, t->y);
        else
            fprintf(f, "    {\"x\": %d, \"y\": %d, \"kind\": \"mine\"}", t->x, t->y);
    }
    fputs("\n  ],\n  \"supports\": [\n", f);
    for (int i = 0; i < m->nsupports; i++) {
        const HhmsSupport *s = &m->supports[i];
        if (i)
            fputs(",\n", f);
        fprintf(f, "    {\"x\": %d, \"y\": %d, \"kind\": \"%s\"}", s->x, s->y, hhms_support_name(s->kind));
    }
    fputs("\n  ]\n}\n", f);
    fclose(f);
    return 0;
}

int hhms_load(HhmsMap *m, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return -1;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    long sz = ftell(f);
    if (sz < 0 || sz > 8 * 1024 * 1024) {
        fclose(f);
        return -1;
    }
    rewind(f);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return -1;
    }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = 0;

    HhmsMap *tmp = (HhmsMap *)malloc(sizeof(*tmp));
    if (!tmp) {
        free(buf);
        return -1;
    }
    hhms_init(tmp);
    Json j = {buf, buf + n};
    int ok = 0;
    if (jneed(&j, '{')) {
        ok = 1;
        while (!jpeek(&j, '}')) {
            char key[32];
            if (!jstring(&j, key, sizeof(key)) || !jneed(&j, ':')) {
                ok = 0;
                break;
            }
            if (strcmp(key, "version") == 0) {
                int v;
                if (!jint(&j, &v)) { ok = 0; break; }
            } else if (strcmp(key, "tiles") == 0) {
                if (!jneed(&j, '[')) { ok = 0; break; }
                while (!jpeek(&j, ']')) {
                    if (!parse_tile_obj(&j, tmp)) { ok = 0; break; }
                    if (jpeek(&j, ','))
                        j.p++;
                    else
                        break;
                }
                if (!ok || !jneed(&j, ']')) { ok = 0; break; }
            } else if (strcmp(key, "supports") == 0) {
                if (!jneed(&j, '[')) { ok = 0; break; }
                while (!jpeek(&j, ']')) {
                    if (!parse_sup_obj(&j, tmp)) { ok = 0; break; }
                    if (jpeek(&j, ','))
                        j.p++;
                    else
                        break;
                }
                if (!ok || !jneed(&j, ']')) { ok = 0; break; }
            } else {
                ok = 0;
                break;
            }
            if (jpeek(&j, ','))
                j.p++;
            else
                break;
        }
        if (ok && !jneed(&j, '}'))
            ok = 0;
    }
    free(buf);
    if (!ok) {
        free(tmp);
        return -1;
    }
    *m = *tmp;
    free(tmp);
    rebuild_hash(m);
    return 0;
}



void hhms_drop_ephemeral(HhmsMap *m)
{
    int w = 0;
    for (int i = 0; i < m->ntiles; i++) {
        HhmsTile *t = &m->tiles[i];
        if (t->user && t->kind != HHMS_UNKNOWN) {
            t->mark = HHMS_MARK_NONE;
            t->p_mine = -1.f;
            m->tiles[w++] = *t;
        }
    }
    m->ntiles = w;
    rebuild_hash(m);
}

HhmsTile *hhms_touch(HhmsMap *m, int x, int y)
{
    return touch(m, x, y);
}
