#include "hhms.h"

#include <math.h>
#include <string.h>

static const int DX[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
static const int DY[8] = {-1, -1, -1, 0, 0, 1, 1, 1};

static int valid_coord(int x, int y)
{
    return x >= -HHMS_COORD_LIMIT && x <= HHMS_COORD_LIMIT &&
           y >= -HHMS_COORD_LIMIT && y <= HHMS_COORD_LIMIT;
}

static unsigned hash_xy(int x, int y)
{
    unsigned h = (unsigned)x * 0x9E3779B9u ^ (unsigned)y * 0x85EBCA6Bu;
    return h & (HHMS_HASH - 1);
}


static int hash_find(const HhmsSlot *hash, int x, int y)
{
    unsigned h = hash_xy(x, y);
    for (int n = 0; n < HHMS_HASH; n++) {
        const HhmsSlot *s = &hash[h];
        if (s->occ == 0)
            return -1;
        if (s->occ == 1 && s->x == x && s->y == y)
            return s->ti;
        h = (h + 1) & (HHMS_HASH - 1);
    }
    return -1;
}

static int hash_put(HhmsSlot *hash, int x, int y, int ti)
{
    unsigned h = hash_xy(x, y);
    int tomb = -1;
    for (int n = 0; n < HHMS_HASH; n++) {
        HhmsSlot *s = &hash[h];
        if (s->occ == 1 && s->x == x && s->y == y) {
            s->ti = ti;
            return 0;
        }
        if (s->occ == 2 && tomb < 0)
            tomb = (int)h;
        if (s->occ == 0) {
            int at = tomb >= 0 ? tomb : (int)h;
            hash[at].occ = 1;
            hash[at].x = x;
            hash[at].y = y;
            hash[at].ti = ti;
            return 0;
        }
        h = (h + 1) & (HHMS_HASH - 1);
    }
    if (tomb >= 0) {
        hash[tomb].occ = 1;
        hash[tomb].x = x;
        hash[tomb].y = y;
        hash[tomb].ti = ti;
        return 0;
    }
    return -1;
}

static void hash_del(HhmsSlot *hash, int x, int y)
{
    unsigned h = hash_xy(x, y);
    for (int n = 0; n < HHMS_HASH; n++) {
        HhmsSlot *s = &hash[h];
        if (s->occ == 0)
            return;
        if (s->occ == 1 && s->x == x && s->y == y) {
            s->occ = 2;
            return;
        }
        h = (h + 1) & (HHMS_HASH - 1);
    }
}

void hhms_history_clear(HhmsMap *m)
{
    memset(&m->history, 0, sizeof(m->history));
    m->history.next_state = 1;
}

void hhms_init(HhmsMap *m)
{
    memset(m, 0, sizeof(*m));
    m->next_support_id = 1;
    m->history.next_state = 1;
}

const HhmsTile *hhms_get(const HhmsMap *m, int x, int y)
{
    int ti = hash_find(m->hash, x, y);
    return ti >= 0 ? &m->tiles[ti] : NULL;
}

static HhmsTile *get_mut(HhmsMap *m, int x, int y)
{
    int ti = hash_find(m->hash, x, y);
    return ti >= 0 ? &m->tiles[ti] : NULL;
}

static HhmsTile *touch(HhmsMap *m, int x, int y)
{
    HhmsTile *tile = get_mut(m, x, y);
    if (tile)
        return tile;
    if (m->ntiles >= HHMS_MAX_TILES)
        return NULL;
    int ti = m->ntiles++;
    tile = &m->tiles[ti];
    memset(tile, 0, sizeof(*tile));
    tile->x = x;
    tile->y = y;
    tile->kind = HHMS_UNKNOWN;
    tile->terrain = HHMS_TERRAIN_ROCK;
    if (hash_put(m->hash, x, y, ti) != 0) {
        m->ntiles--;
        return NULL;
    }
    return tile;
}

static void drop_tile(HhmsMap *m, int ti)
{
    hash_del(m->hash, m->tiles[ti].x, m->tiles[ti].y);
    m->tiles[ti] = m->tiles[m->ntiles - 1];
    m->ntiles--;
    if (ti < m->ntiles)
        hash_put(m->hash, m->tiles[ti].x, m->tiles[ti].y, ti);
}

static HhmsTileState tile_state(const HhmsMap *m, int x, int y)
{
    HhmsTileState state;
    memset(&state, 0, sizeof(state));
    const HhmsTile *tile = hhms_get(m, x, y);
    if (tile) {
        state.had = 1;
        state.kind = tile->kind;
        state.count = tile->count;
        state.terrain = tile->terrain;
    } else {
        state.kind = HHMS_UNKNOWN;
        state.terrain = HHMS_TERRAIN_ROCK;
    }
    return state;
}

static int tile_state_equal(HhmsTileState a, HhmsTileState b)
{
    return a.had == b.had && (!a.had ||
           (a.kind == b.kind && a.count == b.count && a.terrain == b.terrain));
}

static int apply_tile_state(HhmsMap *m, int x, int y, HhmsTileState state)
{
    HhmsTile *tile = get_mut(m, x, y);
    if (!state.had) {
        if (tile)
            drop_tile(m, (int)(tile - m->tiles));
        return 1;
    }
    if (!tile) {
        tile = touch(m, x, y);
        if (!tile)
            return 0;
    }
    tile->kind = state.kind;
    tile->count = state.kind == HHMS_CLEAR ? state.count : 0;
    tile->terrain = state.terrain;
    return 1;
}

int hhms_support_index_by_id(const HhmsMap *m, uint32_t id)
{
    if (id == 0)
        return -1;
    for (int i = 0; i < m->nsupports; i++) {
        if (m->supports[i].id == id)
            return i;
    }
    return -1;
}

static HhmsSupportState support_state(const HhmsMap *m, uint32_t id)
{
    HhmsSupportState state;
    memset(&state, 0, sizeof(state));
    int index = hhms_support_index_by_id(m, id);
    if (index >= 0) {
        state.had = 1;
        state.support = m->supports[index];
    }
    return state;
}

static int support_state_equal(HhmsSupportState a, HhmsSupportState b)
{
    if (a.had != b.had)
        return 0;
    if (!a.had)
        return 1;
    return a.support.id == b.support.id &&
           a.support.x == b.support.x && a.support.y == b.support.y &&
           a.support.kind == b.support.kind &&
           a.support.orientation == b.support.orientation;
}

static int apply_support_state(HhmsMap *m, uint32_t id, HhmsSupportState state)
{
    int index = hhms_support_index_by_id(m, id);
    if (!state.had) {
        if (index >= 0) {
            m->supports[index] = m->supports[m->nsupports - 1];
            m->nsupports--;
        }
        return 1;
    }
    if (index < 0) {
        if (m->nsupports >= HHMS_MAX_SUPPORTS)
            return 0;
        index = m->nsupports++;
    }
    m->supports[index] = state.support;
    if (state.support.id >= m->next_support_id)
        m->next_support_id = state.support.id + 1;
    if (m->next_support_id == 0)
        m->next_support_id = 1;
    return 1;
}

static void discard_redo(HhmsHistory *history)
{
    if (history->cursor >= history->ntransactions)
        return;
    history->nchanges = history->transactions[history->cursor].first;
    history->ntransactions = history->cursor;
}

static void drop_oldest_transaction(HhmsHistory *history)
{
    if (history->ntransactions <= 0)
        return;
    int count = history->transactions[0].count;
    if (count < history->nchanges) {
        memmove(history->changes, history->changes + count,
                sizeof(history->changes[0]) * (size_t)(history->nchanges - count));
    }
    history->nchanges -= count;
    if (history->ntransactions > 1) {
        memmove(history->transactions, history->transactions + 1,
                sizeof(history->transactions[0]) * (size_t)(history->ntransactions - 1));
    }
    history->ntransactions--;
    if (history->cursor > 0)
        history->cursor--;
    for (int i = 0; i < history->ntransactions; i++)
        history->transactions[i].first -= count;
}

int hhms_begin_edit(HhmsMap *m)
{
    HhmsHistory *history = &m->history;
    if (history->active)
        return 0;
    history->active = 1;
    history->nactive_changes = 0;
    return 1;
}

static HhmsEditResult record_tile_change(HhmsMap *m, int x, int y,
                                         HhmsTileState before, HhmsTileState after)
{
    HhmsHistory *history = &m->history;
    if (!history->active)
        return HHMS_EDIT_OK;
    for (int i = 0; i < history->nactive_changes; i++) {
        HhmsChange *change = &history->active_changes[i];
        if (change->kind == HHMS_CHANGE_TILE &&
            change->as.tile.x == x && change->as.tile.y == y) {
            change->as.tile.after = after;
            return HHMS_EDIT_OK;
        }
    }
    if (history->nactive_changes >= HHMS_MAX_ACTIVE_CHANGES)
        return HHMS_EDIT_HISTORY_FULL;
    HhmsChange *change = &history->active_changes[history->nactive_changes++];
    memset(change, 0, sizeof(*change));
    change->kind = HHMS_CHANGE_TILE;
    change->as.tile.x = x;
    change->as.tile.y = y;
    change->as.tile.before = before;
    change->as.tile.after = after;
    return HHMS_EDIT_OK;
}

static HhmsEditResult record_support_change(HhmsMap *m, uint32_t id,
                                            HhmsSupportState before,
                                            HhmsSupportState after)
{
    HhmsHistory *history = &m->history;
    if (!history->active)
        return HHMS_EDIT_OK;
    for (int i = 0; i < history->nactive_changes; i++) {
        HhmsChange *change = &history->active_changes[i];
        if (change->kind == HHMS_CHANGE_SUPPORT && change->as.support.id == id) {
            change->as.support.after = after;
            return HHMS_EDIT_OK;
        }
    }
    if (history->nactive_changes >= HHMS_MAX_ACTIVE_CHANGES)
        return HHMS_EDIT_HISTORY_FULL;
    HhmsChange *change = &history->active_changes[history->nactive_changes++];
    memset(change, 0, sizeof(*change));
    change->kind = HHMS_CHANGE_SUPPORT;
    change->as.support.id = id;
    change->as.support.before = before;
    change->as.support.after = after;
    return HHMS_EDIT_OK;
}

int hhms_commit_edit(HhmsMap *m)
{
    HhmsHistory *history = &m->history;
    if (!history->active)
        return 0;
    int write = 0;
    for (int i = 0; i < history->nactive_changes; i++) {
        HhmsChange *change = &history->active_changes[i];
        int same = change->kind == HHMS_CHANGE_TILE
            ? tile_state_equal(change->as.tile.before, change->as.tile.after)
            : support_state_equal(change->as.support.before, change->as.support.after);
        if (!same)
            history->active_changes[write++] = *change;
    }
    history->active = 0;
    history->nactive_changes = write;
    if (write == 0)
        return 0;

    discard_redo(history);
    while (history->ntransactions >= HHMS_MAX_HISTORY ||
           history->nchanges + write > HHMS_MAX_HISTORY_CHANGES) {
        if (history->ntransactions <= 0)
            break;
        drop_oldest_transaction(history);
    }
    if (history->nchanges + write > HHMS_MAX_HISTORY_CHANGES) {
        history->nactive_changes = 0;
        return 0;
    }

    HhmsTransaction *transaction = &history->transactions[history->ntransactions++];
    transaction->first = history->nchanges;
    transaction->count = write;
    transaction->before_state = history->current_state;
    transaction->after_state = history->next_state++;
    if (history->next_state == 0)
        history->next_state = 1;
    memcpy(history->changes + history->nchanges, history->active_changes,
           sizeof(history->changes[0]) * (size_t)write);
    history->nchanges += write;
    history->nactive_changes = 0;
    history->current_state = transaction->after_state;
    history->cursor = history->ntransactions;
    return 1;
}

static int apply_change_before(HhmsMap *m, const HhmsChange *change)
{
    if (change->kind == HHMS_CHANGE_TILE)
        return apply_tile_state(m, change->as.tile.x, change->as.tile.y,
                                change->as.tile.before);
    return apply_support_state(m, change->as.support.id, change->as.support.before);
}

static int apply_change_after(HhmsMap *m, const HhmsChange *change)
{
    if (change->kind == HHMS_CHANGE_TILE)
        return apply_tile_state(m, change->as.tile.x, change->as.tile.y,
                                change->as.tile.after);
    return apply_support_state(m, change->as.support.id, change->as.support.after);
}

void hhms_cancel_edit(HhmsMap *m)
{
    HhmsHistory *history = &m->history;
    if (!history->active)
        return;
    for (int i = history->nactive_changes - 1; i >= 0; i--)
        apply_change_before(m, &history->active_changes[i]);
    history->nactive_changes = 0;
    history->active = 0;
}

int hhms_undo(HhmsMap *m)
{
    HhmsHistory *history = &m->history;
    if (history->active || history->cursor <= 0)
        return 0;
    const HhmsTransaction *transaction = &history->transactions[history->cursor - 1];
    for (int i = transaction->first + transaction->count - 1;
         i >= transaction->first; i--) {
        if (!apply_change_before(m, &history->changes[i]))
            return 0;
    }
    history->cursor--;
    history->current_state = transaction->before_state;
    return 1;
}

int hhms_redo(HhmsMap *m)
{
    HhmsHistory *history = &m->history;
    if (history->active || history->cursor >= history->ntransactions)
        return 0;
    const HhmsTransaction *transaction = &history->transactions[history->cursor];
    for (int i = transaction->first; i < transaction->first + transaction->count; i++) {
        if (!apply_change_after(m, &history->changes[i]))
            return 0;
    }
    history->cursor++;
    history->current_state = transaction->after_state;
    return 1;
}

int hhms_can_undo(const HhmsMap *m)
{
    return !m->history.active && m->history.cursor > 0;
}

int hhms_can_redo(const HhmsMap *m)
{
    return !m->history.active && m->history.cursor < m->history.ntransactions;
}

uint64_t hhms_state_id(const HhmsMap *m)
{
    return m->history.current_state;
}

HhmsEditResult hhms_set_tile(HhmsMap *m, int x, int y, HhmsKind kind, int count)
{
    if (!valid_coord(x, y) || kind < HHMS_UNKNOWN || kind > HHMS_OPEN)
        return HHMS_EDIT_INVALID;
    if (kind == HHMS_CLEAR && (count < 0 || count > 8))
        return HHMS_EDIT_INVALID;
    const HhmsTile *old_tile = hhms_get(m, x, y);
    if (old_tile && old_tile->terrain == HHMS_TERRAIN_WATER &&
        (kind == HHMS_CLEAR || kind == HHMS_OPEN))
        return HHMS_EDIT_INVALID;
    HhmsTileState before = tile_state(m, x, y);
    HhmsTileState after = before;
    after.had = kind != HHMS_UNKNOWN || before.terrain != HHMS_TERRAIN_ROCK;
    after.kind = kind;
    after.count = kind == HHMS_CLEAR ? count : 0;
    if (!after.had)
        after.terrain = HHMS_TERRAIN_ROCK;
    if (tile_state_equal(before, after))
        return HHMS_EDIT_NO_CHANGE;
    if (!apply_tile_state(m, x, y, after))
        return HHMS_EDIT_MAP_FULL;
    HhmsEditResult result = record_tile_change(m, x, y, before, after);
    if (result != HHMS_EDIT_OK)
        apply_tile_state(m, x, y, before);
    return result;
}

HhmsEditResult hhms_set_terrain(HhmsMap *m, int x, int y, HhmsTerrain terrain)
{
    if (!valid_coord(x, y) || terrain < HHMS_TERRAIN_ROCK ||
        terrain > HHMS_TERRAIN_WATER)
        return HHMS_EDIT_INVALID;
    HhmsTileState before = tile_state(m, x, y);
    if (terrain == HHMS_TERRAIN_WATER &&
        (before.kind == HHMS_CLEAR || before.kind == HHMS_OPEN))
        return HHMS_EDIT_INVALID;
    HhmsTileState after = before;
    after.terrain = terrain;
    after.had = terrain != HHMS_TERRAIN_ROCK || after.kind != HHMS_UNKNOWN;
    if (tile_state_equal(before, after))
        return HHMS_EDIT_NO_CHANGE;
    if (!apply_tile_state(m, x, y, after))
        return HHMS_EDIT_MAP_FULL;
    HhmsEditResult result = record_tile_change(m, x, y, before, after);
    if (result != HHMS_EDIT_OK)
        apply_tile_state(m, x, y, before);
    return result;
}

HhmsEditResult hhms_erase_tile(HhmsMap *m, int x, int y)
{
    if (!valid_coord(x, y))
        return HHMS_EDIT_INVALID;
    HhmsTileState before = tile_state(m, x, y);
    if (!before.had)
        return HHMS_EDIT_NO_CHANGE;
    HhmsTileState after;
    memset(&after, 0, sizeof(after));
    after.kind = HHMS_UNKNOWN;
    after.terrain = HHMS_TERRAIN_ROCK;
    apply_tile_state(m, x, y, after);
    HhmsEditResult result = record_tile_change(m, x, y, before, after);
    if (result != HHMS_EDIT_OK)
        apply_tile_state(m, x, y, before);
    return result;
}

static int valid_support(double x, double y, HhmsSupportKind kind,
                         HhmsOrientation orientation)
{
    return isfinite(x) && isfinite(y) &&
           x >= -HHMS_COORD_LIMIT && x <= HHMS_COORD_LIMIT &&
           y >= -HHMS_COORD_LIMIT && y <= HHMS_COORD_LIMIT &&
           kind >= HHMS_SUP_WOOD && kind < HHMS_SUP_COUNT &&
           orientation >= HHMS_ORIENT_NORTH && orientation <= HHMS_ORIENT_WEST;
}

HhmsEditResult hhms_add_support(HhmsMap *m, double x, double y,
                                HhmsSupportKind kind, HhmsOrientation orientation,
                                uint32_t *id_out)
{
    if (!valid_support(x, y, kind, orientation))
        return HHMS_EDIT_INVALID;
    if (m->nsupports >= HHMS_MAX_SUPPORTS)
        return HHMS_EDIT_SUPPORTS_FULL;
    uint32_t id = m->next_support_id++;
    if (id == 0)
        id = m->next_support_id++;
    HhmsSupportState before;
    memset(&before, 0, sizeof(before));
    HhmsSupportState after;
    memset(&after, 0, sizeof(after));
    after.had = 1;
    after.support.id = id;
    after.support.x = x;
    after.support.y = y;
    after.support.kind = kind;
    after.support.orientation = orientation;
    if (!apply_support_state(m, id, after))
        return HHMS_EDIT_SUPPORTS_FULL;
    HhmsEditResult result = record_support_change(m, id, before, after);
    if (result != HHMS_EDIT_OK)
        apply_support_state(m, id, before);
    else if (id_out)
        *id_out = id;
    return result;
}

HhmsEditResult hhms_update_support(HhmsMap *m, uint32_t id, double x, double y,
                                   HhmsSupportKind kind, HhmsOrientation orientation)
{
    if (!valid_support(x, y, kind, orientation))
        return HHMS_EDIT_INVALID;
    HhmsSupportState before = support_state(m, id);
    if (!before.had)
        return HHMS_EDIT_INVALID;
    HhmsSupportState after = before;
    after.support.x = x;
    after.support.y = y;
    after.support.kind = kind;
    after.support.orientation = orientation;
    if (support_state_equal(before, after))
        return HHMS_EDIT_NO_CHANGE;
    apply_support_state(m, id, after);
    HhmsEditResult result = record_support_change(m, id, before, after);
    if (result != HHMS_EDIT_OK)
        apply_support_state(m, id, before);
    return result;
}

HhmsEditResult hhms_del_support(HhmsMap *m, uint32_t id)
{
    HhmsSupportState before = support_state(m, id);
    if (!before.had)
        return HHMS_EDIT_NO_CHANGE;
    HhmsSupportState after;
    memset(&after, 0, sizeof(after));
    apply_support_state(m, id, after);
    HhmsEditResult result = record_support_change(m, id, before, after);
    if (result != HHMS_EDIT_OK)
        apply_support_state(m, id, before);
    return result;
}

int hhms_support_index_near(const HhmsMap *m, double x, double y, double max_distance)
{
    int best = -1;
    double best_d2 = max_distance * max_distance;
    for (int i = 0; i < m->nsupports; i++) {
        double dx = x - m->supports[i].x;
        double dy = y - m->supports[i].y;
        double d2 = dx * dx + dy * dy;
        if (d2 <= best_d2) {
            best = i;
            best_d2 = d2;
        }
    }
    return best;
}

double hhms_support_radius(HhmsSupportKind kind)
{
    switch (kind) {
    case HHMS_SUP_WOOD: return 100.0 / 11.0;
    case HHMS_SUP_STONE: return 125.0 / 11.0;
    case HHMS_SUP_BEAM: return 150.0 / 11.0;
    case HHMS_SUP_MONUMENT: return 30.0;
    default: return 0.0;
    }
}

const char *hhms_support_name(HhmsSupportKind kind)
{
    switch (kind) {
    case HHMS_SUP_WOOD: return "wood";
    case HHMS_SUP_STONE: return "stone";
    case HHMS_SUP_BEAM: return "beam";
    case HHMS_SUP_MONUMENT: return "monument";
    case HHMS_SUP_TIMBER_TUNNEL: return "timber_tunnel";
    case HHMS_SUP_REINFORCED_TUNNEL: return "reinforced_tunnel";
    case HHMS_SUP_STONE_ARCH: return "stone_arch";
    default: return NULL;
    }
}

int hhms_support_kind_from_name(const char *name, HhmsSupportKind *kind)
{
    if (!name || !kind)
        return 0;
    for (int i = 0; i < HHMS_SUP_COUNT; i++) {
        const char *candidate = hhms_support_name((HhmsSupportKind)i);
        if (candidate && strcmp(name, candidate) == 0) {
            *kind = (HhmsSupportKind)i;
            return 1;
        }
    }
    return 0;
}

int hhms_support_is_directional(HhmsSupportKind kind)
{
    return kind == HHMS_SUP_TIMBER_TUNNEL ||
           kind == HHMS_SUP_REINFORCED_TUNNEL ||
           kind == HHMS_SUP_STONE_ARCH;
}

void hhms_support_estimated_size(HhmsSupportKind kind, double *width, double *length)
{
    double w = 0.0;
    double l = 0.0;
    if (kind == HHMS_SUP_TIMBER_TUNNEL) { w = 1.0; l = 4.0; }
    else if (kind == HHMS_SUP_REINFORCED_TUNNEL) { w = 2.0; l = 8.0; }
    else if (kind == HHMS_SUP_STONE_ARCH) { w = 3.0; l = 15.0; }
    if (width) *width = w;
    if (length) *length = l;
}

HhmsCoverKind hhms_support_covers(const HhmsSupport *support, int x, int y,
                                  double *margin)
{
    if (!support)
        return HHMS_COVER_NONE;
    double dx = (double)x - support->x;
    double dy = (double)y - support->y;
    if (!hhms_support_is_directional(support->kind)) {
        double radius = hhms_support_radius(support->kind);
        double distance = sqrt(dx * dx + dy * dy);
        double edge = radius - distance;
        if (margin) *margin = edge;
        return edge >= -1e-9 ? HHMS_COVER_CONFIRMED : HHMS_COVER_NONE;
    }
    double width, length;
    hhms_support_estimated_size(support->kind, &width, &length);
    int vertical = support->orientation == HHMS_ORIENT_NORTH ||
                   support->orientation == HHMS_ORIENT_SOUTH;
    double across = fabs(vertical ? dx : dy);
    double along = fabs(vertical ? dy : dx);
    double edge = fmin(width * 0.5 - across, length * 0.5 - along);
    if (margin) *margin = edge;
    return edge >= -1e-9 ? HHMS_COVER_ESTIMATED : HHMS_COVER_NONE;
}

HhmsCoverage hhms_coverage(const HhmsMap *m, int x, int y, uint32_t exclude_id)
{
    HhmsCoverage coverage;
    memset(&coverage, 0, sizeof(coverage));
    coverage.nearest_margin = -1.0;
    for (int i = 0; i < m->nsupports; i++) {
        const HhmsSupport *support = &m->supports[i];
        if (support->id == exclude_id)
            continue;
        double margin = 0.0;
        HhmsCoverKind kind = hhms_support_covers(support, x, y, &margin);
        if (kind == HHMS_COVER_CONFIRMED) {
            coverage.confirmed_count++;
            if (coverage.primary_confirmed == 0 || margin > coverage.nearest_margin) {
                coverage.primary_confirmed = support->id;
                coverage.nearest_margin = margin;
            }
        } else if (kind == HHMS_COVER_ESTIMATED) {
            coverage.estimated_count++;
            if (coverage.primary_estimated == 0)
                coverage.primary_estimated = support->id;
        }
    }
    return coverage;
}

static int observed_mine(const HhmsMap *m, int x, int y)
{
    const HhmsTile *tile = hhms_get(m, x, y);
    return tile && tile->kind == HHMS_MINE;
}

static int derived_mine(const HhmsAnalysis *a, int x, int y)
{
    const HhmsAnalysisCell *cell = hhms_analysis_get(a, x, y);
    return cell && cell->mark == HHMS_MARK_MINE;
}

int hhms_support_exposed_caves(const HhmsMap *m, const HhmsAnalysis *a,
                               uint32_t support_id)
{
    int index = hhms_support_index_by_id(m, support_id);
    if (index < 0 || hhms_support_is_directional(m->supports[index].kind))
        return 0;
    int exposed = 0;
    for (int i = 0; i < m->ntiles; i++) {
        const HhmsTile *tile = &m->tiles[i];
        if (tile->kind != HHMS_MINE)
            continue;
        if (hhms_support_covers(&m->supports[index], tile->x, tile->y, NULL) == HHMS_COVER_CONFIRMED &&
            hhms_coverage(m, tile->x, tile->y, support_id).confirmed_count == 0)
            exposed++;
    }
    if (!a || a->contradiction)
        return exposed;
    for (int i = 0; i < a->ncells; i++) {
        const HhmsAnalysisCell *cell = &a->cells[i];
        if (cell->mark != HHMS_MARK_MINE || observed_mine(m, cell->x, cell->y))
            continue;
        if (hhms_support_covers(&m->supports[index], cell->x, cell->y, NULL) == HHMS_COVER_CONFIRMED &&
            hhms_coverage(m, cell->x, cell->y, support_id).confirmed_count == 0)
            exposed++;
    }
    return exposed;
}

static int known_mine(const HhmsMap *m, const HhmsAnalysis *a, int x, int y)
{
    if (observed_mine(m, x, y))
        return 1;
    return a && !a->contradiction && derived_mine(a, x, y);
}

static int known_safe(const HhmsMap *m, const HhmsAnalysis *a, int x, int y)
{
    const HhmsTile *tile = hhms_get(m, x, y);
    if (tile && (tile->kind == HHMS_CLEAR || tile->kind == HHMS_OPEN))
        return 1;
    if (!a || a->contradiction)
        return 0;
    const HhmsAnalysisCell *cell = hhms_analysis_get(a, x, y);
    return cell && cell->mark == HHMS_MARK_SAFE;
}

HhmsCellInfo hhms_cell_info(const HhmsMap *m, const HhmsAnalysis *a, int x, int y)
{
    HhmsCellInfo info;
    memset(&info, 0, sizeof(info));
    const HhmsTile *tile = hhms_get(m, x, y);
    info.kind = tile ? tile->kind : HHMS_UNKNOWN;
    info.terrain = tile ? tile->terrain : HHMS_TERRAIN_ROCK;
    info.authored = tile && (tile->kind != HHMS_UNKNOWN ||
                             tile->terrain != HHMS_TERRAIN_ROCK);
    info.coverage = hhms_coverage(m, x, y, 0);
    const HhmsAnalysisCell *cell = a ? hhms_analysis_get(a, x, y) : NULL;
    if (cell) {
        info.mark = cell->mark;
        info.reason = cell->reason;
        info.mine_models = cell->mine_models;
        info.total_models = cell->total_models;
    }
    if (cell && cell->mark == HHMS_MARK_CONFLICT) {
        info.action = HHMS_ACTION_CONFLICT;
    } else if (info.kind == HHMS_CLEAR) {
        info.action = HHMS_ACTION_CLUE;
    } else if (info.kind == HHMS_OPEN) {
        info.action = HHMS_ACTION_OPEN;
    } else if (info.kind == HHMS_MINE) {
        info.action = info.coverage.confirmed_count > 0
            ? HHMS_ACTION_PROTECTED_CAVE : HHMS_ACTION_CAVE;
    } else if (cell && !a->contradiction && cell->mark == HHMS_MARK_SAFE) {
        info.action = info.terrain == HHMS_TERRAIN_WATER
            ? HHMS_ACTION_SAFE_WATER : HHMS_ACTION_DIG;
    } else if (cell && !a->contradiction && cell->mark == HHMS_MARK_MINE) {
        info.action = info.coverage.confirmed_count > 0
            ? HHMS_ACTION_PROTECTED_CAVE : HHMS_ACTION_CAVE;
    } else if (cell && !a->contradiction && cell->total_models > 0 &&
               cell->mine_models > 0 && cell->mine_models < cell->total_models) {
        info.action = info.terrain == HHMS_TERRAIN_WATER
            ? HHMS_ACTION_ODDS_WATER : HHMS_ACTION_ODDS_ROCK;
    } else {
        info.action = info.terrain == HHMS_TERRAIN_WATER
            ? HHMS_ACTION_UNKNOWN_WATER : HHMS_ACTION_UNKNOWN_ROCK;
    }
    return info;
}

HhmsConstraintSummary hhms_constraint_summary(const HhmsMap *m,
                                               const HhmsAnalysis *a, int x, int y)
{
    HhmsConstraintSummary summary;
    memset(&summary, 0, sizeof(summary));
    const HhmsTile *clue = hhms_get(m, x, y);
    if (!clue || clue->kind != HHMS_CLEAR)
        return summary;
    int mines = 0;
    int unknown = 0;
    for (int i = 0; i < 8; i++) {
        int nx = x + DX[i];
        int ny = y + DY[i];
        if (known_mine(m, a, nx, ny))
            mines++;
        else if (!known_safe(m, a, nx, ny))
            unknown++;
    }
    summary.remaining_cells = unknown;
    summary.remaining_mines = clue->count - mines;
    summary.valid = summary.remaining_mines >= 0 &&
                    summary.remaining_mines <= summary.remaining_cells;
    return summary;
}

static int unresolved_cell(const HhmsMap *m, const HhmsAnalysis *a, int x, int y)
{
    HhmsCellInfo info = hhms_cell_info(m, a, x, y);
    return info.action == HHMS_ACTION_UNKNOWN_ROCK ||
           info.action == HHMS_ACTION_UNKNOWN_WATER ||
           info.action == HHMS_ACTION_ODDS_ROCK ||
           info.action == HHMS_ACTION_ODDS_WATER;
}

static int adjacent8(int x0, int y0, int x1, int y1)
{
    int dx = x0 - x1;
    int dy = y0 - y1;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return dx <= 1 && dy <= 1 && (dx || dy);
}

HhmsLinkRole hhms_link_role(const HhmsMap *m, const HhmsAnalysis *a,
                            int hover_x, int hover_y, int x, int y)
{
    if (hover_x == x && hover_y == y)
        return HHMS_LINK_NONE;
    const HhmsTile *hover = hhms_get(m, hover_x, hover_y);
    if (hover && hover->kind == HHMS_CLEAR) {
        return adjacent8(hover_x, hover_y, x, y) && unresolved_cell(m, a, x, y)
            ? HHMS_LINK_FRONTIER : HHMS_LINK_NONE;
    }
    if (!unresolved_cell(m, a, hover_x, hover_y))
        return HHMS_LINK_NONE;
    for (int i = 0; i < 8; i++) {
        int cx = hover_x + DX[i];
        int cy = hover_y + DY[i];
        const HhmsTile *clue = hhms_get(m, cx, cy);
        if (!clue || clue->kind != HHMS_CLEAR)
            continue;
        if (x == cx && y == cy)
            return HHMS_LINK_CLUE;
        if (adjacent8(cx, cy, x, y) && unresolved_cell(m, a, x, y))
            return HHMS_LINK_FRONTIER;
    }
    return HHMS_LINK_NONE;
}

int hhms_bounds(const HhmsMap *m, double *min_x, double *min_y,
                double *max_x, double *max_y)
{
    if (m->ntiles == 0 && m->nsupports == 0)
        return 0;
    double lo_x = 0.0, lo_y = 0.0, hi_x = 0.0, hi_y = 0.0;
    int have = 0;
    for (int i = 0; i < m->ntiles; i++) {
        double x = m->tiles[i].x;
        double y = m->tiles[i].y;
        if (!have) { lo_x = hi_x = x; lo_y = hi_y = y; have = 1; }
        else {
            if (x < lo_x) lo_x = x;
            if (x > hi_x) hi_x = x;
            if (y < lo_y) lo_y = y;
            if (y > hi_y) hi_y = y;
        }
    }
    for (int i = 0; i < m->nsupports; i++) {
        double x = m->supports[i].x;
        double y = m->supports[i].y;
        if (!have) { lo_x = hi_x = x; lo_y = hi_y = y; have = 1; }
        else {
            if (x < lo_x) lo_x = x;
            if (x > hi_x) hi_x = x;
            if (y < lo_y) lo_y = y;
            if (y > hi_y) hi_y = y;
        }
    }
    if (min_x) *min_x = lo_x;
    if (min_y) *min_y = lo_y;
    if (max_x) *max_x = hi_x;
    if (max_y) *max_y = hi_y;
    return 1;
}
