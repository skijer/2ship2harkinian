#pragma once
// Layer-merged JSON documents and the shared readers of the 2Ship Unbound format (mm/2s2h/Unbound/SPEC.md §2-§3).
#include <functional>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include <libultraship/libultra.h>
#include "z64math.h"

namespace S2H::Unbound {

using Json = nlohmann::json;

// A document that violates the format: factories catch it and fail the resource.
struct DocumentError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Deep-merges `overlay` into `base`: objects key-wise (later wins), null deletes, arrays replace,
// "$replace": true discards `base` for that subtree.
void MergeJson(Json& base, const Json& overlay);

// Every mounted archive's copy of `path`, merged lowest first, without merge directives or nulls. A null Json when no
// archive has the path or nothing parses.
Json LoadMergedJson(const std::string& path);

// One layer-merged registry document: `add` runs on each object entry in registry order; a JSON error inside one entry
// skips only that entry. Returns how many entries `add` accepted.
size_t ForEachRegistryEntry(const std::string& path, const char* what,
                            const std::function<bool(const std::string& key, const Json& entry)>& add);

// One registry entry per `<dir><name>.json`, each layer-merged on its own path, in sorted name order. `dir` is a glob
// prefix and must not contain glob characters.
size_t ForEachRegistryFile(const std::string& dir, const char* what,
                           const std::function<bool(const std::string& name, const Json& entry)>& add);

// Keys in engine order: "$order" first, then integer keys ascending, then the rest lexically. "$" keys never appear.
std::vector<std::string> ListKeys(const Json& obj);

// Keys "0", "1", ... of a positional list; a gap or "$order" is a DocumentError naming `what`.
std::vector<std::string> PositionalKeys(const Json& list, const std::string& what);

// The nested object / array under `key`, or a static empty one when absent or of another type.
const Json& Sub(const Json& obj, const char* key);
const Json& SubArray(const Json& obj, const char* key);

std::string SchemaOf(const Json& doc);
// "<type>/<n>" into its halves; false when the suffix is not a plain decimal integer.
bool ParseSchema(const std::string& schema, std::string& type, int& version);

// Decimal or 0x hex, optionally signed, consumed whole; ParseNumberString also takes a fraction and exponent.
bool ParseIntString(const std::string& text, int64_t& out);
bool ParseNumberString(const std::string& text, double& out);
int64_t ToInt(const Json& value, int64_t fallback = 0);
double ToNumber(const Json& value, double fallback = 0.0);

int64_t Field(const Json& obj, const char* key, int64_t fallback = 0);
double NumberField(const Json& obj, const char* key, double fallback = 0.0);
// Integral world data (collision): a fractional value is rounded.
s32 IntegralField(const Json& obj, const char* key, s32 fallback = 0);
std::string PathField(const Json& obj, const char* key);

Vec3s ReadVec3s(const Json& v);
Vec3f ReadVec3f(const Json& v);
Vec3i ReadVec3i(const Json& v);

template <typename T> void ReadRgb(const Json& v, T* out) {
    if (v.is_array() && v.size() >= 3) {
        out[0] = (T)ToInt(v[0]);
        out[1] = (T)ToInt(v[1]);
        out[2] = (T)ToInt(v[2]);
    }
}

// Raw bytes of `path` from the topmost archive that has it (bulk resources replace whole).
std::vector<char> LoadBulk(const std::string& path);

} // namespace S2H::Unbound
