#pragma once

#include <cmath>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>

namespace aste::density {

inline bool parseFiniteDecimal(std::string_view text, double& result) {
  std::istringstream input{std::string{text}};
  input.imbue(std::locale::classic());
  double parsed{};
  input >> std::noskipws >> parsed;
  if (input.fail() || !input.eof() || !std::isfinite(parsed)) {
    return false;
  }
  result = parsed;
  return true;
}

}  // namespace aste::density
