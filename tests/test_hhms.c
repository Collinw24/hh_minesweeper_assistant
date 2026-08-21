#define _XOPEN_SOURCE 700

#include "hhms.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define rmdir _rmdir
#define chmod _chmod
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

static int g_fail;
static int g_pass;

static void expect(int condition, const char *behavior)
{
    if (condition) {
        g_pass++;
    } else {
        g_fail++;
        fprintf(stderr, "FAIL %s\n", behavior);
    }
}

static HhmsMark mark_at(const HhmsAnalysis *analysis, int x, int y)
{
    const HhmsAnalysisCell *cell = hhms_analysis_get(analysis, x, y);
    return cell ? cell->mark : HHMS_MARK_NONE;
}

static uint64_t mines_at(const HhmsAnalysis *analysis, int x, int y)
{
    const HhmsAnalysisCell *cell = hhms_analysis_get(analysis, x, y);
    return cell ? cell->mine_models : 0;
}

static uint64_t models_at(const HhmsAnalysis *analysis, int x, int y)
{
    const HhmsAnalysisCell *cell = hhms_analysis_get(analysis, x, y);
    return cell ? cell->total_models : 0;
}

static void solve(const HhmsMap *map, HhmsAnalysis *analysis)
{
    hhms_analysis_init(analysis);
    hhms_solve(map, analysis);
}

static void test_zero_open_and_full_ring(void)
{
    static HhmsMap map;
    static HhmsAnalysis analysis;
    hhms_init(&map);
    expect(hhms_set_tile(&map, 0, 0, HHMS_CLEAR, 0) == HHMS_EDIT_OK,
           "zero clue setup is accepted");
    solve(&map, &analysis);
    expect(!analysis.contradiction, "zero clue is consistent");
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            if (x || y)
                expect(mark_at(&analysis, x, y) == HHMS_MARK_SAFE,
                       "zero clue proves every neighboring wall safe");
        }
    }

    hhms_init(&map);
    expect(hhms_set_tile(&map, 0, 0, HHMS_OPEN, 0) == HHMS_EDIT_OK,
           "open floor setup is accepted");
    solve(&map, &analysis);
    const HhmsTile *open = hhms_get(&map, 0, 0);
    expect(open && open->kind == HHMS_OPEN, "open floor remains authored open floor");
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            if (x || y)
                expect(mark_at(&analysis, x, y) == HHMS_MARK_NONE,
                       "open floor does not act as a zero clue");
        }
    }

    hhms_init(&map);
    hhms_set_tile(&map, 0, 0, HHMS_CLEAR, 8);
    solve(&map, &analysis);
    expect(!analysis.contradiction, "full ring clue is consistent");
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            if (x || y)
                expect(mark_at(&analysis, x, y) == HHMS_MARK_MINE,
                       "full ring clue proves every neighboring wall a cave");
        }
    }
}

static void test_flags_and_contradictions(void)
{
    static HhmsMap map;
    static HhmsAnalysis analysis;
    hhms_init(&map);
    hhms_set_tile(&map, 0, 0, HHMS_CLEAR, 7);
    int authored = 0;
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            if ((x || y) && authored++ < 7)
                hhms_set_tile(&map, x, y, HHMS_MINE, 0);
        }
    }
    solve(&map, &analysis);
    expect(mark_at(&analysis, 1, 1) == HHMS_MARK_SAFE,
           "seven authored flags spend a count of seven and prove the last wall safe");

    hhms_init(&map);
    hhms_set_tile(&map, 0, 0, HHMS_CLEAR, 1);
    hhms_set_tile(&map, -1, 0, HHMS_MINE, 0);
    solve(&map, &analysis);
    expect(mark_at(&analysis, 1, 0) == HHMS_MARK_SAFE,
           "an authored flag reduces the clue's remaining mine count");
    expect(mark_at(&analysis, 0, 1) == HHMS_MARK_SAFE,
           "an authored flag proves every other neighbor safe");

    hhms_init(&map);
    hhms_set_tile(&map, 0, 0, HHMS_CLEAR, 0);
    hhms_set_tile(&map, 1, 0, HHMS_MINE, 0);
    solve(&map, &analysis);
    expect(analysis.contradiction, "a flagged cave beside a zero clue is a contradiction");
    expect(mark_at(&analysis, 0, 0) == HHMS_MARK_CONFLICT,
           "the impossible zero clue is identified as a conflict");
}

static void add_121_fixture(HhmsMap *map)
{
    hhms_set_tile(map, 0, 1, HHMS_CLEAR, 6);
    hhms_set_tile(map, 1, 1, HHMS_CLEAR, 5);
    hhms_set_tile(map, 2, 1, HHMS_CLEAR, 6);
    static const int mines[][2] = {
        {-1, 0}, {-1, 1}, {-1, 2}, {0, 2}, {1, 2},
        {2, 2}, {3, 0}, {3, 1}, {3, 2}
    };
    for (size_t i = 0; i < sizeof(mines) / sizeof(mines[0]); i++)
        hhms_set_tile(map, mines[i][0], mines[i][1], HHMS_MINE, 0);
}

static void test_121_overlap_and_screenshot_ring(void)
{
    static HhmsMap map;
    static HhmsAnalysis analysis;
    hhms_init(&map);
    add_121_fixture(&map);
    solve(&map, &analysis);
    expect(mark_at(&analysis, 0, 0) == HHMS_MARK_MINE,
           "isolated 1-2-1 proves its left cave");
    expect(mark_at(&analysis, 1, 0) == HHMS_MARK_SAFE,
           "isolated 1-2-1 proves its middle safe wall");
    expect(mark_at(&analysis, 2, 0) == HHMS_MARK_MINE,
           "isolated 1-2-1 proves its right cave");

    hhms_init(&map);
    hhms_set_tile(&map, 0, 1, HHMS_CLEAR, 6);
    hhms_set_tile(&map, 1, 1, HHMS_CLEAR, 6);
    static const int overlap_mines[][2] = {
        {-1, 0}, {-1, 1}, {-1, 2}, {0, 2}, {1, 2}, {2, 2}, {2, 1}
    };
    for (size_t i = 0; i < sizeof(overlap_mines) / sizeof(overlap_mines[0]); i++)
        hhms_set_tile(&map, overlap_mines[i][0], overlap_mines[i][1], HHMS_MINE, 0);
    solve(&map, &analysis);
    expect(mark_at(&analysis, 2, 0) == HHMS_MARK_MINE,
           "overlapping clue subset proves the extra wall a cave");
    const HhmsAnalysisCell *subset = hhms_analysis_get(&analysis, 2, 0);
    expect(subset && subset->reason == HHMS_REASON_SUBSET,
           "overlap deduction records subset provenance");

    hhms_init(&map);
    static const int ring_clues[][3] = {
        {1, 1, 1}, {2, 1, 1}, {3, 1, 1}, {1, 2, 1},
        {1, 3, 1}, {2, 3, 1}, {3, 3, 1}, {2, 4, 0}
    };
    for (size_t i = 0; i < sizeof(ring_clues) / sizeof(ring_clues[0]); i++)
        hhms_set_tile(&map, ring_clues[i][0], ring_clues[i][1], HHMS_CLEAR,
                      ring_clues[i][2]);
    solve(&map, &analysis);
    expect(mark_at(&analysis, 2, 2) == HHMS_MARK_NONE,
           "known screenshot ring does not falsely force its center cave");
    expect(mark_at(&analysis, 3, 2) == HHMS_MARK_NONE,
           "known screenshot ring does not falsely force its right cave");
    expect(models_at(&analysis, 2, 2) > mines_at(&analysis, 2, 2) &&
           mines_at(&analysis, 2, 2) > 0,
           "known screenshot center retains nontrivial legal-layout odds");
    expect(models_at(&analysis, 3, 2) > mines_at(&analysis, 3, 2) &&
           mines_at(&analysis, 3, 2) > mines_at(&analysis, 2, 2),
           "known screenshot right wall is more likely than its center");
}

static void test_solver_idempotence(void)
{
    static HhmsMap map;
    static HhmsAnalysis first;
    static HhmsAnalysis second;
    hhms_init(&map);
    hhms_set_tile(&map, 0, 0, HHMS_CLEAR, 1);
    solve(&map, &first);
    int authored_tiles = map.ntiles;
    solve(&map, &second);
    expect(map.ntiles == authored_tiles,
           "repeated solving never materializes derived cells into authored map state");
    expect(memcmp(&first, &second, sizeof(first)) == 0,
           "repeated solving produces byte-identical derived analysis");
    expect(mines_at(&second, 1, 0) == 1 && models_at(&second, 1, 0) == 8,
           "isolated one reports exact one-of-eight odds");
}

typedef struct {
    int x;
    int y;
} OracleCell;

typedef struct {
    uint32_t mask;
    int need;
} OracleConstraint;

typedef struct {
    OracleCell cells[20];
    OracleConstraint constraints[64];
    int ncells;
    int nconstraints;
    uint64_t total;
    uint64_t mines[20];
} Oracle;

static int oracle_cell(Oracle *oracle, int x, int y)
{
    for (int i = 0; i < oracle->ncells; i++) {
        if (oracle->cells[i].x == x && oracle->cells[i].y == y)
            return i;
    }
    if (oracle->ncells >= 20)
        return -1;
    oracle->cells[oracle->ncells].x = x;
    oracle->cells[oracle->ncells].y = y;
    return oracle->ncells++;
}

static int authored_mine(const HhmsMap *map, int x, int y)
{
    const HhmsTile *tile = hhms_get(map, x, y);
    return tile && tile->kind == HHMS_MINE;
}

static int authored_safe(const HhmsMap *map, int x, int y)
{
    const HhmsTile *tile = hhms_get(map, x, y);
    return tile && (tile->kind == HHMS_CLEAR || tile->kind == HHMS_OPEN);
}

static void oracle_solve(const HhmsMap *map, Oracle *oracle)
{
    static const int dx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static const int dy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    memset(oracle, 0, sizeof(*oracle));
    for (int i = 0; i < map->ntiles; i++) {
        const HhmsTile *clue = &map->tiles[i];
        if (clue->kind != HHMS_CLEAR)
            continue;
        OracleConstraint *constraint = &oracle->constraints[oracle->nconstraints++];
        constraint->need = clue->count;
        for (int k = 0; k < 8; k++) {
            int x = clue->x + dx[k];
            int y = clue->y + dy[k];
            if (authored_mine(map, x, y)) {
                constraint->need--;
            } else if (!authored_safe(map, x, y)) {
                int cell = oracle_cell(oracle, x, y);
                if (cell >= 0)
                    constraint->mask |= (uint32_t)1u << cell;
            }
        }
    }
    if (oracle->ncells > 20 || oracle->nconstraints > 64)
        return;
    uint64_t assignments = (uint64_t)1u << oracle->ncells;
    for (uint64_t bits = 0; bits < assignments; bits++) {
        int valid = 1;
        for (int c = 0; c < oracle->nconstraints; c++) {
            int have = 0;
            uint32_t selected = (uint32_t)bits & oracle->constraints[c].mask;
            while (selected) {
                have += (int)(selected & 1u);
                selected >>= 1;
            }
            if (have != oracle->constraints[c].need) {
                valid = 0;
                break;
            }
        }
        if (!valid)
            continue;
        oracle->total++;
        for (int cell = 0; cell < oracle->ncells; cell++) {
            if (bits & ((uint64_t)1u << cell))
                oracle->mines[cell]++;
        }
    }
}

static uint32_t next_generated(uint32_t *state)
{
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state;
}

static void build_generated_frontier(HhmsMap *map, int seed)
{
    int n = 3 + seed % 6;
    uint32_t state = UINT32_C(0x9e3779b9) ^ (uint32_t)seed;
    uint32_t truth = next_generated(&state);
    for (int x = 0; x < n; x++) {
        int count = 0;
        for (int candidate = x - 1; candidate <= x + 1; candidate++) {
            if (candidate >= 0 && candidate < n && (truth & ((uint32_t)1u << candidate)))
                count++;
        }
        if (seed % 13 == 0 && x == n / 2)
            count = 8;
        hhms_set_tile(map, x, 1, HHMS_CLEAR, count);
    }
    for (int x = 0; x < n; x++) {
        for (int y = 0; y <= 2; y++) {
            for (int nx = x - 1; nx <= x + 1; nx++) {
                if (y == 0 && nx >= 0 && nx < n)
                    continue;
                const HhmsTile *tile = hhms_get(map, nx, y);
                if (!tile)
                    hhms_set_tile(map, nx, y, HHMS_OPEN, 0);
            }
        }
    }
}

static void test_independent_bruteforce_oracle(void)
{
    static HhmsMap map;
    static HhmsAnalysis analysis;
    Oracle oracle;
    char behavior[160];
    for (int seed = 0; seed < 96; seed++) {
        hhms_init(&map);
        build_generated_frontier(&map, seed);
        oracle_solve(&map, &oracle);
        solve(&map, &analysis);
        snprintf(behavior, sizeof(behavior),
                 "generated frontier %d contradiction matches independent enumeration", seed);
        expect(analysis.contradiction == (oracle.total == 0), behavior);
        if (oracle.total == 0)
            continue;
        for (int i = 0; i < oracle.ncells; i++) {
            HhmsMark expected_mark = HHMS_MARK_NONE;
            if (oracle.mines[i] == 0)
                expected_mark = HHMS_MARK_SAFE;
            else if (oracle.mines[i] == oracle.total)
                expected_mark = HHMS_MARK_MINE;
            snprintf(behavior, sizeof(behavior),
                     "generated frontier %d cell %d SAFE/MINE proof matches independent enumeration",
                     seed, i);
            expect(mark_at(&analysis, oracle.cells[i].x, oracle.cells[i].y) == expected_mark,
                   behavior);
            snprintf(behavior, sizeof(behavior),
                     "generated frontier %d cell %d exact mine model count matches independent enumeration",
                     seed, i);
            expect(mines_at(&analysis, oracle.cells[i].x, oracle.cells[i].y) == oracle.mines[i],
                   behavior);
            snprintf(behavior, sizeof(behavior),
                     "generated frontier %d cell %d exact total model count matches independent enumeration",
                     seed, i);
            expect(models_at(&analysis, oracle.cells[i].x, oracle.cells[i].y) == oracle.total,
                   behavior);
        }
    }
}

static int split_fixture_candidate(int x, int y)
{
    return (x == 0 && y == 0) ||
           (x == -2 && (y == 0 || y == 2)) ||
           (x == 2 && (y == 0 || y == 2));
}

static int split_fixture_clue(int x, int y)
{
    return (x == -1 && y == 1) ||
           (x == 0 && y == -1) ||
           (x == 1 && y == 1);
}

static void test_original_component_counts_survive_split(void)
{
    static HhmsMap map;
    static HhmsAnalysis analysis;
    const int clues[3][3] = {
        {-1, 1, 1},
        {0, -1, 0},
        {1, 1, 1}
    };
    hhms_init(&map);
    for (int i = 0; i < 3; i++)
        hhms_set_tile(&map, clues[i][0], clues[i][1], HHMS_CLEAR, clues[i][2]);
    for (int i = 0; i < 3; i++) {
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0)
                    continue;
                int x = clues[i][0] + dx;
                int y = clues[i][1] + dy;
                if (!split_fixture_candidate(x, y) && !split_fixture_clue(x, y))
                    hhms_set_tile(&map, x, y, HHMS_OPEN, 0);
            }
        }
    }
    solve(&map, &analysis);
    expect(mark_at(&analysis, 0, 0) == HHMS_MARK_SAFE &&
           models_at(&analysis, 0, 0) == 4,
           "forced articulation cell retains the original component's four layouts");
    const int branches[4][2] = {{-2, 0}, {-2, 2}, {2, 0}, {2, 2}};
    for (int i = 0; i < 4; i++) {
        char behavior[128];
        snprintf(behavior, sizeof(behavior),
                 "split branch cell %d retains two mines across four original layouts", i);
        expect(mines_at(&analysis, branches[i][0], branches[i][1]) == 2 &&
               models_at(&analysis, branches[i][0], branches[i][1]) == 4,
               behavior);
    }
}

static void test_fail_closed_and_enumeration_limit(void)
{
    static HhmsMap map;
    static HhmsAnalysis analysis;
    hhms_init(&map);
    hhms_set_tile(&map, 0, 0, HHMS_CLEAR, 0);
    hhms_set_tile(&map, 1, 0, HHMS_MINE, 0);
    hhms_set_tile(&map, 100, 100, HHMS_CLEAR, 1);
    solve(&map, &analysis);
    expect(analysis.contradiction, "one contradictory component makes the global analysis contradictory");
    expect(mark_at(&analysis, 101, 100) == HHMS_MARK_NONE,
           "global contradiction hides a distant component's otherwise valid derived action");
    expect(models_at(&analysis, 101, 100) == 0 && mines_at(&analysis, 101, 100) == 0,
           "global contradiction hides a distant component's otherwise valid odds");
    expect(hhms_cell_info(&map, &analysis, 101, 100).action == HHMS_ACTION_UNKNOWN_ROCK,
           "global contradiction fails closed to an unknown action outside the conflict");
    int all_failed_closed = 1;
    for (int i = 0; i < analysis.ncells; i++) {
        const HhmsAnalysisCell *cell = &analysis.cells[i];
        if (cell->mark != HHMS_MARK_CONFLICT &&
            (cell->mark != HHMS_MARK_NONE || cell->reason != HHMS_REASON_NONE ||
             cell->mine_models != 0 || cell->total_models != 0)) {
            all_failed_closed = 0;
        }
    }
    expect(all_failed_closed,
           "global contradiction hides every non-conflict proof reason and odds result");

    hhms_init(&map);
    for (int x = 0; x < 8; x++)
        hhms_set_tile(&map, x, 0, HHMS_CLEAR, 1);
    hhms_set_tile(&map, -1, -1, HHMS_OPEN, 0);
    solve(&map, &analysis);
    expect(!analysis.contradiction, "twenty-one-cell frontier is not itself contradictory");
    expect(!analysis.complete, "twenty-one-cell frontier explicitly reports incomplete analysis");
    expect((analysis.limits & HHMS_LIMIT_ENUM_COMPONENT) != 0,
           "twenty-one-cell frontier identifies the enumeration component limit");
    expect(models_at(&analysis, 0, -1) == 0 && mines_at(&analysis, 0, -1) == 0,
           "limited frontier does not publish partial odds");
    int no_partial_odds = 1;
    for (int i = 0; i < analysis.ncells; i++) {
        if (analysis.cells[i].mine_models != 0 || analysis.cells[i].total_models != 0)
            no_partial_odds = 0;
    }
    expect(no_partial_odds,
           "incomplete twenty-one-cell component publishes no odds for any frontier cell");
    expect(hhms_cell_info(&map, &analysis, 0, -1).action == HHMS_ACTION_UNKNOWN_ROCK,
           "incomplete frontier exposes no derived DIG or CAVE action");
}
static void test_conflict_location_survives_analysis_cap(void)
{
    static HhmsMap map;
    static HhmsAnalysis analysis;
    hhms_init(&map);
    for (int i = 0; i < 1024; i++)
        hhms_set_tile(&map, i * 4, 0, HHMS_CLEAR, 0);
    hhms_set_tile(&map, 10000, 0, HHMS_CLEAR, 0);
    hhms_set_tile(&map, 10001, 0, HHMS_MINE, 0);
    solve(&map, &analysis);
    const HhmsAnalysisCell *conflict = hhms_analysis_get(&analysis, 10000, 0);
    expect(analysis.contradiction &&
           (analysis.limits & HHMS_LIMIT_ANALYSIS_CELLS) != 0,
           "late contradiction remains fail-closed after the analysis-cell cap fills");
    expect(conflict && conflict->mark == HHMS_MARK_CONFLICT,
           "late contradictory clue remains locatable after non-conflict eviction");
}


static void test_water_and_cell_provenance(void)
{
    static HhmsMap map;
    static HhmsAnalysis analysis;
    hhms_init(&map);
    hhms_set_tile(&map, 0, 0, HHMS_CLEAR, 0);
    hhms_set_terrain(&map, 1, 0, HHMS_TERRAIN_WATER);
    solve(&map, &analysis);
    HhmsCellInfo water = hhms_cell_info(&map, &analysis, 1, 0);
    expect(water.mark == HHMS_MARK_SAFE,
           "water wall participates in a neighboring dust constraint");
    expect(water.action == HHMS_ACTION_SAFE_WATER,
           "proved-safe water is classified as safe water rather than DIG");
    expect(water.authored, "authored water terrain retains authored provenance");

    hhms_init(&map);
    hhms_set_tile(&map, 0, 0, HHMS_CLEAR, 8);
    hhms_set_terrain(&map, 1, 0, HHMS_TERRAIN_WATER);
    solve(&map, &analysis);
    water = hhms_cell_info(&map, &analysis, 1, 0);
    expect(water.mark == HHMS_MARK_MINE && water.action == HHMS_ACTION_CAVE,
           "water remains a cave candidate and a proved water cave is never DIG");
    expect(water.kind == HHMS_UNKNOWN && water.authored,
           "proved water cave remains derived while its terrain remains authored");

    hhms_init(&map);
    hhms_set_tile(&map, 5, 5, HHMS_MINE, 0);
    solve(&map, &analysis);
    HhmsCellInfo flag = hhms_cell_info(&map, &analysis, 5, 5);
    expect(flag.authored && flag.kind == HHMS_MINE && flag.action == HHMS_ACTION_CAVE,
           "authored flag is reported as an authored cave");

    hhms_init(&map);
    hhms_set_tile(&map, 0, 0, HHMS_CLEAR, 8);
    uint32_t support_id = 0;
    hhms_add_support(&map, 1.0, 0.0, HHMS_SUP_WOOD, HHMS_ORIENT_NORTH, &support_id);
    solve(&map, &analysis);
    HhmsCellInfo proved = hhms_cell_info(&map, &analysis, 1, 0);
    expect(!proved.authored && proved.mark == HHMS_MARK_MINE,
           "solver-proved cave has derived rather than authored provenance");
    expect(proved.action == HHMS_ACTION_PROTECTED_CAVE &&
           proved.coverage.primary_confirmed == support_id,
           "confirmed radial coverage classifies a proved cave as protected");
}

static void test_support_geometry_and_exposure(void)
{
    static HhmsMap map;
    static HhmsAnalysis analysis;
    uint32_t id = 0;
    hhms_init(&map);
    expect(hhms_add_support(&map, NAN, 0.0, HHMS_SUP_WOOD,
                            HHMS_ORIENT_NORTH, NULL) == HHMS_EDIT_INVALID,
           "nonfinite support position is rejected by the authored-state API");
    expect(hhms_add_support(&map, INFINITY, 0.0, HHMS_SUP_WOOD,
                            HHMS_ORIENT_NORTH, NULL) == HHMS_EDIT_INVALID,
           "overflowed infinite support position is rejected by the authored-state API");
    hhms_add_support(&map, 0.0, 0.0, HHMS_SUP_WOOD, HHMS_ORIENT_NORTH, &id);
    expect(hhms_support_covers(&map.supports[0], 9, 0, NULL) == HHMS_COVER_CONFIRMED,
           "wood radial support covers the old nine-cell axial boundary");
    expect(hhms_support_covers(&map.supports[0], 9, 1, NULL) == HHMS_COVER_CONFIRMED,
           "wood radial support covers the old nine-by-one boundary");
    expect(hhms_support_covers(&map.supports[0], 9, 2, NULL) == HHMS_COVER_NONE,
           "wood radial support excludes the first wall beyond its diagonal radius");
    expect(hhms_support_covers(&map.supports[0], 10, 0, NULL) == HHMS_COVER_NONE,
           "wood radial support excludes ten axial cells");

    hhms_init(&map);
    hhms_add_support(&map, 0.0, 0.0, HHMS_SUP_STONE, HHMS_ORIENT_NORTH, &id);
    expect(hhms_support_covers(&map.supports[0], 11, 0, NULL) == HHMS_COVER_CONFIRMED,
           "stone radial support covers eleven axial cells");
    expect(hhms_support_covers(&map.supports[0], 11, 3, NULL) == HHMS_COVER_NONE,
           "stone radial support excludes its old diagonal boundary");

    hhms_init(&map);
    hhms_add_support(&map, 0.0, 0.0, HHMS_SUP_BEAM, HHMS_ORIENT_NORTH, &id);
    expect(hhms_support_covers(&map.supports[0], 13, 4, NULL) == HHMS_COVER_CONFIRMED,
           "beam radial support covers thirteen-by-four cells");
    expect(hhms_support_covers(&map.supports[0], 13, 5, NULL) == HHMS_COVER_NONE,
           "beam radial support excludes thirteen-by-five cells");

    hhms_init(&map);
    hhms_add_support(&map, 0.0, 0.0, HHMS_SUP_MONUMENT, HHMS_ORIENT_NORTH, &id);
    expect(hhms_support_covers(&map.supports[0], 30, 0, NULL) == HHMS_COVER_CONFIRMED,
           "monument radial support includes its exact radius boundary");
    expect(hhms_support_covers(&map.supports[0], 18, 24, NULL) == HHMS_COVER_CONFIRMED,
           "monument radial support includes a Pythagorean radius boundary");
    expect(hhms_support_covers(&map.supports[0], 21, 22, NULL) == HHMS_COVER_NONE,
           "monument radial support excludes a wall beyond its radius");

    hhms_init(&map);
    hhms_add_support(&map, 0.25, -0.50, HHMS_SUP_WOOD, HHMS_ORIENT_NORTH, &id);
    expect(hhms_support_covers(&map.supports[0], 9, 0, NULL) == HHMS_COVER_CONFIRMED,
           "continuous radial support position affects coverage without grid rounding");
    expect(hhms_support_covers(&map.supports[0], 10, 0, NULL) == HHMS_COVER_NONE,
           "continuous radial support position preserves the true outer boundary");

    hhms_init(&map);
    hhms_set_tile(&map, 0, 0, HHMS_MINE, 0);
    hhms_add_support(&map, 0.0, 0.0, HHMS_SUP_TIMBER_TUNNEL,
                     HHMS_ORIENT_NORTH, &id);
    solve(&map, &analysis);
    double margin = 0.0;
    expect(hhms_support_covers(&map.supports[0], 0, 0, &margin) == HHMS_COVER_ESTIMATED,
           "directional support footprint is explicitly estimated");
    HhmsCellInfo directional = hhms_cell_info(&map, &analysis, 0, 0);
    expect(directional.coverage.estimated_count == 1 &&
           directional.coverage.confirmed_count == 0,
           "directional footprint contributes estimated but not confirmed coverage");
    expect(directional.action == HHMS_ACTION_CAVE,
           "directional estimated coverage never creates PROTECTED CAVE");

    hhms_init(&map);
    hhms_set_tile(&map, 0, 0, HHMS_CLEAR, 8);
    uint32_t first = 0;
    uint32_t second = 0;
    hhms_add_support(&map, 0.0, 0.0, HHMS_SUP_WOOD, HHMS_ORIENT_NORTH, &first);
    hhms_add_support(&map, 0.5, 0.0, HHMS_SUP_STONE, HHMS_ORIENT_NORTH, &second);
    solve(&map, &analysis);
    expect(hhms_support_exposed_caves(&map, &analysis, first) == 0,
           "removing one overlapping confirmed support exposes no proved cave");
    expect(hhms_del_support(&map, second) == HHMS_EDIT_OK,
           "overlapping confirmed support can be removed");
    expect(hhms_support_exposed_caves(&map, &analysis, first) == 8,
           "removing the last confirmed support exposes every proved ring cave");
}

static void test_transactions_redo_and_state_ids(void)
{
    static HhmsMap map;
    hhms_init(&map);
    uint64_t initial = hhms_state_id(&map);
    hhms_set_tile(&map, 50, 50, HHMS_OPEN, 0);
    expect(!hhms_can_undo(&map) && hhms_state_id(&map) == initial,
           "core setup outside begin-edit creates no history or new state id");

    uint32_t support = 0;
    expect(hhms_begin_edit(&map), "support placement transaction begins");
    expect(hhms_add_support(&map, 0.0, 0.0, HHMS_SUP_WOOD,
                            HHMS_ORIENT_NORTH, &support) == HHMS_EDIT_OK,
           "support placement transaction records a support");
    expect(hhms_commit_edit(&map), "support placement transaction commits one state");
    uint64_t wood_state = hhms_state_id(&map);

    expect(hhms_begin_edit(&map), "support replacement transaction begins");
    expect(hhms_update_support(&map, support, 1.25, -2.5, HHMS_SUP_STONE,
                               HHMS_ORIENT_EAST) == HHMS_EDIT_OK,
           "support replacement updates the same stable support id");
    expect(hhms_commit_edit(&map), "support replacement commits one state");
    uint64_t stone_state = hhms_state_id(&map);
    expect(hhms_undo(&map), "one undo reverses one support replacement transaction");
    int index = hhms_support_index_by_id(&map, support);
    expect(index >= 0 && map.supports[index].kind == HHMS_SUP_WOOD &&
           map.supports[index].x == 0.0 && map.supports[index].y == 0.0,
           "one undo restores the replaced support exactly");
    expect(hhms_state_id(&map) == wood_state,
           "undo returns to the exact saved support state id");
    expect(hhms_redo(&map), "one redo reapplies one support replacement transaction");
    index = hhms_support_index_by_id(&map, support);
    expect(index >= 0 && map.supports[index].kind == HHMS_SUP_STONE &&
           map.supports[index].orientation == HHMS_ORIENT_EAST,
           "one redo reapplies the complete support replacement");
    expect(hhms_state_id(&map) == stone_state,
           "redo returns to the exact replacement state id");

    expect(hhms_begin_edit(&map), "multi-cell stroke transaction begins");
    hhms_set_tile(&map, 0, 10, HHMS_MINE, 0);
    hhms_set_tile(&map, 1, 10, HHMS_MINE, 0);
    hhms_set_tile(&map, 2, 10, HHMS_MINE, 0);
    expect(hhms_commit_edit(&map), "multi-cell stroke commits as one transaction");
    uint64_t stroke_state = hhms_state_id(&map);
    expect(hhms_undo(&map), "one undo reverses a complete multi-cell stroke");
    expect(!hhms_get(&map, 0, 10) && !hhms_get(&map, 1, 10) && !hhms_get(&map, 2, 10),
           "one undo removes every cell in the stroke");
    expect(hhms_redo(&map), "one redo reapplies a complete multi-cell stroke");
    expect(hhms_get(&map, 0, 10) && hhms_get(&map, 1, 10) && hhms_get(&map, 2, 10),
           "one redo restores every cell in the stroke");
    expect(hhms_state_id(&map) == stroke_state,
           "stroke redo restores its exact state id");

    expect(hhms_undo(&map), "branching setup undoes the stroke");
    uint64_t branch_base = hhms_state_id(&map);
    expect(hhms_begin_edit(&map), "no-op transaction begins while a redo branch exists");
    expect(hhms_update_support(&map, support, 1.25, -2.5, HHMS_SUP_STONE,
                               HHMS_ORIENT_EAST) == HHMS_EDIT_NO_CHANGE,
           "repeating the exact support state is a no-op");
    expect(!hhms_commit_edit(&map), "no-op transaction creates no revision");
    expect(hhms_state_id(&map) == branch_base && hhms_can_redo(&map),
           "no-op transaction preserves the current state id and redo branch");
    expect(hhms_begin_edit(&map), "branching edit transaction begins");
    hhms_set_tile(&map, 9, 9, HHMS_OPEN, 0);
    expect(hhms_commit_edit(&map), "branching edit transaction commits");
    expect(!hhms_can_redo(&map) && !hhms_redo(&map),
           "new edit after undo discards the old redo branch");
    expect(hhms_state_id(&map) != branch_base && hhms_state_id(&map) != stroke_state,
           "new history branch receives a distinct state id");
}

static void test_tile_storage_boundary(void)
{
    static HhmsMap map;
    hhms_init(&map);
    int accepted = 1;
    for (int i = 0; i < HHMS_MAX_TILES; i++) {
        if (hhms_set_tile(&map, i, 200, HHMS_MINE, 0) != HHMS_EDIT_OK) {
            accepted = 0;
            break;
        }
    }
    expect(accepted && map.ntiles == HHMS_MAX_TILES,
           "map stores exactly 8192 authored tiles");
    expect(hhms_set_tile(&map, HHMS_MAX_TILES, 200, HHMS_MINE, 0) ==
           HHMS_EDIT_MAP_FULL,
           "map rejects the 8193rd authored tile with the storage-limit result");
    expect(map.ntiles == HHMS_MAX_TILES &&
           !hhms_get(&map, HHMS_MAX_TILES, 200),
           "8193rd tile rejection leaves the full map unchanged");
}

typedef struct {
    char path[512];
    int ready;
} TempDirectory;

static int temp_directory_create(TempDirectory *directory)
{
    memset(directory, 0, sizeof(*directory));
#ifdef _WIN32
    char base[MAX_PATH];
    char unique[MAX_PATH];
    DWORD length = GetTempPathA((DWORD)sizeof(base), base);
    if (length == 0 || length >= sizeof(base))
        return 0;
    if (!GetTempFileNameA(base, "hhm", 0, unique))
        return 0;
    DeleteFileA(unique);
    if (_mkdir(unique) != 0)
        return 0;
    snprintf(directory->path, sizeof(directory->path), "%s", unique);
#else
    if (snprintf(directory->path, sizeof(directory->path),
                 "/tmp/hhms-tests-XXXXXX") >= (int)sizeof(directory->path))
        return 0;
    int descriptor = mkstemp(directory->path);
    if (descriptor < 0)
        return 0;
    close(descriptor);
    if (remove(directory->path) != 0 || mkdir(directory->path, 0700) != 0)
        return 0;
#endif
    directory->ready = 1;
    return 1;
}

static void temp_path(const TempDirectory *directory, const char *name,
                      char *path, size_t capacity)
{
    snprintf(path, capacity, "%s/%s", directory->path, name);
}

static int write_text(const char *path, const char *text)
{
    FILE *file = fopen(path, "wb");
    if (!file)
        return 0;
    size_t length = strlen(text);
    int ok = fwrite(text, 1, length, file) == length && fclose(file) == 0;
    return ok;
}

static int read_text(const char *path, char *buffer, size_t capacity)
{
    FILE *file = fopen(path, "rb");
    if (!file || capacity == 0) {
        if (file)
            fclose(file);
        return 0;
    }
    size_t length = fread(buffer, 1, capacity - 1, file);
    int ok = !ferror(file) && fclose(file) == 0;
    buffer[length] = '\0';
    return ok;
}

static void sentinel_document(HhmsMap *map, HhmsView *view)
{
    hhms_init(map);
    hhms_set_tile(map, 77, -31, HHMS_MINE, 0);
    hhms_add_support(map, 2.25, -4.5, HHMS_SUP_STONE, HHMS_ORIENT_WEST, NULL);
    hhms_begin_edit(map);
    hhms_set_tile(map, 78, -31, HHMS_OPEN, 0);
    hhms_commit_edit(map);
    hhms_view_init(view);
    view->camx = 123.5;
    view->camy = -87.25;
    view->cell = 41.0f;
    view->ui_scale = 1.5f;
    view->has_view = 1;
}

static void expect_rejected_unchanged(const char *path, const char *json,
                                      const char *behavior)
{
    static HhmsMap map;
    static HhmsMap before;
    HhmsView view;
    HhmsView before_view;
    HhmsError error;
    sentinel_document(&map, &view);
    before = map;
    before_view = view;
    if (!write_text(path, json)) {
        expect(0, "invalid persistence fixture can be written outside the repository");
        return;
    }
    HhmsIoResult result = hhms_load(&map, &view, path, &error);
    expect(result != HHMS_IO_OK, behavior);
    expect(memcmp(&map, &before, sizeof(map)) == 0,
           "rejected persistence input leaves authored map and history unchanged");
    expect(memcmp(&view, &before_view, sizeof(view)) == 0,
           "rejected persistence input leaves view unchanged");
}

static void test_v1_examples_and_v2_roundtrip(const TempDirectory *directory)
{
    char path[640];
    temp_path(directory, "compat.hhmap", path, sizeof(path));
    const char *v1 =
        "{\n"
        "  \"version\": 1,\n"
        "  \"tiles\": [\n"
        "    {\"x\": 0, \"y\": 0, \"kind\": \"clear\", \"n\": 0},\n"
        "    {\"x\": 4, \"y\": 0, \"kind\": \"clear\", \"n\": 1},\n"
        "    {\"x\": 5, \"y\": 0, \"kind\": \"mine\"},\n"
        "    {\"x\": -4, \"y\": 7, \"kind\": \"open\"}\n"
        "  ],\n"
        "  \"supports\": [{\"x\": -6, \"y\": 0, \"kind\": \"wood\"}]\n"
        "}\n";
    expect(write_text(path, v1), "exact version-one compatibility fixture is written");
    static HhmsMap loaded;
    HhmsView loaded_view;
    HhmsError error;
    hhms_init(&loaded);
    hhms_view_init(&loaded_view);
    expect(hhms_load(&loaded, &loaded_view, path, &error) == HHMS_IO_OK,
           "exact version-one map example loads successfully");
    const HhmsTile *tile = hhms_get(&loaded, 4, 0);
    expect(tile && tile->kind == HHMS_CLEAR && tile->count == 1,
           "version-one clear clue and count load exactly");
    tile = hhms_get(&loaded, -4, 7);
    expect(tile && tile->kind == HHMS_OPEN && tile->terrain == HHMS_TERRAIN_ROCK,
           "version-one open floor defaults to rock terrain");
    expect(loaded.nsupports == 1 && loaded.supports[0].x == -6.0 &&
           loaded.supports[0].kind == HHMS_SUP_WOOD &&
           loaded.supports[0].orientation == HHMS_ORIENT_NORTH,
           "version-one support defaults to north orientation and continuous coordinates");
    expect(!loaded_view.has_view,
           "version-one document without view reports that no saved view exists");
    const char *v1_ring =
        "{\n"
        "  \"version\": 1,\n"
        "  \"tiles\": [\n"
        "    {\"x\": 1, \"y\": 1, \"kind\": \"clear\", \"n\": 0},\n"
        "    {\"x\": 2, \"y\": 1, \"kind\": \"clear\", \"n\": 0},\n"
        "    {\"x\": 3, \"y\": 1, \"kind\": \"clear\", \"n\": 0},\n"
        "    {\"x\": 1, \"y\": 2, \"kind\": \"clear\", \"n\": 0},\n"
        "    {\"x\": 3, \"y\": 2, \"kind\": \"clear\", \"n\": 0},\n"
        "    {\"x\": 1, \"y\": 3, \"kind\": \"clear\", \"n\": 0},\n"
        "    {\"x\": 2, \"y\": 3, \"kind\": \"clear\", \"n\": 0},\n"
        "    {\"x\": 3, \"y\": 3, \"kind\": \"clear\", \"n\": 0},\n"
        "    {\"x\": 2, \"y\": 4, \"kind\": \"clear\", \"n\": 0},\n"
        "    {\"x\": 2, \"y\": 2, \"kind\": \"clear\", \"n\": 0}\n"
        "  ],\n"
        "  \"supports\": []\n"
        "}\n";
    expect(write_text(path, v1_ring),
           "second exact version-one example fixture is written");
    expect(hhms_load(&loaded, &loaded_view, path, &error) == HHMS_IO_OK &&
           loaded.ntiles == 10 && loaded.nsupports == 0,
           "second exact version-one ring example loads every clue");
    tile = hhms_get(&loaded, 2, 2);
    expect(tile && tile->kind == HHMS_CLEAR && tile->count == 0,
           "second version-one example preserves its center zero clue");

    static HhmsMap original;
    static HhmsMap roundtrip;
    HhmsView original_view;
    HhmsView roundtrip_view;
    hhms_init(&original);
    hhms_set_tile(&original, 2, -3, HHMS_CLEAR, 4);
    hhms_set_tile(&original, 5, 5, HHMS_MINE, 0);
    hhms_set_tile(&original, -4, 7, HHMS_OPEN, 0);
    hhms_set_terrain(&original, 8, -2, HHMS_TERRAIN_WATER);
    hhms_add_support(&original, 0.125, -0.75, HHMS_SUP_BEAM,
                     HHMS_ORIENT_SOUTH, NULL);
    hhms_add_support(&original, 12.5, 9.25, HHMS_SUP_STONE_ARCH,
                     HHMS_ORIENT_EAST, NULL);
    hhms_view_init(&original_view);
    original_view.camx = -17.25;
    original_view.camy = 88.5;
    original_view.cell = 37.5f;
    original_view.ui_scale = 1.25f;
    original_view.has_view = 1;
    temp_path(directory, "roundtrip.hhmap", path, sizeof(path));
    expect(hhms_save(&original, &original_view, path, &error) == HHMS_IO_OK,
           "version-two document with water supports orientation and view saves");
    hhms_init(&roundtrip);
    hhms_view_init(&roundtrip_view);
    expect(hhms_load(&roundtrip, &roundtrip_view, path, &error) == HHMS_IO_OK,
           "saved version-two document loads");
    tile = hhms_get(&roundtrip, 8, -2);
    expect(tile && tile->kind == HHMS_UNKNOWN && tile->terrain == HHMS_TERRAIN_WATER,
           "version-two roundtrip preserves authored water terrain");
    expect(roundtrip.nsupports == 2 &&
           fabs(roundtrip.supports[0].x - 0.125) < 1e-12 &&
           fabs(roundtrip.supports[0].y + 0.75) < 1e-12,
           "version-two roundtrip preserves continuous support positions");
    expect(roundtrip.supports[0].orientation == HHMS_ORIENT_SOUTH &&
           roundtrip.supports[1].kind == HHMS_SUP_STONE_ARCH &&
           roundtrip.supports[1].orientation == HHMS_ORIENT_EAST,
           "version-two roundtrip preserves support kinds and orientations");
    expect(roundtrip_view.has_view && roundtrip_view.camx == original_view.camx &&
           roundtrip_view.camy == original_view.camy &&
           roundtrip_view.cell == original_view.cell &&
           roundtrip_view.ui_scale == original_view.ui_scale,
           "version-two roundtrip preserves the complete saved view");
    expect(!hhms_can_undo(&roundtrip) && !hhms_can_redo(&roundtrip),
           "loading establishes authored state without synthetic history transactions");

    remove(path);
    temp_path(directory, "compat.hhmap", path, sizeof(path));
    remove(path);
}

static void test_persistence_rejection_table(const TempDirectory *directory)
{
    char path[640];
    temp_path(directory, "invalid.hhmap", path, sizeof(path));
    static const struct {
        const char *json;
        const char *behavior;
    } cases[] = {
        {"{}", "empty object is rejected as a document with missing required fields"},
        {"{\"tiles\":[],\"supports\":[]}", "document missing version is rejected"},
        {"{\"version\":0,\"tiles\":[],\"supports\":[]}", "unsupported old version is rejected"},
        {"{\"version\":3,\"tiles\":[],\"supports\":[]}", "unsupported future version is rejected"},
        {"{\"version\":\"2\",\"tiles\":[],\"supports\":[]}", "non-numeric version is rejected"},
        {"{\"version\":2,\"tiles\":[]}", "document missing supports array is rejected"},
        {"{\"version\":2,\"supports\":[]}", "document missing tiles array is rejected"},
        {"{\"version\":2,\"tiles\":{},\"supports\":[]}", "wrong tiles collection type is rejected"},
        {"{\"version\":2,\"tiles\":[],\"supports\":{}}", "wrong supports collection type is rejected"},
        {"{\"version\":2,\"version\":2,\"tiles\":[],\"supports\":[]}", "duplicate root field is rejected"},
        {"{\"version\":2,\"tiles\":[],\"supports\":[],\"mystery\":0}", "unknown root field is rejected"},
        {"{\"version\":2,\"tiles\":[{\"y\":0,\"kind\":\"mine\",\"terrain\":\"rock\"}],\"supports\":[]}", "tile missing x coordinate is rejected"},
        {"{\"version\":2,\"tiles\":[{\"x\":0,\"kind\":\"mine\",\"terrain\":\"rock\"}],\"supports\":[]}", "tile missing y coordinate is rejected"},
        {"{\"version\":2,\"tiles\":[{\"x\":0,\"y\":0,\"terrain\":\"rock\"}],\"supports\":[]}", "tile missing kind is rejected"},
        {"{\"version\":2,\"tiles\":[{\"x\":0,\"y\":0,\"kind\":\"mine\"}],\"supports\":[]}", "version-two tile missing terrain is rejected"},
        {"{\"version\":2,\"tiles\":[{\"x\":0,\"y\":0,\"kind\":\"clear\",\"terrain\":\"rock\"}],\"supports\":[]}", "clear tile missing count is rejected"},
        {"{\"version\":2,\"tiles\":[{\"x\":0,\"y\":0,\"kind\":\"mine\",\"n\":1,\"terrain\":\"rock\"}],\"supports\":[]}", "count field on a non-clear tile is rejected"},
        {"{\"version\":2,\"tiles\":[],\"supports\":[{\"y\":0,\"kind\":\"wood\",\"orientation\":\"north\"}]}", "support missing x coordinate is rejected"},
        {"{\"version\":2,\"tiles\":[],\"supports\":[{\"x\":0,\"kind\":\"wood\",\"orientation\":\"north\"}]}", "support missing y coordinate is rejected"},
        {"{\"version\":2,\"tiles\":[],\"supports\":[{\"x\":0,\"y\":0,\"orientation\":\"north\"}]}", "support missing kind is rejected"},
        {"{\"version\":2,\"tiles\":[],\"supports\":[{\"x\":0,\"y\":0,\"kind\":\"wood\"}]}", "version-two support missing orientation is rejected"},
        {"{\"version\":2,\"tiles\":[],\"supports\":[],\"view\":{\"camx\":0,\"camy\":0,\"cell\":32}}", "view missing ui scale is rejected"},
        {"{\"version\":2,\"tiles\":[],\"supports\":[],\"view\":{\"camx\":0,\"camy\":0,\"cell\":32,\"ui_scale\":1,\"extra\":0}}", "unknown view field is rejected"},
        {"{\"version\":2,\"tiles\":[{\"x\":0,\"x\":1,\"y\":0,\"kind\":\"mine\",\"terrain\":\"rock\"}],\"supports\":[]}", "duplicate tile field is rejected"},
        {"{\"version\":2,\"tiles\":[{\"x\":0,\"y\":0,\"kind\":\"mine\",\"terrain\":\"rock\",\"extra\":0}],\"supports\":[]}", "unknown tile field is rejected"},
        {"{\"version\":2,\"tiles\":[],\"supports\":[{\"x\":0,\"y\":0,\"kind\":\"wood\",\"orientation\":\"north\",\"extra\":0}]}", "unknown support field is rejected"},
        {"{\"version\":2,\"tiles\":[{\"x\":0,\"y\":0,\"kind\":\"sand\",\"terrain\":\"rock\"}],\"supports\":[]}", "unknown tile kind is rejected"},
        {"{\"version\":2,\"tiles\":[],\"supports\":[{\"x\":0,\"y\":0,\"kind\":\"wood\",\"orientation\":\"up\"}]}", "unknown support orientation is rejected"},
        {"{\"version\":2,\"tiles\":[],\"supports\":[{\"x\":0,\"y\":0,\"kind\":\"plastic\",\"orientation\":\"north\"}]}", "unknown support kind is rejected"},
        {"{\"version\":2,\"tiles\":[{\"x\":0,\"y\":0,\"kind\":\"clear\",\"n\":1,\"terrain\":\"water\"}],\"supports\":[]}", "water clear clue terrain combination is rejected"},
        {"{\"version\":2,\"tiles\":[{\"x\":0,\"y\":0,\"kind\":\"open\",\"terrain\":\"water\"}],\"supports\":[]}", "water open-floor terrain combination is rejected"},
        {"{\"version\":2,\"tiles\":[{\"x\":0,\"y\":0,\"kind\":\"mine\",\"terrain\":\"lava\"}],\"supports\":[]}", "unknown terrain is rejected"},
        {"{\"version\":2,\"tiles\":[{\"x\":0,\"y\":0,\"kind\":\"clear\",\"n\":-1,\"terrain\":\"rock\"}],\"supports\":[]}", "negative clear count is rejected"},
        {"{\"version\":2,\"tiles\":[{\"x\":0,\"y\":0,\"kind\":\"clear\",\"n\":9,\"terrain\":\"rock\"}],\"supports\":[]}", "clear count above eight is rejected"},
        {"{\"version\":2,\"tiles\":[{\"x\":1000001,\"y\":0,\"kind\":\"mine\",\"terrain\":\"rock\"}],\"supports\":[]}", "tile coordinate beyond supported range is rejected"},
        {"{\"version\":2,\"tiles\":[{\"x\":2147483648,\"y\":0,\"kind\":\"mine\",\"terrain\":\"rock\"}],\"supports\":[]}", "overflowing integer coordinate is rejected"},
        {"{\"version\":2,\"tiles\":[],\"supports\":[{\"x\":1e309,\"y\":0,\"kind\":\"wood\",\"orientation\":\"north\"}]}", "overflowing support coordinate is rejected"},
        {"{\"version\":2,\"tiles\":[],\"supports\":[],\"view\":{\"camx\":1e309,\"camy\":0,\"cell\":32,\"ui_scale\":1}}", "overflowing view number is rejected"},
        {"{\"version\":2,\"tiles\":[],\"supports\":[],\"view\":{\"camx\":NaN,\"camy\":0,\"cell\":32,\"ui_scale\":1}}", "nonfinite JSON number spelling is rejected"},
        {"{\"version\":2,\"tiles\":[],\"supports\":[]} trailing", "trailing garbage after a document is rejected"}
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        expect_rejected_unchanged(path, cases[i].json, cases[i].behavior);
    remove(path);
}

static void test_too_large_and_atomic_failure(const TempDirectory *directory)
{
    char path[640];
    temp_path(directory, "too-large.hhmap", path, sizeof(path));
    FILE *file = fopen(path, "wb");
    int wrote = file != NULL;
    if (file) {
        char block[4096];
        memset(block, ' ', sizeof(block));
        size_t remaining = 8u * 1024u * 1024u + 1u;
        while (remaining > 0 && wrote) {
            size_t amount = remaining < sizeof(block) ? remaining : sizeof(block);
            wrote = fwrite(block, 1, amount, file) == amount;
            remaining -= amount;
        }
        wrote = fclose(file) == 0 && wrote;
    }
    expect(wrote, "over-eight-mebibyte fixture is written outside the repository");
    static HhmsMap map;
    static HhmsMap before;
    HhmsView view;
    HhmsView before_view;
    HhmsError error;
    sentinel_document(&map, &view);
    before = map;
    before_view = view;
    expect(hhms_load(&map, &view, path, &error) == HHMS_IO_TOO_LARGE,
           "document larger than eight mebibytes is rejected with the size-limit result");
    expect(memcmp(&map, &before, sizeof(map)) == 0 &&
           memcmp(&view, &before_view, sizeof(view)) == 0,
           "oversized document rejection leaves map and view unchanged");
    remove(path);

#ifndef _WIN32
    char unique_destination[640];
    char predictable_sibling[648];
    temp_path(directory, "unique-save.hhmap", unique_destination,
              sizeof(unique_destination));
    snprintf(predictable_sibling, sizeof(predictable_sibling), "%s.tmp",
             unique_destination);
    const char *sibling_marker = "unrelated predictable sibling";
    expect(write_text(predictable_sibling, sibling_marker),
           "predictable sibling sentinel is prepared");
    hhms_init(&map);
    hhms_set_tile(&map, 0, 0, HHMS_MINE, 0);
    hhms_view_init(&view);
    expect(hhms_save(&map, &view, unique_destination, &error) == HHMS_IO_OK,
           "save uses an exclusive unique temporary sibling");
    char sibling_contents[128];
    expect(read_text(predictable_sibling, sibling_contents,
                     sizeof(sibling_contents)) &&
           strcmp(sibling_contents, sibling_marker) == 0,
           "save never truncates an unrelated predictable sibling");
    expect(chmod(unique_destination, 0600) == 0,
           "destination mode is restricted before replacement");
    expect(hhms_save(&map, &view, unique_destination, &error) == HHMS_IO_OK,
           "save replaces an existing restricted destination");
    struct stat saved_stat;
    expect(stat(unique_destination, &saved_stat) == 0 &&
           (saved_stat.st_mode & 0777) == 0600,
           "atomic replacement preserves destination permission bits");
    remove(predictable_sibling);
    remove(unique_destination);

    char locked[640];
    temp_path(directory, "locked", locked, sizeof(locked));
    int made = mkdir(locked, 0700) == 0;
    char destination[700];
    snprintf(destination, sizeof(destination), "%s/existing.hhmap", locked);
    const char *marker = "existing destination must survive";
    int prepared = made && write_text(destination, marker) && chmod(locked, 0500) == 0;
    expect(prepared, "atomic-save failure destination is prepared outside the repository");
    if (prepared) {
        hhms_init(&map);
        hhms_set_tile(&map, 0, 0, HHMS_MINE, 0);
        hhms_view_init(&view);
        HhmsIoResult result = hhms_save(&map, &view, destination, &error);
        expect(result != HHMS_IO_OK,
               "atomic save reports failure when its destination directory is not writable");
        char contents[128];
        expect(read_text(destination, contents, sizeof(contents)) && strcmp(contents, marker) == 0,
               "failed atomic save preserves the existing destination bytes");
    }
    chmod(locked, 0700);
    remove(destination);
    rmdir(locked);
#endif
}

static void test_persistence(void)
{
    TempDirectory directory;
    if (!temp_directory_create(&directory)) {
        expect(0, "isolated temporary persistence directory can be created");
        return;
    }
    expect(1, "persistence tests use an isolated temporary directory outside the repository");
    test_v1_examples_and_v2_roundtrip(&directory);
    test_persistence_rejection_table(&directory);
    test_too_large_and_atomic_failure(&directory);
    expect(rmdir(directory.path) == 0,
           "temporary persistence directory is empty and cleaned up");
}

int main(void)
{
    test_zero_open_and_full_ring();
    test_flags_and_contradictions();
    test_121_overlap_and_screenshot_ring();
    test_solver_idempotence();
    test_independent_bruteforce_oracle();
    test_original_component_counts_survive_split();
    test_fail_closed_and_enumeration_limit();
    test_conflict_location_survives_analysis_cap();
    test_water_and_cell_provenance();
    test_support_geometry_and_exposure();
    test_transactions_redo_and_state_ids();
    test_tile_storage_boundary();
    test_persistence();

    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
