// Text helpers shared by firmware screens (pure C++, unit-tested natively).
#pragma once
#include <string>

// Verb shown on the working screen for a Claude Code tool name (current language, see i18n.h).
const char *verbFor(const std::string &tool);

// Keep at most maxChars UTF-8 code points; if cut, append "…". Never splits a multi-byte char.
std::string utf8Truncate(const std::string &s, size_t maxChars);
