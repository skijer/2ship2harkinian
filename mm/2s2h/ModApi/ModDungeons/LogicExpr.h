#pragma once

#include <functional>
#include <cstdint>
#include <string>

class LogicExpr {
  public:
    bool Compile(const std::string& expression, std::string& error, int32_t dungeon = -1);
    bool Evaluate() const;

  private:
    std::function<bool()> mCondition;
};
