#include "clrp.hpp"

#include <format>

namespace clrp {
std::string format(const std::string& str, ATTRIBUTE attr, FG fg, BG bg, RESET reset) {
  return std::format("\033[{};{};{}m{}\033[{}m", (u16)attr, (u16)fg, (u16)bg, str, (u16)reset);
}

std::string prepare(ATTRIBUTE attr, FG fg, BG bg, RESET reset) {
  return std::format("\033[{};{};{}m%s\033[{}m", (u16)attr, (u16)fg, (u16)bg, (u16)reset);
}
}; // namespace clrp

