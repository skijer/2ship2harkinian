#include "2s2h/BenPort.h"
#include "2s2h/resource/type/TextMM.h"
#include "UnboundJson.h"
#include <message_data_static.h>
#include <deque>
#include <map>
#include <unordered_map>
#include <spdlog/spdlog.h>

extern "C" MessageTableEntry* sMessageTableNES;
extern "C" MessageTableEntry* sMessageTableCredits;

namespace {
struct Table {
    std::deque<std::string> bytes;
    std::vector<MessageTableEntry> entries;
    std::unordered_map<u16, size_t> indices;
};
Table normal, staff;
bool initialized = false;
std::string TextBytes(const std::string& text) {
    std::string result;
    for (size_t i = 0; i < text.size();) {
        const uint8_t first = text[i++];
        uint32_t point = first;
        if (first >= 0x80) {
            const size_t extra = first >= 0xF0 ? 3 : first >= 0xE0 ? 2 : first >= 0xC0 ? 1 : 0;
            point = first & (extra == 3 ? 7 : extra == 2 ? 15 : 31);
            bool valid = extra != 0 && i + extra <= text.size();
            for (size_t n = 0; valid && n < extra; n++) {
                const uint8_t next = text[i];
                if ((next & 0xC0) != 0x80) { valid = false; break; }
                point = (point << 6) | (next & 0x3F);
                i++;
            }
            if (!valid || point > 255 || extra > 1 || point < 128) { point = '?'; }
        }
        result.push_back((char)point);
    }
    return result;
}
void Load(Table& table, const char* binary, const char* jsonPath, bool credits) {
    using namespace S2H::Unbound;
    std::map<u16, SOH::MessageEntryMM> messages;
    const Json document = LoadMergedJson(jsonPath);
    const bool jsonBase = document.is_object() && document.contains("messages") && document["messages"].is_object();
    auto resources = Ship::Context::GetRawInstance()->GetResourceManager();
    if (!jsonBase) {
        auto file = std::dynamic_pointer_cast<SOH::TextMM>(resources->LoadResource(binary));
        if (file) { for (const auto& row : file->messages) { if (row.id != 0xFFFF) { messages[row.id] = row; } } }
    } else {
        for (const auto& [name, value] : document["messages"].items()) {
            if (name.starts_with("$")) { continue; }
            int64_t id;
            if (!ParseIntString(name, id) || id < 0 || id >= 0xFFFF || !value.is_object()) {
                SPDLOG_WARN("[Unbound] {}: invalid message '{}'", jsonPath, name);
                continue;
            }
            SOH::MessageEntryMM row = {};
            row.id = id;
            row.textboxType = Field(value, "box");
            row.textboxYPos = Field(value, "ypos");
            row.icon = Field(value, "icon", 0xFE);
            row.nextMessageID = Field(value, "nextMessage", 0xFFFF);
            row.firstItemCost = Field(value, "firstItemCost", 0xFFFF);
            row.secondItemCost = Field(value, "secondItemCost", 0xFFFF);
            if (value.contains("text") && value["text"].is_string()) { row.msg = TextBytes(value["text"].get<std::string>()); }
            messages[row.id] = std::move(row);
        }
    }
    for (const auto& path : *resources->GetArchiveManager()->ListFiles(std::string("override/") + binary + "*")) {
        auto override = std::dynamic_pointer_cast<SOH::TextMM>(resources->LoadResource(path));
        if (override) { for (const auto& row : override->messages) { if (row.id != 0xFFFF) { messages[row.id] = row; } } }
    }
    for (const auto& [id, row] : messages) {
        std::string bytes;
        if (!credits) {
            bytes.push_back(row.textboxType);
            bytes.push_back(row.textboxYPos);
            bytes.push_back(row.icon);
            for (u16 value : { row.nextMessageID, row.firstItemCost, row.secondItemCost }) {
                bytes.push_back(value >> 8);
                bytes.push_back(value & 255);
            }
            bytes.append(2, (char)0xFF);
        }
        bytes += row.msg;
        const char end = credits ? 2 : (char)0xBF;
        if (bytes.empty() || bytes.back() != end) { bytes.push_back(end); }
        if (bytes.size() > 8192) {
            SPDLOG_WARN("[Unbound] Message {:#x} truncated to 8192 bytes", id);
            bytes.resize(8192);
            bytes.back() = end;
        }
        table.bytes.push_back(std::move(bytes));
        table.indices[id] = table.entries.size();
        table.entries.push_back({ id, (u8)((row.textboxType << 4) | row.textboxYPos),
                                  table.bytes.back().data(), (u32)table.bytes.back().size() });
    }
    if (table.entries.empty()) {
        std::string fallback(11, (char)0xFF);
        fallback[0] = fallback[1] = 0;
        fallback[2] = (char)0xFE;
        fallback.push_back((char)0xBF);
        table.bytes.push_back(credits ? std::string(1, 2) : std::move(fallback));
        table.entries.push_back({ 0, 0, table.bytes.back().data(), (u32)table.bytes.back().size() });
    }
    table.entries.push_back({ 0xFFFF, 0, nullptr, 0 });
}
}
extern "C" MessageTableEntry* OTRMessage_Find(MessageTableEntry* entries, u16 id) {
    Table* table = entries == normal.entries.data() ? &normal : entries == staff.entries.data() ? &staff : nullptr;
    if (!table) { return nullptr; }
    const auto found = table->indices.find(id);
    return found == table->indices.end() ? &table->entries.front() : &table->entries[found->second];
}
extern "C" void OTRMessage_Init() {
    if (!initialized) {
        Load(normal, "text/message_data_static/message_data_static", "text/eng/messages.json", false);
        Load(staff, "text/staff_message_data_static/staff_message_data_static", "text/staff/messages.json", true);
        initialized = true;
    }
    sMessageTableNES = normal.entries.data();
    sMessageTableCredits = staff.entries.data();
}
