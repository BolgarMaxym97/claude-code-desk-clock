#include "text.h"

const char *verbFor(const std::string &tool) {
    if (tool == "Bash") return "Виконує";
    if (tool == "Edit" || tool == "Write" || tool == "MultiEdit" || tool == "NotebookEdit") return "Редагує";
    if (tool == "Read" || tool == "Grep" || tool == "Glob") return "Читає";
    if (tool == "Task" || tool == "Agent") return "Делегує";
    if (tool == "WebSearch" || tool == "WebFetch" || tool.rfind("mcp__", 0) == 0) return "Шукає";
    return "Працює";
}

std::string utf8Truncate(const std::string &s, size_t maxChars) {
    size_t chars = 0, i = 0;
    while (i < s.size()) {
        if (chars == maxChars) return s.substr(0, i) + "…";
        unsigned char c = static_cast<unsigned char>(s[i]);
        size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
        i += len;
        chars++;
    }
    return s;
}
