
#ifndef SAVE_MANAGER_H
#define SAVE_MANAGER_H

#ifdef __cplusplus
#include <string>
#include <filesystem>
#include <functional>
#include <nlohmann/json.hpp>
std::string SaveManager_GetFileName(int fileNum, bool isBackup = false);
bool SaveManager_HandleFileDropped(char* filePath);
bool BinarySaveConverter_HandleFileDropped(char* filePath);
int SaveManager_GetOpenFileSlot();
bool SaveManager_SlotExists(int fileNum);
void SaveManager_WriteSaveFile(const std::filesystem::path& fileName, nlohmann::json j);
int SaveManager_ReadSaveFile(const std::filesystem::path& fileName, nlohmann::json& j);
// Runs on the save thread: fills the save file as read from disk. Capture only snapshots, never live game state.
using FleetSaveFill = std::function<void(nlohmann::json& saveFile)>;
void SaveManager_QueueFleetSave(const std::filesystem::path& fileName, FleetSaveFill fill);
void SaveManager_WaitForFleetSaves();
std::string SaveManager_TakeFleetSaveError();
void SaveManager_StopFleetSaves();
void SaveManager_PersistSariaHintsAvailable();
#else
void SaveManager_SysFlashrom_WriteData(u8* addr, u32 pageNum, u32 pageCount);
s32 SaveManager_SysFlashrom_ReadData(void* addr, u32 pageNum, u32 pageCount);
#endif

#endif // SAVE_MANAGER_H
