#include "LogicExpr.h"

#include <cctype>
#include <limits>
#include <stdexcept>
#include "2s2h/ModApi/RandoItems/RandoItems.h"
#include "2s2h/ModApi/RandoLogic/RandoLogic.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/Logic/Logic.h"
#include "2s2h/Unbound/UnboundJson.h"

namespace {
using Condition = std::function<bool()>;

class Parser {
  public:
    explicit Parser(const std::string& expression, int32_t context) : text(expression), dungeon(context) {
    }

    Condition Parse() {
        Condition result = Or();
        Space();
        if (position != text.size()) {
            Fail("unexpected input");
        }
        return result;
    }

  private:
    const std::string& text;
    size_t position = 0;
    size_t depth = 0;
    int32_t dungeon;

    void Fail(const std::string& message) {
        throw std::runtime_error(message + " at column " + std::to_string(position + 1));
    }
    int32_t Integer(const std::string& value) {
        int64_t parsed;
        if (!S2H::Unbound::ParseIntString(value, parsed) || parsed < INT32_MIN || parsed > INT32_MAX) {
            Fail("expected a 32-bit integer");
        }
        return (int32_t)parsed;
    }
    static bool NameChar(char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.' || c == ':' || c == '-';
    }
    void Space() {
        while (position < text.size() && std::isspace(static_cast<unsigned char>(text[position]))) {
            position++;
        }
    }
    bool Take(const std::string& value, bool word = false) {
        Space();
        const size_t end = position + value.size();
        if (text.compare(position, value.size(), value) != 0 ||
            (word && end < text.size() && NameChar(text[end]))) {
            return false;
        }
        position = end;
        return true;
    }
    std::string Name() {
        Space();
        if (position < text.size() && text[position] == '"') {
            const size_t start = ++position;
            while (position < text.size() && text[position] != '"') { position++; }
            if (position == text.size()) { Fail("unterminated quoted name"); }
            const std::string result = text.substr(start, position - start);
            position++;
            return result;
        }
        size_t start = position;
        while (position < text.size() && NameChar(text[position])) {
            position++;
        }
        if (start == position) {
            Fail("expected a name");
        }
        return text.substr(start, position - start);
    }
    Condition Or() {
        Condition result = And();
        while (Take("or", true) || Take("||")) {
            Condition right = And();
            result = [left = std::move(result), right]() { return left() || right(); };
        }
        return result;
    }
    Condition And() {
        Condition result = Factor();
        while (Take("and", true) || Take("&&")) {
            Condition right = Factor();
            result = [left = std::move(result), right]() { return left() && right(); };
        }
        return result;
    }
    Condition Factor() {
        if (++depth > 128) {
            Fail("expression is too deeply nested");
        }
        Condition result;
        if (Take("not", true) || Take("!")) {
            Condition inner = Factor();
            result = [inner]() { return !inner(); };
        } else if (Take("(")) {
            result = Or();
            if (!Take(")")) {
                Fail("expected ')'");
            }
        } else {
            result = Atom();
        }
        depth--;
        return result;
    }
    Condition Atom() {
        const std::string name = Name();
        if (name == "true") { return []() { return true; }; }
        if (name == "false") { return []() { return false; }; }
        if (name == "child" || name == "human") { return RandoLogic_IsChild; }
        if (name == "adult" || name == "transformed") { return RandoLogic_IsAdult; }
        if (name == "day") { return RandoLogic_IsAtDay; }
        if (name == "night") { return RandoLogic_IsAtNight; }
        if (name == "bosskey") { return [context = dungeon]() { return DungeonRegistry_HasBossKey(context); }; }
        if (Take("(")) {
            Space();
            const size_t start = position;
            while (position < text.size() && text[position] != ')' && text[position] != ',') { position++; }
            std::string argument = text.substr(start, position - start);
            while (!argument.empty() && std::isspace(static_cast<unsigned char>(argument.back()))) { argument.pop_back(); }
            if (argument.size() >= 2 && argument.front() == '"' && argument.back() == '"') {
                argument = argument.substr(1, argument.size() - 2);
            }
            int32_t settingValue = 1;
            if (Take(",")) { settingValue = Integer(Name()); }
            if (!Take(")")) { Fail("expected ')'"); }
            if (name == "keys") {
                const int32_t count = Integer(argument);
                if (count < 0 || count > 127) { Fail("invalid key count"); }
                return [context = dungeon, count]() { return DungeonRegistry_HasKeys(context, count); };
            }
            if (name == "event") {
                const int32_t event = DungeonRegistry_FindSymbol(argument);
                if (event >= 0 && argument.starts_with("RE_")) {
                    return [event]() { return RANDO_EVENTS[event] != 0; };
                }
                return [context = dungeon, argument]() { return DungeonRegistry_GetEvent(context, argument); };
            }
            if (name == "setting" || name == "trick") {
                const int32_t option = DungeonRegistry_FindSymbol(argument);
                if (option < 0 || option >= RO_MAX || !argument.starts_with("RO_")) { Fail("unknown option"); }
                return [option, settingValue]() { return RANDO_SAVE_OPTIONS[option] == settingValue; };
            }
            if (name == "tokens") {
                const int32_t count = Integer(argument);
                return [count]() { return Inventory_GetSkullTokenCount(SCENE_KINSTA1) >= count ||
                                         Inventory_GetSkullTokenCount(SCENE_KINDAN2) >= count; };
            }
            if (name == "canKill") {
                const int32_t actor = DungeonRegistry_FindSymbol(argument);
                if (actor < 0 || !argument.starts_with("ACTOR_")) { Fail("unknown actor"); }
                return [actor]() { return Rando::Logic::CanKillEnemy(static_cast<ActorId>(actor)); };
            }
            if (name == "capability" || name == "cap") {
                return Capability(argument);
            }
            if (name != "canUse" && name != "item" && name != "has" && name != "hasItem") {
                Fail("unknown predicate '" + name + "'");
            }
            return Item(argument, name == "has" || name == "hasItem");
        }
        const char* const capabilities[] = {
#define S2H_LOGIC_NAME(name, token) #name,
            RANDO_LOGIC_CAPABILITIES(S2H_LOGIC_NAME)
#undef S2H_LOGIC_NAME
        };
        for (const char* capability : capabilities) {
            if (name == capability) { return Capability(name); }
        }
        return Item(name, false);
    }
    Condition Capability(const std::string& name) {
        bool known = false;
#define S2H_LOGIC_KNOWN(capability, token) if (name == #capability) { known = true; }
        RANDO_LOGIC_CAPABILITIES(S2H_LOGIC_KNOWN)
#undef S2H_LOGIC_KNOWN
        if (!known) { Fail("unknown capability '" + name + "'"); }
        return [name]() { return RandoLogic_HasCapability(name.c_str()); };
    }
    Condition Item(const std::string& name, bool has) {
        const int32_t symbol = DungeonRegistry_FindSymbol(name);
        if (symbol >= 0 && name.starts_with("RI_")) {
            return [symbol, has]() { return has ? RandoLogic_HasLogicItem(symbol) : RandoLogic_CanUseItem(symbol); };
        }
        for (const auto& [id, item] : Rando::StaticData::Items) {
            if ((item.name && name == item.name) || (item.spoilerName && name == item.spoilerName)) {
                return [id, has]() { return has ? RandoLogic_HasLogicItem(id) : RandoLogic_CanUseItem(id); };
            }
        }
        const int32_t custom = RandoItems_GetRandoGet(name.c_str());
        if (!RandoItems_IsModItem(custom)) { Fail("unknown item '" + name + "'"); }
        return [custom, has]() { return has ? RandoLogic_HasLogicItem(custom) : RandoLogic_CanUseItem(custom); };
    }
};
}

bool LogicExpr::Compile(const std::string& expression, std::string& error, int32_t dungeon) {
    mCondition = nullptr;
    error.clear();
    try {
        if (expression.size() > 8192) {
            throw std::runtime_error("expression exceeds 8192 bytes");
        }
        mCondition = expression.empty() ? Condition([]() { return true; }) : Parser(expression, dungeon).Parse();
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

bool LogicExpr::Evaluate() const {
    return mCondition && mCondition();
}
