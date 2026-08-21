#include "hhms.h"

#include <string.h>

static const int DX[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
static const int DY[8] = {-1, -1, -1, 0, 0, 1, 1, 1};

static unsigned hash_xy(int x, int y)
{
    unsigned h = (unsigned)x * 0x9E3779B9u ^ (unsigned)y * 0x85EBCA6Bu;
    return h & (HHMS_HASH - 1);
}

static int hash_find(const HhmsSlot *hash, int x, int y)
{
    unsigned h = hash_xy(x, y);
    for (int n = 0; n < HHMS_HASH; n++) {
        const HhmsSlot *slot = &hash[h];
        if (slot->occ == 0)
            return -1;
        if (slot->occ == 1 && slot->x == x && slot->y == y)
            return slot->ti;
        h = (h + 1) & (HHMS_HASH - 1);
    }
    return -1;
}

static int hash_put(HhmsSlot *hash, int x, int y, int index)
{
    unsigned h = hash_xy(x, y);
    for (int n = 0; n < HHMS_HASH; n++) {
        HhmsSlot *slot = &hash[h];
        if (slot->occ == 1 && slot->x == x && slot->y == y) {
            slot->ti = index;
            return 1;
        }
        if (slot->occ == 0 || slot->occ == 2) {
            slot->occ = 1;
            slot->x = x;
            slot->y = y;
            slot->ti = index;
            return 1;
        }
        h = (h + 1) & (HHMS_HASH - 1);
    }
    return 0;
}

void hhms_analysis_init(HhmsAnalysis *analysis)
{
    memset(analysis, 0, sizeof(*analysis));
    analysis->complete = 1;
}

const HhmsAnalysisCell *hhms_analysis_get(const HhmsAnalysis *analysis, int x, int y)
{
    int index = hash_find(analysis->hash, x, y);
    return index >= 0 ? &analysis->cells[index] : NULL;
}

static HhmsAnalysisCell *analysis_get_mut(HhmsAnalysis *analysis, int x, int y)
{
    int index = hash_find(analysis->hash, x, y);
    return index >= 0 ? &analysis->cells[index] : NULL;
}

static HhmsAnalysisCell *analysis_touch(HhmsAnalysis *analysis, int x, int y)
{
    HhmsAnalysisCell *cell = analysis_get_mut(analysis, x, y);
    if (cell)
        return cell;
    if (analysis->ncells >= HHMS_MAX_TILES) {
        analysis->limits |= HHMS_LIMIT_ANALYSIS_CELLS;
        analysis->complete = 0;
        return NULL;
    }
    int index = analysis->ncells++;
    cell = &analysis->cells[index];
    memset(cell, 0, sizeof(*cell));
    cell->x = x;
    cell->y = y;
    if (!hash_put(analysis->hash, x, y, index)) {
        analysis->ncells--;
        analysis->limits |= HHMS_LIMIT_ANALYSIS_CELLS;
        analysis->complete = 0;
        return NULL;
    }
    return cell;
}
static HhmsAnalysisCell *analysis_touch_conflict(HhmsAnalysis *analysis,
                                                 int x, int y)
{
    HhmsAnalysisCell *cell = analysis_get_mut(analysis, x, y);
    if (cell)
        return cell;
    if (analysis->ncells < HHMS_MAX_TILES)
        return analysis_touch(analysis, x, y);

    analysis->limits |= HHMS_LIMIT_ANALYSIS_CELLS;
    analysis->complete = 0;
    for (int i = 0; i < analysis->ncells; i++) {
        if (analysis->cells[i].mark == HHMS_MARK_CONFLICT)
            continue;
        cell = &analysis->cells[i];
        memset(cell, 0, sizeof(*cell));
        cell->x = x;
        cell->y = y;
        memset(analysis->hash, 0, sizeof(analysis->hash));
        for (int j = 0; j < analysis->ncells; j++)
            hash_put(analysis->hash, analysis->cells[j].x,
                     analysis->cells[j].y, j);
        return cell;
    }
    return NULL;
}


static int observed_mine(const HhmsMap *map, int x, int y)
{
    const HhmsTile *tile = hhms_get(map, x, y);
    return tile && tile->kind == HHMS_MINE;
}

static int observed_safe(const HhmsMap *map, int x, int y)
{
    const HhmsTile *tile = hhms_get(map, x, y);
    return tile && (tile->kind == HHMS_CLEAR || tile->kind == HHMS_OPEN);
}

static int known_mine(const HhmsMap *map, const HhmsAnalysis *analysis, int x, int y)
{
    if (observed_mine(map, x, y))
        return 1;
    const HhmsAnalysisCell *cell = hhms_analysis_get(analysis, x, y);
    return cell && cell->mark == HHMS_MARK_MINE;
}

static int known_safe(const HhmsMap *map, const HhmsAnalysis *analysis, int x, int y)
{
    if (observed_safe(map, x, y))
        return 1;
    const HhmsAnalysisCell *cell = hhms_analysis_get(analysis, x, y);
    return cell && cell->mark == HHMS_MARK_SAFE;
}

static void set_reason(HhmsAnalysisCell *cell, HhmsReason reason,
                       int x1, int y1, int x2, int y2)
{
    cell->reason = reason;
    cell->reason_x[0] = x1;
    cell->reason_y[0] = y1;
    cell->reason_x[1] = x2;
    cell->reason_y[1] = y2;
}

static void mark_conflict(HhmsAnalysis *analysis, int x, int y,
                          HhmsReason reason, int x1, int y1, int x2, int y2)
{
    HhmsAnalysisCell *cell = analysis_touch_conflict(analysis, x, y);
    analysis->contradiction = 1;
    if (!cell)
        return;
    cell->mark = HHMS_MARK_CONFLICT;
    cell->mine_models = 0;
    cell->total_models = 0;
    set_reason(cell, reason, x1, y1, x2, y2);
}

static int mark_safe(HhmsMap const *map, HhmsAnalysis *analysis, int x, int y,
                     HhmsReason reason, int x1, int y1, int x2, int y2)
{
    if (observed_mine(map, x, y)) {
        mark_conflict(analysis, x, y, reason, x1, y1, x2, y2);
        mark_conflict(analysis, x1, y1, reason, x, y, x2, y2);
        return 0;
    }
    if (observed_safe(map, x, y))
        return 0;
    HhmsAnalysisCell *cell = analysis_get_mut(analysis, x, y);
    if (cell && cell->mark == HHMS_MARK_CONFLICT) {
        analysis->contradiction = 1;
        return 0;
    }
    if (cell && cell->mark == HHMS_MARK_MINE) {
        mark_conflict(analysis, x, y, reason, x1, y1, x2, y2);
        mark_conflict(analysis, x1, y1, reason, x, y, x2, y2);
        return 0;
    }
    if (cell && cell->mark == HHMS_MARK_SAFE)
        return 0;
    cell = analysis_touch(analysis, x, y);
    if (!cell)
        return 0;
    cell->mark = HHMS_MARK_SAFE;
    if (cell->total_models == 0) {
        cell->mine_models = 0;
        cell->total_models = 1;
    }
    set_reason(cell, reason, x1, y1, x2, y2);
    return 1;
}

static int mark_mine(HhmsMap const *map, HhmsAnalysis *analysis, int x, int y,
                     HhmsReason reason, int x1, int y1, int x2, int y2)
{
    if (observed_safe(map, x, y)) {
        mark_conflict(analysis, x, y, reason, x1, y1, x2, y2);
        mark_conflict(analysis, x1, y1, reason, x, y, x2, y2);
        return 0;
    }
    if (observed_mine(map, x, y))
        return 0;
    HhmsAnalysisCell *cell = analysis_get_mut(analysis, x, y);
    if (cell && cell->mark == HHMS_MARK_CONFLICT) {
        analysis->contradiction = 1;
        return 0;
    }
    if (cell && cell->mark == HHMS_MARK_SAFE) {
        mark_conflict(analysis, x, y, reason, x1, y1, x2, y2);
        mark_conflict(analysis, x1, y1, reason, x, y, x2, y2);
        return 0;
    }
    if (cell && cell->mark == HHMS_MARK_MINE)
        return 0;
    cell = analysis_touch(analysis, x, y);
    if (!cell)
        return 0;
    cell->mark = HHMS_MARK_MINE;
    if (cell->total_models == 0) {
        cell->mine_models = 1;
        cell->total_models = 1;
    }
    set_reason(cell, reason, x1, y1, x2, y2);
    return 1;
}

typedef struct {
    int x[8], y[8];
    int n;
    int need;
    int clue_x, clue_y;
} Constraint;

static void gather(const HhmsMap *map, const HhmsAnalysis *analysis,
                   const HhmsTile *clue, Constraint *out)
{
    memset(out, 0, sizeof(*out));
    out->need = clue->count;
    out->clue_x = clue->x;
    out->clue_y = clue->y;
    for (int i = 0; i < 8; i++) {
        int x = clue->x + DX[i];
        int y = clue->y + DY[i];
        if (known_mine(map, analysis, x, y)) {
            out->need--;
        } else if (!known_safe(map, analysis, x, y)) {
            out->x[out->n] = x;
            out->y[out->n] = y;
            out->n++;
        }
    }
}

static int apply_simple(const HhmsMap *map, HhmsAnalysis *analysis)
{
    int changed = 0;
    for (int i = 0; i < map->ntiles; i++) {
        const HhmsTile *clue = &map->tiles[i];
        if (clue->kind != HHMS_CLEAR)
            continue;
        Constraint constraint;
        gather(map, analysis, clue, &constraint);
        if (constraint.need < 0 || constraint.need > constraint.n) {
            mark_conflict(analysis, clue->x, clue->y, HHMS_REASON_SIMPLE,
                          clue->x, clue->y, 0, 0);
            continue;
        }
        if (constraint.need == 0) {
            for (int k = 0; k < constraint.n; k++) {
                changed |= mark_safe(map, analysis, constraint.x[k], constraint.y[k],
                                     HHMS_REASON_SIMPLE, clue->x, clue->y, 0, 0);
            }
        } else if (constraint.need == constraint.n) {
            for (int k = 0; k < constraint.n; k++) {
                changed |= mark_mine(map, analysis, constraint.x[k], constraint.y[k],
                                     HHMS_REASON_SIMPLE, clue->x, clue->y, 0, 0);
            }
        }
    }
    return changed;
}

static int same_cell(int x1, int y1, int x2, int y2)
{
    return x1 == x2 && y1 == y2;
}

static int is_subset(const Constraint *small, const Constraint *big)
{
    for (int i = 0; i < small->n; i++) {
        int found = 0;
        for (int j = 0; j < big->n; j++) {
            if (same_cell(small->x[i], small->y[i], big->x[j], big->y[j])) {
                found = 1;
                break;
            }
        }
        if (!found)
            return 0;
    }
    return 1;
}

static void extra_cells(const Constraint *big, const Constraint *small,
                        Constraint *out)
{
    memset(out, 0, sizeof(*out));
    for (int i = 0; i < big->n; i++) {
        int found = 0;
        for (int j = 0; j < small->n; j++) {
            if (same_cell(big->x[i], big->y[i], small->x[j], small->y[j])) {
                found = 1;
                break;
            }
        }
        if (!found) {
            out->x[out->n] = big->x[i];
            out->y[out->n] = big->y[i];
            out->n++;
        }
    }
    out->need = big->need - small->need;
}

static int collect_constraints(const HhmsMap *map, const HhmsAnalysis *analysis,
                               Constraint *constraints)
{
    int count = 0;
    for (int i = 0; i < map->ntiles; i++) {
        if (map->tiles[i].kind != HHMS_CLEAR)
            continue;
        gather(map, analysis, &map->tiles[i], &constraints[count]);
        if (constraints[count].n > 0)
            count++;
    }
    return count;
}

static int apply_subset(const HhmsMap *map, HhmsAnalysis *analysis)
{
    static Constraint constraints[HHMS_MAX_TILES];
    int count = collect_constraints(map, analysis, constraints);
    int changed = 0;
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < count; j++) {
            if (i == j || !is_subset(&constraints[i], &constraints[j]))
                continue;
            Constraint extra;
            extra_cells(&constraints[j], &constraints[i], &extra);
            if (extra.need < 0 || extra.need > extra.n) {
                mark_conflict(analysis, constraints[i].clue_x,
                              constraints[i].clue_y, HHMS_REASON_SUBSET,
                              constraints[i].clue_x, constraints[i].clue_y,
                              constraints[j].clue_x, constraints[j].clue_y);
                mark_conflict(analysis, constraints[j].clue_x,
                              constraints[j].clue_y, HHMS_REASON_SUBSET,
                              constraints[i].clue_x, constraints[i].clue_y,
                              constraints[j].clue_x, constraints[j].clue_y);
                continue;
            }
            if (extra.n == 0)
                continue;
            if (extra.need == 0) {
                for (int k = 0; k < extra.n; k++) {
                    changed |= mark_safe(map, analysis, extra.x[k], extra.y[k],
                                         HHMS_REASON_SUBSET,
                                         constraints[i].clue_x, constraints[i].clue_y,
                                         constraints[j].clue_x, constraints[j].clue_y);
                }
            } else if (extra.need == extra.n) {
                for (int k = 0; k < extra.n; k++) {
                    changed |= mark_mine(map, analysis, extra.x[k], extra.y[k],
                                         HHMS_REASON_SUBSET,
                                         constraints[i].clue_x, constraints[i].clue_y,
                                         constraints[j].clue_x, constraints[j].clue_y);
                }
            }
        }
    }
    return changed;
}

typedef struct {
    int x, y;
} Cell;

typedef struct {
    unsigned mask;
    int need;
    int constraint_index;
} BitConstraint;

typedef struct {
    uint64_t valid;
    uint64_t mine_count[HHMS_ENUM_MAX];
    int n;
    int constraint_count;
    BitConstraint constraints[HHMS_ENUM_MAX * 8];
} Enumeration;

static int popcount32(unsigned value)
{
    value = value - ((value >> 1) & 0x55555555u);
    value = (value & 0x33333333u) + ((value >> 2) & 0x33333333u);
    return (int)((((value + (value >> 4)) & 0x0F0F0F0Fu) * 0x01010101u) >> 24);
}

static int partial_valid(const Enumeration *enumeration, int decided_bits,
                         unsigned assigned)
{
    unsigned decided = decided_bits == 0 ? 0u : ((1u << decided_bits) - 1u);
    for (int i = 0; i < enumeration->constraint_count; i++) {
        unsigned mask = enumeration->constraints[i].mask;
        int have = popcount32(assigned & mask & decided);
        int remaining = popcount32(mask & ~decided);
        if (have > enumeration->constraints[i].need ||
            have + remaining < enumeration->constraints[i].need)
            return 0;
    }
    return 1;
}

static void enumerate(Enumeration *enumeration, int bit, unsigned assigned)
{
    if (bit == enumeration->n) {
        enumeration->valid++;
        for (int i = 0; i < enumeration->n; i++) {
            if (assigned & (1u << i))
                enumeration->mine_count[i]++;
        }
        return;
    }
    if (partial_valid(enumeration, bit + 1, assigned))
        enumerate(enumeration, bit + 1, assigned);
    unsigned with_mine = assigned | (1u << bit);
    if (partial_valid(enumeration, bit + 1, with_mine))
        enumerate(enumeration, bit + 1, with_mine);
}

static int union_find(int *parent, int item)
{
    while (parent[item] != item) {
        parent[item] = parent[parent[item]];
        item = parent[item];
    }
    return item;
}

static void union_cells(int *parent, int a, int b)
{
    a = union_find(parent, a);
    b = union_find(parent, b);
    if (a != b)
        parent[b] = a;
}

static int cell_id(Cell *cells, int *count, int x, int y)
{
    for (int i = 0; i < *count; i++) {
        if (cells[i].x == x && cells[i].y == y)
            return i;
    }
    if (*count >= HHMS_MAX_TILES)
        return -1;
    cells[*count].x = x;
    cells[*count].y = y;
    return (*count)++;
}
static void mark_limited_component(HhmsAnalysis *analysis, const Cell *cells,
                                   int cell_count, int *parent, int root)
{
    for (int i = 0; i < cell_count; i++) {
        if (union_find(parent, i) != root)
            continue;
        HhmsAnalysisCell *cell = analysis_touch(analysis, cells[i].x, cells[i].y);
        if (cell)
            cell->model_limited = 1;
    }
}


static int apply_enumeration(const HhmsMap *map, HhmsAnalysis *analysis,
                             int write_models, int promote)
{
    static Constraint constraints[HHMS_MAX_TILES];
    static Cell cells[HHMS_MAX_TILES];
    static int cell_map[HHMS_MAX_TILES][8];
    int constraint_count = collect_constraints(map, analysis, constraints);
    if (constraint_count == 0)
        return 0;
    int cell_count = 0;
    memset(cell_map, -1, sizeof(cell_map));
    for (int i = 0; i < constraint_count; i++) {
        for (int k = 0; k < constraints[i].n; k++) {
            int id = cell_id(cells, &cell_count, constraints[i].x[k], constraints[i].y[k]);
            if (id < 0) {
                analysis->limits |= HHMS_LIMIT_ANALYSIS_CELLS;
                analysis->complete = 0;
                return 0;
            }
            cell_map[i][k] = id;
        }
    }
    if (cell_count == 0)
        return 0;

    static int parent[HHMS_MAX_TILES];
    static int seen[HHMS_MAX_TILES];
    for (int i = 0; i < cell_count; i++)
        parent[i] = i;
    memset(seen, 0, sizeof(int) * (size_t)cell_count);
    for (int i = 0; i < constraint_count; i++) {
        int first = cell_map[i][0];
        for (int k = 1; k < constraints[i].n; k++)
            union_cells(parent, first, cell_map[i][k]);
    }

    int changed = 0;
    for (int root_item = 0; root_item < cell_count; root_item++) {
        int root = union_find(parent, root_item);
        if (seen[root])
            continue;
        seen[root] = 1;
        int local[HHMS_ENUM_MAX];
        int local_count = 0;
        int too_large = 0;
        for (int i = 0; i < cell_count; i++) {
            if (union_find(parent, i) != root)
                continue;
            if (local_count >= HHMS_ENUM_MAX) {
                too_large = 1;
                break;
            }
            local[local_count++] = i;
        }
        if (too_large) {
            analysis->limits |= HHMS_LIMIT_ENUM_COMPONENT;
            analysis->complete = 0;
            if (!promote)
                mark_limited_component(analysis, cells, cell_count, parent, root);
            continue;
        }

        Enumeration enumeration;
        memset(&enumeration, 0, sizeof(enumeration));
        enumeration.n = local_count;
        static int global_to_local[HHMS_MAX_TILES];
        memset(global_to_local, -1, sizeof(int) * (size_t)cell_count);
        for (int i = 0; i < local_count; i++)
            global_to_local[local[i]] = i;

        for (int i = 0; i < constraint_count; i++) {
            if (union_find(parent, cell_map[i][0]) != root)
                continue;
            if (enumeration.constraint_count >= HHMS_ENUM_MAX * 8) {
                analysis->limits |= HHMS_LIMIT_ENUM_COMPONENT;
                analysis->complete = 0;
                too_large = 1;
                break;
            }
            unsigned mask = 0;
            for (int k = 0; k < constraints[i].n; k++) {
                int local_index = global_to_local[cell_map[i][k]];
                if (local_index >= 0)
                    mask |= 1u << local_index;
            }
            BitConstraint *bit_constraint =
                &enumeration.constraints[enumeration.constraint_count++];
            bit_constraint->mask = mask;
            bit_constraint->need = constraints[i].need;
            bit_constraint->constraint_index = i;
        }
        if (too_large || enumeration.constraint_count == 0) {
            if (too_large && !promote)
                mark_limited_component(analysis, cells, cell_count, parent, root);
            continue;
        }

        enumerate(&enumeration, 0, 0);
        if (enumeration.valid == 0) {
            for (int i = 0; i < constraint_count; i++) {
                if (union_find(parent, cell_map[i][0]) == root) {
                    mark_conflict(analysis, constraints[i].clue_x,
                                  constraints[i].clue_y, HHMS_REASON_ENUMERATION,
                                  constraints[i].clue_x, constraints[i].clue_y, 0, 0);
                }
            }
            continue;
        }

        int reason_constraint = enumeration.constraints[0].constraint_index;
        for (int i = 0; i < local_count; i++) {
            int x = cells[local[i]].x;
            int y = cells[local[i]].y;
            uint64_t mines = enumeration.mine_count[i];
            if (!promote) {
                if (write_models) {
                    HhmsAnalysisCell *cell = analysis_touch(analysis, x, y);
                    if (cell && cell->mark == HHMS_MARK_NONE) {
                        cell->mine_models = mines;
                        cell->total_models = enumeration.valid;
                        set_reason(cell, HHMS_REASON_ENUMERATION,
                                   constraints[reason_constraint].clue_x,
                                   constraints[reason_constraint].clue_y, 0, 0);
                    }
                }
            } else if (mines == enumeration.valid) {
                changed |= mark_mine(map, analysis, x, y, HHMS_REASON_ENUMERATION,
                                     constraints[reason_constraint].clue_x,
                                     constraints[reason_constraint].clue_y, 0, 0);
            } else if (mines == 0) {
                changed |= mark_safe(map, analysis, x, y, HHMS_REASON_ENUMERATION,
                                     constraints[reason_constraint].clue_x,
                                     constraints[reason_constraint].clue_y, 0, 0);
            } else if (write_models) {
                HhmsAnalysisCell *cell = analysis_touch(analysis, x, y);
                if (cell && cell->mark == HHMS_MARK_NONE &&
                    cell->total_models == 0 && !cell->model_limited) {
                    cell->mine_models = mines;
                    cell->total_models = enumeration.valid;
                    set_reason(cell, HHMS_REASON_ENUMERATION,
                               constraints[reason_constraint].clue_x,
                               constraints[reason_constraint].clue_y, 0, 0);
                }
            }
        }
    }
    return changed;
}

static void fail_closed(HhmsAnalysis *analysis)
{
    for (int i = 0; i < analysis->ncells; i++) {
        HhmsAnalysisCell *cell = &analysis->cells[i];
        if (cell->mark == HHMS_MARK_CONFLICT)
            continue;
        cell->mark = HHMS_MARK_NONE;
        cell->reason = HHMS_REASON_NONE;
        cell->mine_models = 0;
        cell->total_models = 0;
        memset(cell->reason_x, 0, sizeof(cell->reason_x));
        memset(cell->reason_y, 0, sizeof(cell->reason_y));
    }
}

void hhms_solve(const HhmsMap *map, HhmsAnalysis *analysis)
{
    hhms_analysis_init(analysis);
    /* Preserve model totals from the original component before deductions shrink it. */
    apply_enumeration(map, analysis, 1, 0);
    for (;;) {
        int changed;
        do {
            changed = apply_simple(map, analysis);
            changed |= apply_subset(map, analysis);
        } while (changed);
        if (!apply_enumeration(map, analysis, 0, 1))
            break;
    }
    apply_enumeration(map, analysis, 1, 1);
    analysis->complete = analysis->limits == HHMS_LIMIT_NONE;
    if (analysis->contradiction)
        fail_closed(analysis);
}
