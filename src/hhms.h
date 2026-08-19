#ifndef HHMS_H
#define HHMS_H

#include <stddef.h>

#define HHMS_MAX_TILES    8192
#define HHMS_MAX_SUPPORTS 256
#define HHMS_MAX_UNDO     512
#define HHMS_HASH         16384
#define HHMS_ENUM_MAX     20

typedef enum {
    HHMS_UNKNOWN = 0,
    HHMS_CLEAR,
    HHMS_MINE,
    HHMS_OPEN
} HhmsKind;

typedef enum {
    HHMS_MARK_NONE = 0,
    HHMS_MARK_SAFE,
    HHMS_MARK_MINE,
    HHMS_MARK_CONFLICT
} HhmsMark;

typedef enum {
    HHMS_SUP_WOOD = 0,
    HHMS_SUP_STONE,
    HHMS_SUP_BEAM,
    HHMS_SUP_MONUMENT
} HhmsSupportKind;

typedef struct {
    int x, y;
    HhmsKind kind;
    int count;
    int user;
    HhmsMark mark;
    float p_mine;
} HhmsTile;

typedef struct {
    int x, y;
    HhmsSupportKind kind;
} HhmsSupport;

typedef enum {
    HHMS_UNDO_TILE = 0,
    HHMS_UNDO_SUP_ADD,
    HHMS_UNDO_SUP_DEL
} HhmsUndoOp;

typedef struct {
    HhmsUndoOp op;
    int x, y;
    HhmsKind kind;
    int count;
    int user;
    int had;
    HhmsSupportKind sup_kind;
} HhmsUndo;

typedef struct {
    int occ;
    int x, y;
    int ti;
} HhmsSlot;

typedef struct {
    HhmsTile tiles[HHMS_MAX_TILES];
    int ntiles;
    HhmsSlot hash[HHMS_HASH];
    HhmsSupport supports[HHMS_MAX_SUPPORTS];
    int nsupports;
    HhmsUndo undo[HHMS_MAX_UNDO];
    int nundo;
    int contradiction;
} HhmsMap;

void hhms_init(HhmsMap *m);

const HhmsTile *hhms_get(const HhmsMap *m, int x, int y);
HhmsTile *hhms_get_mut(HhmsMap *m, int x, int y);

int hhms_set_tile(HhmsMap *m, int x, int y, HhmsKind kind, int count, int record_undo);
int hhms_erase_tile(HhmsMap *m, int x, int y, int record_undo);

int hhms_add_support(HhmsMap *m, int x, int y, HhmsSupportKind kind, int record_undo);
int hhms_del_support_at(HhmsMap *m, int x, int y, int record_undo);
int hhms_support_index(const HhmsMap *m, int x, int y);

int hhms_covered(const HhmsMap *m, int x, int y);
float hhms_support_radius(HhmsSupportKind kind);
const char *hhms_support_name(HhmsSupportKind kind);

void hhms_solve(HhmsMap *m);
int hhms_undo(HhmsMap *m);

int hhms_save(const HhmsMap *m, const char *path);
int hhms_load(HhmsMap *m, const char *path);

void hhms_drop_ephemeral(HhmsMap *m);
HhmsTile *hhms_touch(HhmsMap *m, int x, int y);


#endif
