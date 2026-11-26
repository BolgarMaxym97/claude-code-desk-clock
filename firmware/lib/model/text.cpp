#include "text.h"
#include "i18n.h"

const char *verbFor(const std::string &tool) {
    if (tool == "Bash") return tr(T_V_RUN);
    if (tool == "Edit" || tool == "Write" || tool == "MultiEdit" || tool == "NotebookEdit") return tr(T_V_EDIT);
    if (tool == "Read" || tool == "Grep" || tool == "Glob") return tr(T_V_READ);
    if (tool == "Task" || tool == "Agent") return tr(T_V_DELEGATE);
    if (tool == "WebSearch" || tool == "WebFetch" || tool.rfind("mcp__", 0) == 0) return tr(T_V_SEARCH);
    return tr(T_V_WORK);
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
