#ifndef S2H_MOD_API_MOD_LABELS_H
#define S2H_MOD_API_MOD_LABELS_H

#include <stdbool.h>
#include <stdint.h>

// The names SoH's own windows (save editor, gameplay stats, time splits) show
typedef enum {
    S2H_LABEL_ITEM,       // an item id; per language
    S2H_LABEL_QUEST_ITEM, // a quest item; per language
    S2H_LABEL_SCENE,      // a scene id
    S2H_LABEL_TIMESTAMP,  // a gameplay stats timestamp (an item id, or TIMESTAMP_DEFEAT_GOHMA onward)
} S2HLabelKind;

typedef struct {
    // TimeSplits' SplitTypes: SPLIT_TYPE_ITEM (id: an ITEM_*), SPLIT_TYPE_SCENE (a SCENE_*),
    // SPLIT_TYPE_BOSS (the boss's ACTOR_*)
    uint32_t type;
    uint32_t id;      // what the split waits for
    const char* name; // the label (the time splits draw the image for the type and id themselves)
} S2HTimeSplit;

#ifdef __cplusplus
extern "C" {
#endif

bool ModLabels_Set(S2HLabelKind kind, int32_t language, int32_t index, const char* name);
bool ModLabels_SetTimeSplits(const S2HTimeSplit* splits, uint32_t count);
bool ModLabels_SetTimeSplitGroup(uint32_t id, const uint32_t* members, uint32_t count);
void ModLabels_ClearTimeSplitGroups(void);
bool ModLabels_SetItemIcon(uint32_t item, uint32_t source);
bool ModLabels_HasCustomSplits(void);
const char* ModLabels_Get(S2HLabelKind kind, int32_t language, int32_t index);

#ifdef __cplusplus
}
#endif

#endif
