#ifndef HHMS_H
#define HHMS_H

#include <stddef.h>
#include <stdint.h>

#define HHMS_FILE_VERSION         2
#define HHMS_MAX_TILES            8192
#define HHMS_MAX_SUPPORTS         256
#define HHMS_MAX_HISTORY          512
#define HHMS_MAX_HISTORY_CHANGES  16384
#define HHMS_MAX_ACTIVE_CHANGES   (HHMS_MAX_TILES + HHMS_MAX_SUPPORTS)
#define HHMS_HASH                 16384
#define HHMS_ENUM_MAX             20
#define HHMS_COORD_LIMIT          1000000

typedef enum {
    HHMS_UNKNOWN = 0,
    HHMS_CLEAR,
    HHMS_MINE,
    HHMS_OPEN
} HhmsKind;

typedef enum {
    HHMS_TERRAIN_ROCK = 0,
    HHMS_TERRAIN_WATER
} HhmsTerrain;

typedef enum {
    HHMS_MARK_NONE = 0,
    HHMS_MARK_SAFE,
    HHMS_MARK_MINE,
    HHMS_MARK_CONFLICT
} HhmsMark;

typedef enum {
    HHMS_REASON_NONE = 0,
    HHMS_REASON_SIMPLE,
    HHMS_REASON_SUBSET,
    HHMS_REASON_ENUMERATION
} HhmsReason;

typedef enum {
    HHMS_SUP_WOOD = 0,
    HHMS_SUP_STONE,
    HHMS_SUP_BEAM,
    HHMS_SUP_MONUMENT,
    HHMS_SUP_TIMBER_TUNNEL,
    HHMS_SUP_REINFORCED_TUNNEL,
    HHMS_SUP_STONE_ARCH,
    HHMS_SUP_COUNT
} HhmsSupportKind;

typedef enum {
    HHMS_ORIENT_NORTH = 0,
    HHMS_ORIENT_EAST,
    HHMS_ORIENT_SOUTH,
    HHMS_ORIENT_WEST
} HhmsOrientation;

typedef enum {
    HHMS_EDIT_OK = 0,
    HHMS_EDIT_NO_CHANGE,
    HHMS_EDIT_INVALID,
    HHMS_EDIT_MAP_FULL,
    HHMS_EDIT_SUPPORTS_FULL,
    HHMS_EDIT_HISTORY_FULL,
    HHMS_EDIT_BUSY
} HhmsEditResult;

typedef enum {
    HHMS_LIMIT_NONE = 0,
    HHMS_LIMIT_ANALYSIS_CELLS = 1u << 0,
    HHMS_LIMIT_ENUM_COMPONENT = 1u << 1
} HhmsLimit;

typedef enum {
    HHMS_COVER_NONE = 0,
    HHMS_COVER_CONFIRMED,
    HHMS_COVER_ESTIMATED
} HhmsCoverKind;

typedef enum {
    HHMS_ACTION_UNKNOWN_ROCK = 0,
    HHMS_ACTION_UNKNOWN_WATER,
    HHMS_ACTION_CLUE,
    HHMS_ACTION_OPEN,
    HHMS_ACTION_DIG,
    HHMS_ACTION_SAFE_WATER,
    HHMS_ACTION_CAVE,
    HHMS_ACTION_PROTECTED_CAVE,
    HHMS_ACTION_ODDS_ROCK,
    HHMS_ACTION_ODDS_WATER,
    HHMS_ACTION_CONFLICT
} HhmsAction;

typedef enum {
    HHMS_LINK_NONE = 0,
    HHMS_LINK_FRONTIER,
    HHMS_LINK_CLUE
} HhmsLinkRole;

typedef enum {
    HHMS_IO_OK = 0,
    HHMS_IO_OPEN_FAILED,
    HHMS_IO_READ_FAILED,
    HHMS_IO_TOO_LARGE,
    HHMS_IO_OUT_OF_MEMORY,
    HHMS_IO_PARSE_FAILED,
    HHMS_IO_UNSUPPORTED_VERSION,
    HHMS_IO_SCHEMA_FAILED,
    HHMS_IO_RANGE_FAILED,
    HHMS_IO_WRITE_FAILED,
    HHMS_IO_FLUSH_FAILED,
    HHMS_IO_REPLACE_FAILED
} HhmsIoResult;

typedef struct {
    int x, y;
    HhmsKind kind;
    int count;
    HhmsTerrain terrain;
} HhmsTile;

typedef struct {
    uint32_t id;
    double x, y;
    HhmsSupportKind kind;
    HhmsOrientation orientation;
} HhmsSupport;

typedef struct {
    int occ;
    int x, y;
    int ti;
} HhmsSlot;

typedef struct {
    int had;
    HhmsKind kind;
    int count;
    HhmsTerrain terrain;
} HhmsTileState;

typedef struct {
    int had;
    HhmsSupport support;
} HhmsSupportState;

typedef enum {
    HHMS_CHANGE_TILE = 0,
    HHMS_CHANGE_SUPPORT
} HhmsChangeKind;

typedef struct {
    HhmsChangeKind kind;
    union {
        struct {
            int x, y;
            HhmsTileState before, after;
        } tile;
        struct {
            uint32_t id;
            HhmsSupportState before, after;
        } support;
    } as;
} HhmsChange;

typedef struct {
    int first, count;
    uint64_t before_state, after_state;
} HhmsTransaction;

typedef struct {
    HhmsChange changes[HHMS_MAX_HISTORY_CHANGES];
    HhmsChange active_changes[HHMS_MAX_ACTIVE_CHANGES];
    HhmsTransaction transactions[HHMS_MAX_HISTORY];
    int nchanges;
    int nactive_changes;
    int ntransactions;
    int cursor;
    int active;
    uint64_t current_state;
    uint64_t next_state;
} HhmsHistory;

typedef struct {
    HhmsTile tiles[HHMS_MAX_TILES];
    int ntiles;
    HhmsSlot hash[HHMS_HASH];
    HhmsSupport supports[HHMS_MAX_SUPPORTS];
    int nsupports;
    uint32_t next_support_id;
    /* Session-only edit history; persistence serializes authored fields above. */
    HhmsHistory history;
} HhmsMap;

/* Solver output is rebuilt from a const HhmsMap and is never persisted. */
typedef struct {
    int x, y;
    HhmsMark mark;
    HhmsReason reason;
    int model_limited;
    int reason_x[2], reason_y[2];
    uint64_t mine_models;
    uint64_t total_models;
} HhmsAnalysisCell;

typedef struct {
    HhmsAnalysisCell cells[HHMS_MAX_TILES];
    int ncells;
    HhmsSlot hash[HHMS_HASH];
    int contradiction;
    int complete;
    unsigned limits;
} HhmsAnalysis;

typedef struct {
    double camx, camy;
    float cell;
    float ui_scale;
    int has_view;
} HhmsView;

typedef struct {
    HhmsIoResult code;
    int line, column;
    int os_error;
    char detail[192];
} HhmsError;

typedef struct {
    int confirmed_count;
    int estimated_count;
    uint32_t primary_confirmed;
    uint32_t primary_estimated;
    double nearest_margin;
} HhmsCoverage;

typedef struct {
    HhmsKind kind;
    HhmsTerrain terrain;
    HhmsMark mark;
    HhmsReason reason;
    HhmsAction action;
    uint64_t mine_models;
    uint64_t total_models;
    HhmsCoverage coverage;
    int authored;
} HhmsCellInfo;

typedef struct {
    int valid;
    int remaining_cells;
    int remaining_mines;
} HhmsConstraintSummary;

void hhms_init(HhmsMap *m);
void hhms_analysis_init(HhmsAnalysis *a);
void hhms_view_init(HhmsView *v);
void hhms_error_clear(HhmsError *e);

const HhmsTile *hhms_get(const HhmsMap *m, int x, int y);

int hhms_begin_edit(HhmsMap *m);
int hhms_commit_edit(HhmsMap *m);
void hhms_cancel_edit(HhmsMap *m);
int hhms_undo(HhmsMap *m);
int hhms_redo(HhmsMap *m);
int hhms_can_undo(const HhmsMap *m);
int hhms_can_redo(const HhmsMap *m);
uint64_t hhms_state_id(const HhmsMap *m);
void hhms_history_clear(HhmsMap *m);

HhmsEditResult hhms_set_tile(HhmsMap *m, int x, int y, HhmsKind kind, int count);
HhmsEditResult hhms_set_terrain(HhmsMap *m, int x, int y, HhmsTerrain terrain);
HhmsEditResult hhms_erase_tile(HhmsMap *m, int x, int y);

HhmsEditResult hhms_add_support(HhmsMap *m, double x, double y,
                                HhmsSupportKind kind, HhmsOrientation orientation,
                                uint32_t *id_out);
HhmsEditResult hhms_update_support(HhmsMap *m, uint32_t id, double x, double y,
                                   HhmsSupportKind kind, HhmsOrientation orientation);
HhmsEditResult hhms_del_support(HhmsMap *m, uint32_t id);
int hhms_support_index_by_id(const HhmsMap *m, uint32_t id);
int hhms_support_index_near(const HhmsMap *m, double x, double y, double max_distance);

double hhms_support_radius(HhmsSupportKind kind);
const char *hhms_support_name(HhmsSupportKind kind);
int hhms_support_kind_from_name(const char *name, HhmsSupportKind *kind);
int hhms_support_is_directional(HhmsSupportKind kind);
void hhms_support_estimated_size(HhmsSupportKind kind, double *width, double *length);
HhmsCoverKind hhms_support_covers(const HhmsSupport *support, int x, int y,
                                  double *margin);
HhmsCoverage hhms_coverage(const HhmsMap *m, int x, int y, uint32_t exclude_id);
int hhms_support_exposed_caves(const HhmsMap *m, const HhmsAnalysis *a,
                               uint32_t support_id);

void hhms_solve(const HhmsMap *m, HhmsAnalysis *a);
const HhmsAnalysisCell *hhms_analysis_get(const HhmsAnalysis *a, int x, int y);
HhmsCellInfo hhms_cell_info(const HhmsMap *m, const HhmsAnalysis *a, int x, int y);
HhmsConstraintSummary hhms_constraint_summary(const HhmsMap *m,
                                               const HhmsAnalysis *a, int x, int y);
HhmsLinkRole hhms_link_role(const HhmsMap *m, const HhmsAnalysis *a,
                            int hover_x, int hover_y, int x, int y);

int hhms_bounds(const HhmsMap *m, double *min_x, double *min_y,
                double *max_x, double *max_y);

HhmsIoResult hhms_save(const HhmsMap *m, const HhmsView *view,
                       const char *path, HhmsError *error);
HhmsIoResult hhms_load(HhmsMap *m, HhmsView *view,
                       const char *path, HhmsError *error);
const char *hhms_io_result_name(HhmsIoResult result);

#endif
