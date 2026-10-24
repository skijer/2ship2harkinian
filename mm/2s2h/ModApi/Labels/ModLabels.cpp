#include "ModLabels.h"
#include <algorithm>
#include <array>
#include <map>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>
#include "2s2h/Enhancements/Trackers/TimeSplits/Timesplits.h"
extern "C" {
#include "z64.h"
#include "variables.h"
extern TexturePtr gItemIcons[131];
}
extern std::unordered_map<s16, const char*> sceneNames;
namespace {
std::map<std::tuple<int32_t, int32_t, int32_t>, std::string> labels;
std::array<TexturePtr, 131> originalIcons{};
bool haveIcons = false;
bool customSplits = false;
}

bool ModLabels_HasCustomSplits() { return customSplits; }

const char* ModLabels_Get(S2HLabelKind kind, int32_t language, int32_t index) {
    auto found = labels.find({kind, language, index});
    if (found == labels.end()) found = labels.find({kind, -1, index});
    return found == labels.end() ? nullptr : found->second.c_str();
}

bool ModLabels_Set(S2HLabelKind kind, int32_t language, int32_t index, const char* name) {
    if (name == nullptr || index < 0 || kind < S2H_LABEL_ITEM || kind > S2H_LABEL_TIMESTAMP ||
        (kind == S2H_LABEL_SCENE && index > INT16_MAX)) return false;
    labels[{kind, language, index}] = name;
    const char* stored = labels.at({kind, language, index}).c_str();
    if (kind == S2H_LABEL_SCENE) {
        sceneNames[static_cast<s16>(index)] = stored;
        for (auto& split : sceneObjectList) if (split.splitId == static_cast<uint32_t>(index)) split.splitName = stored;
    } else {
        for (auto& split : splitObjectList) if (split.splitId == static_cast<uint32_t>(index)) split.splitName = stored;
    }
    return true;
}

bool ModLabels_SetTimeSplits(const S2HTimeSplit* splits, uint32_t count) {
    if (splits == nullptr || count == 0 || count > 4096) return false;
    std::vector<TimesplitObject> items, scenes;
    for (uint32_t i = 0; i < count; ++i) {
        if (splits[i].type > SPLIT_TYPE_BOSS) return false;
        TimesplitObject object{};
        object.splitType = splits[i].type;
        object.splitId = splits[i].id;
        object.splitName = splits[i].name != nullptr ? splits[i].name : "";
        object.splitStatus = SPLIT_INACTIVE;
        const auto& old = splits[i].type == SPLIT_TYPE_SCENE ? sceneObjectList : splitObjectList;
        for (const auto& prior : old) if (prior.splitId == object.splitId && prior.splitType == object.splitType) {
            object.splitCurrentTime = prior.splitCurrentTime;
            object.splitPreviousBest = prior.splitPreviousBest;
            object.splitStatus = prior.splitStatus;
            break;
        }
        (object.splitType == SPLIT_TYPE_SCENE ? scenes : items).push_back(std::move(object));
    }
    customSplits = true;
    splitObjectList = std::move(items);
    sceneObjectList = std::move(scenes);
    return true;
}

bool ModLabels_SetTimeSplitGroup(uint32_t id, const uint32_t* members, uint32_t count) {
    if (count > 4096 || (count && members == nullptr)) return false;
    if (count) itemSubMenuList[id] = {members, members + count}; else itemSubMenuList.erase(id);
    return true;
}
void ModLabels_ClearTimeSplitGroups() { itemSubMenuList.clear(); }

bool ModLabels_SetItemIcon(uint32_t item, uint32_t source) {
    if (item >= originalIcons.size() || source >= originalIcons.size()) return false;
    if (!haveIcons) {
        std::copy(std::begin(gItemIcons), std::end(gItemIcons), originalIcons.begin());
        haveIcons = true;
    }
    gItemIcons[item] = originalIcons[source];
    return true;
}
