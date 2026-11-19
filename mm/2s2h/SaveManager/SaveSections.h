#ifndef SAVE_SECTIONS_H
#define SAVE_SECTIONS_H

#ifdef __cplusplus

#include <functional>
#include <string>
#include <nlohmann/json.hpp>

using SaveSectionWrite = std::function<nlohmann::json()>;
using SaveSectionRead = std::function<void(const nlohmann::json& section)>;

void SaveSections_Register(const std::string& name, SaveSectionWrite write, SaveSectionRead read);

void SaveSections_Write(nlohmann::json& saveFile, bool isOwlSave);
void SaveSections_RememberRead(const std::string& fileName, const nlohmann::json& saveFile, bool isOwlSave);

#endif

#endif // SAVE_SECTIONS_H
