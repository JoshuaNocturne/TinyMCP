#pragma once

#include <codecvt>
#include <locale>
#include <string>

namespace MCP {
namespace StringHelper {
inline std::wstring utf8_string_to_wstring(const std::string& str) {
  try {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.from_bytes(str);
  } catch (const std::exception& e) {
    // Èç¹û×ª»»Ê§°Ü£¬·µ»Ø¿Õ×Ö·û´®
    return L"";
  }
}

inline std::string wstring_to_utf8_string(const std::wstring& strW) {
  try {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(strW);
  } catch (const std::exception& e) {
    // Èç¹û×ª»»Ê§°Ü£¬·µ»Ø¿Õ×Ö·û´®
    return "";
  }
}
}  // namespace StringHelper
}  // namespace MCP