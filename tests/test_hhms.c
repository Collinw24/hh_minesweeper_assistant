#include "hhms.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

static int g_fail = 0;
static int g_pass = 0;

static void expect(int cond, const char *name)
{
    if (cond) {
        g_pass++;
        return;
    }
    g_fail++;
    fprintf(stderr, "FAIL %s\n", name);
}

static HhmsMark mark_at(const HhmsMap *m, int x, int y)
{
    const HhmsTile *t = hhms_get(m, x, y);
    return t ? t->mark : HHMS_MARK_NONE;
}

static float p_at(const HhmsMap *m, int x, int y)
{
    const HhmsTile *t = hhms_get(m, x, y);
    return t ? t->p_mine : -1.f;
}

static void test_zero_clears_neighbors(void)
{
    static HhmsMap m;
    hhms_init(&m);
    hhms_set_tile(&m, 0, 0, HHMS_CLEAR, 0, 0);
    hhms_solve(&m);
    expect(!m.contradiction, "zero: no contradiction");
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0)
                continue;
            expect(mark_at(&m, dx, dy) == HHMS_MARK_SAFE, "zero: neighbor safe");
        }
    }
}

static void test_open_is_not_zero(void)
{
    static HhmsMap m;
    hhms_init(&m);
    hhms_set_tile(&m, 0, 0, HHMS_OPEN, 0, 0);
    hhms_solve(&m);

    const HhmsTile *open = hhms_get(&m, 0, 0);
    expect(open && open->kind == HHMS_OPEN && open->user, "open floor preserved");
    expect(open && open->p_mine == 0.f, "open floor is safe");
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0)
                continue;
            expect(mark_at(&m, dx, dy) == HHMS_MARK_NONE, "open: neighbor unmarked");
        }
    }
    hhms_set_tile(&m, 1, 5, HHMS_CLEAR, 0, 0);
    hhms_solve(&m);
    for (int dy = 4; dy <= 6; dy++) {
        for (int dx = 0; dx <= 2; dx++) {
            if (dx == 1 && dy == 5)
                continue;
            expect(mark_at(&m, dx, dy) == HHMS_MARK_SAFE, "zero: distant neighbor safe");
        }
    }
}

static void test_full_ring_all_mines(void)
{
    static HhmsMap m;
    hhms_init(&m);
    hhms_set_tile(&m, 0, 0, HHMS_CLEAR, 8, 0);
    hhms_solve(&m);
    expect(!m.contradiction, "eight: no contradiction");
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0)
                continue;
            expect(mark_at(&m, dx, dy) == HHMS_MARK_MINE, "eight: neighbor mine");
        }
    }
}

static void test_flagged_ring_last_cell_safe(void)
{
    static HhmsMap m;
    hhms_init(&m);
    hhms_set_tile(&m, 0, 0, HHMS_CLEAR, 7, 0);
    int n = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0)
                continue;
            if (n++ < 7)
                hhms_set_tile(&m, dx, dy, HHMS_MINE, 0, 0);
        }
    }
    hhms_solve(&m);
    expect(mark_at(&m, 1, 1) == HHMS_MARK_SAFE, "seventh mine spends the count");
}

static void test_flag_reduces_remaining(void)
{
    static HhmsMap m;
    hhms_init(&m);
    hhms_set_tile(&m, 0, 0, HHMS_CLEAR, 1, 0);
    hhms_set_tile(&m, -1, 0, HHMS_MINE, 0, 0);
    hhms_solve(&m);
    expect(mark_at(&m, 1, 0) == HHMS_MARK_SAFE, "flag spends the one");
    expect(mark_at(&m, 0, 1) == HHMS_MARK_SAFE, "flag spends the one (other)");
}

static void test_contradiction_zero_next_to_mine(void)
{
    static HhmsMap m;
    hhms_init(&m);
    hhms_set_tile(&m, 0, 0, HHMS_CLEAR, 0, 0);
    hhms_set_tile(&m, 1, 0, HHMS_MINE, 0, 0);
    hhms_solve(&m);
    expect(m.contradiction, "0 beside mine is contradiction");
}

static void test_pattern_121(void)
{
    static HhmsMap m;
    hhms_init(&m);
    /* Isolated 1-2-1. Other neighbors are known mines; counts include them. */
    hhms_set_tile(&m, 0, 1, HHMS_CLEAR, 6, 0);
    hhms_set_tile(&m, 1, 1, HHMS_CLEAR, 5, 0);
    hhms_set_tile(&m, 2, 1, HHMS_CLEAR, 6, 0);
    hhms_set_tile(&m, -1, 0, HHMS_MINE, 0, 0);
    hhms_set_tile(&m, -1, 1, HHMS_MINE, 0, 0);
    hhms_set_tile(&m, -1, 2, HHMS_MINE, 0, 0);
    hhms_set_tile(&m, 0, 2, HHMS_MINE, 0, 0);
    hhms_set_tile(&m, 1, 2, HHMS_MINE, 0, 0);
    hhms_set_tile(&m, 2, 2, HHMS_MINE, 0, 0);
    hhms_set_tile(&m, 3, 0, HHMS_MINE, 0, 0);
    hhms_set_tile(&m, 3, 1, HHMS_MINE, 0, 0);
    hhms_set_tile(&m, 3, 2, HHMS_MINE, 0, 0);
    hhms_solve(&m);
    expect(mark_at(&m, 0, 0) == HHMS_MARK_MINE, "121: left mine");
    expect(mark_at(&m, 1, 0) == HHMS_MARK_SAFE, "121: mid safe");
    expect(mark_at(&m, 2, 0) == HHMS_MARK_MINE, "121: right mine");
}

static void test_isolated_one_has_odds(void)
{
    static HhmsMap m;
    hhms_init(&m);
    hhms_set_tile(&m, 0, 0, HHMS_CLEAR, 1, 0);
    hhms_solve(&m);
    expect(mark_at(&m, 1, 0) == HHMS_MARK_NONE, "isolated 1: no certainty");
    float p = p_at(&m, 1, 0);
    expect(p > 0.12f && p < 0.13f, "isolated 1: each neighbor 1/8");
}

static void test_support_radius(void)
{
    static HhmsMap m;
    hhms_init(&m);
    hhms_add_support(&m, 0, 0, HHMS_SUP_WOOD, 0);
    expect(hhms_covered(&m, 0, 0), "wood covers self");
    expect(hhms_covered(&m, 9, 0), "wood covers (9,0)");
    expect(hhms_covered(&m, 9, 1), "wood covers (9,1)");
    expect(!hhms_covered(&m, 9, 2), "wood misses (9,2)");
    expect(!hhms_covered(&m, 10, 0), "wood misses (10,0)");

    hhms_init(&m);
    hhms_add_support(&m, 0, 0, HHMS_SUP_STONE, 0);
    expect(hhms_covered(&m, 11, 0), "stone covers (11,0)");
    expect(!hhms_covered(&m, 11, 3), "stone misses (11,3)");

    hhms_init(&m);
    hhms_add_support(&m, 0, 0, HHMS_SUP_MONUMENT, 0);
    expect(hhms_covered(&m, 30, 0), "monument covers (30,0)");
    expect(hhms_covered(&m, 18, 24), "monument covers (18,24)");
    expect(!hhms_covered(&m, 21, 22), "monument misses (21,22)");
    hhms_init(&m);
    hhms_add_support(&m, 0, 0, HHMS_SUP_BEAM, 0);
    expect(hhms_covered(&m, 13, 0), "beam covers (13,0)");
    expect(hhms_covered(&m, 13, 4), "beam covers (13,4)");
    expect(!hhms_covered(&m, 13, 5), "beam misses (13,5)");
    expect(!hhms_covered(&m, 14, 0), "beam misses (14,0)");
}

static void test_undo_erase_and_support_del(void)
{
    static HhmsMap m;
    hhms_init(&m);
    hhms_set_tile(&m, 4, 4, HHMS_CLEAR, 3, 1);
    expect(hhms_erase_tile(&m, 4, 4, 1) == 0, "erase tile ok");
    expect(hhms_get(&m, 4, 4) == NULL, "tile is erased");
    expect(hhms_undo(&m), "undo erase");
    const HhmsTile *t = hhms_get(&m, 4, 4);
    expect(t && t->kind == HHMS_CLEAR && t->count == 3 && t->user, "tile restored");

    hhms_add_support(&m, 2, 2, HHMS_SUP_WOOD, 0);
    expect(hhms_del_support_at(&m, 2, 2, 1) == 0, "del support ok");
    expect(hhms_support_index(&m, 2, 2) < 0, "support removed");
    expect(hhms_undo(&m), "undo del support");
    int si = hhms_support_index(&m, 2, 2);
    expect(si >= 0 && m.supports[si].kind == HHMS_SUP_WOOD, "support restored");
}

static void test_contradiction_conflicting_clues(void)
{
    static HhmsMap m;
    hhms_init(&m);
    /* (0,0) requires 0 mines around it, but (0,2) with all other neighbors mined requires (0,1) to be mine */
    hhms_set_tile(&m, 0, 0, HHMS_CLEAR, 0, 0);
    hhms_set_tile(&m, 0, 2, HHMS_CLEAR, 8, 0);
    for (int dy = 1; dy <= 3; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 2)
                continue;
            if (dx == 0 && dy == 1)
                continue; /* (0,1) is unflagged shared neighbor */
            hhms_set_tile(&m, dx, dy, HHMS_MINE, 0, 0);
        }
    }
    hhms_solve(&m);
    expect(m.contradiction, "conflicting safe/mine clue detected");
}

static void test_save_load_empty_and_corrupt(void)
{
#ifdef _WIN32
    char path[L_tmpnam];
    if (!tmpnam(path)) {
        expect(0, "tmpnam");
        return;
    }
#else
    char path[] = "/tmp/hhms_empty_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) {
        expect(0, "mkstemp");
        return;
    }
    close(fd);
#endif
    static HhmsMap a, b;
    hhms_init(&a);
    expect(hhms_save(&a, path) == 0, "save empty ok");
    expect(hhms_load(&b, path) == 0, "load empty ok");
    expect(b.ntiles == 0, "empty tiles");
    expect(b.nsupports == 0, "empty supports");

    /* Test corrupted JSON rejection */
    FILE *f = fopen(path, "w");
    if (f) {
        fputs("{ invalid json syntax ...", f);
        fclose(f);
        static HhmsMap c;
        hhms_init(&c);
        hhms_set_tile(&c, 1, 1, HHMS_CLEAR, 2, 0);
        expect(hhms_load(&c, path) == -1, "corrupted json rejected");
        expect(hhms_get(&c, 1, 1) != NULL, "map untouched on parse failure");
    }
    remove(path);
}

static void test_solver_idempotence(void)
{
    static HhmsMap m;
    hhms_init(&m);
    hhms_set_tile(&m, 0, 0, HHMS_CLEAR, 1, 0);
    hhms_solve(&m);
    int nt1 = m.ntiles;
    float p1 = p_at(&m, 1, 0);
    hhms_solve(&m);
    int nt2 = m.ntiles;
    float p2 = p_at(&m, 1, 0);
    expect(nt1 == nt2, "solve idempotence tile count");
    expect(p1 == p2, "solve idempotence probability");
}

static void test_save_load_roundtrip(void)
{
#ifdef _WIN32
    char path[L_tmpnam];
    if (!tmpnam(path)) {
        expect(0, "tmpnam");
        return;
    }
#else
    char path[] = "/tmp/hhms_test_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) {
        expect(0, "mkstemp");
        return;
    }
    close(fd);
#endif
    static HhmsMap a, b;
    hhms_init(&a);
    hhms_set_tile(&a, 2, -3, HHMS_CLEAR, 4, 0);
    hhms_set_tile(&a, 5, 5, HHMS_MINE, 0, 0);
    hhms_set_tile(&a, -4, 7, HHMS_OPEN, 0, 0);
    hhms_add_support(&a, 0, 0, HHMS_SUP_BEAM, 0);
    expect(hhms_save(&a, path) == 0, "save ok");
    expect(hhms_load(&b, path) == 0, "load ok");
    const HhmsTile *t = hhms_get(&b, 2, -3);
    expect(t && t->kind == HHMS_CLEAR && t->count == 4, "loaded clear");
    t = hhms_get(&b, 5, 5);
    expect(t && t->kind == HHMS_MINE, "loaded mine");
    t = hhms_get(&b, -4, 7);
    expect(t && t->kind == HHMS_OPEN && t->user, "loaded open");
    expect(hhms_support_index(&b, 0, 0) >= 0, "loaded support");
    expect(b.supports[hhms_support_index(&b, 0, 0)].kind == HHMS_SUP_BEAM, "loaded beam");
    remove(path);
}

static void test_undo_tile_and_support(void)
{
    static HhmsMap m;
    hhms_init(&m);
    hhms_set_tile(&m, 1, 1, HHMS_CLEAR, 2, 1);
    hhms_set_tile(&m, 1, 1, HHMS_MINE, 0, 1);
    expect(hhms_undo(&m), "undo mine");
    const HhmsTile *t = hhms_get(&m, 1, 1);
    expect(t && t->kind == HHMS_CLEAR && t->count == 2, "undo restored clear");
    expect(hhms_undo(&m), "undo clear");
    expect(hhms_get(&m, 1, 1) == NULL, "undo removed tile");

    hhms_add_support(&m, 0, 0, HHMS_SUP_WOOD, 1);
    hhms_add_support(&m, 0, 0, HHMS_SUP_STONE, 1);
    expect(m.supports[0].kind == HHMS_SUP_STONE, "replaced with stone");
    hhms_undo(&m);
    hhms_undo(&m);
    expect(m.supports[0].kind == HHMS_SUP_WOOD, "undo replace");
    hhms_undo(&m);
    expect(m.nsupports == 0, "undo add");
}

static void test_overlap_forces_shared(void)
{
    static HhmsMap m;
    hhms_init(&m);
    /* 1 sees A,B; 2 sees A,B,C after mining every other neighbor. */
    hhms_set_tile(&m, 0, 1, HHMS_CLEAR, 6, 0);
    hhms_set_tile(&m, 1, 1, HHMS_CLEAR, 6, 0);
    hhms_set_tile(&m, -1, 0, HHMS_MINE, 0, 0);
    hhms_set_tile(&m, -1, 1, HHMS_MINE, 0, 0);
    hhms_set_tile(&m, -1, 2, HHMS_MINE, 0, 0);
    hhms_set_tile(&m, 0, 2, HHMS_MINE, 0, 0);
    hhms_set_tile(&m, 1, 2, HHMS_MINE, 0, 0);
    hhms_set_tile(&m, 2, 2, HHMS_MINE, 0, 0);
    hhms_set_tile(&m, 2, 1, HHMS_MINE, 0, 0);
    hhms_solve(&m);
    expect(mark_at(&m, 2, 0) == HHMS_MARK_MINE, "subset: extra cell is mine");
}

/* Screenshot board (8-neighborhood):
 *   25  G  G  G  G
 *   25  1  1  1  G
 *   25  1  C  R  G
 *   25  1  1  1  G
 *   25  G  0  G  G
 * C and R both touch the right-hand 1s, so exactly one of them is a mine.
 * The left wall column adds extra valid layouts. C is not forced.
 */
static void test_screenshot_ring_not_forced(void)
{
    static HhmsMap m;
    hhms_init(&m);
    hhms_set_tile(&m, 1, 1, HHMS_CLEAR, 1, 0);
    hhms_set_tile(&m, 2, 1, HHMS_CLEAR, 1, 0);
    hhms_set_tile(&m, 3, 1, HHMS_CLEAR, 1, 0);
    hhms_set_tile(&m, 1, 2, HHMS_CLEAR, 1, 0);
    hhms_set_tile(&m, 1, 3, HHMS_CLEAR, 1, 0);
    hhms_set_tile(&m, 2, 3, HHMS_CLEAR, 1, 0);
    hhms_set_tile(&m, 3, 3, HHMS_CLEAR, 1, 0);
    hhms_set_tile(&m, 2, 4, HHMS_CLEAR, 0, 0);
    hhms_solve(&m);
    expect(mark_at(&m, 2, 2) != HHMS_MARK_MINE, "center C is not forced");
    expect(mark_at(&m, 3, 2) != HHMS_MARK_MINE, "right R is not forced");
    float pc = p_at(&m, 2, 2);
    float pr = p_at(&m, 3, 2);
    expect(pc > 0.f && pc < 1.f, "center has leftover odds");
    expect(pr > pc, "R more likely than C");
}


int main(void)
{
    test_zero_clears_neighbors();
    test_open_is_not_zero();
    test_full_ring_all_mines();
    test_flagged_ring_last_cell_safe();
    test_flag_reduces_remaining();
    test_contradiction_zero_next_to_mine();
    test_pattern_121();
    test_isolated_one_has_odds();
    test_support_radius();
    test_save_load_roundtrip();
    test_undo_tile_and_support();
    test_undo_erase_and_support_del();
    test_contradiction_conflicting_clues();
    test_save_load_empty_and_corrupt();
    test_solver_idempotence();
    test_overlap_forces_shared();
    test_screenshot_ring_not_forced();

    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
