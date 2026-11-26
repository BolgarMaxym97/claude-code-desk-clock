#include "scenes_internal.h"
#include <string.h>

int sceneCount() { return SCENES_A_N + SCENES_B_N + SCENES_C_N + SCENES_W_N; }

Scene sceneAt(int i) {
    const Scene *src;
    if (i < SCENES_A_N) src = &SCENES_A[i];
    else if ((i -= SCENES_A_N) < SCENES_B_N) src = &SCENES_B[i];
    else if ((i -= SCENES_B_N) < SCENES_C_N) src = &SCENES_C[i];
    else src = &SCENES_W[i - SCENES_C_N];
    Scene s;
    memcpy_P(&s, src, sizeof s);
    return s;
}

#include <string.h>
#include "model.h"

int sceneIndexByName(const char *name) {
    for (int i = 0; i < sceneCount(); i++)
        if (!strcmp(sceneAt(i).name, name)) return i;
    return -1;
}

static bool any(const char *tool, const char *const *names) {
    for (; *names; names++)
        if (!strcmp(tool, *names)) return true;
    return false;
}

int workScene(SState state, const char *tool, uint32_t variant) {
    static const char *const EDIT[] = {"Edit", "Write", "MultiEdit", "NotebookEdit", nullptr};
    static const char *const READ[] = {"Read", "Grep", "Glob", nullptr};
    static const char *const AGENT[] = {"Task", "Agent", nullptr};
    static const char *const WEB[] = {"WebSearch", "WebFetch", nullptr};
    const char *name;
    switch (state) {
        case SState::Thinking: name = "work:think"; break;
        case SState::Compacting: name = "work:compact"; break;
        case SState::Permission: name = "work:permission"; break;
        case SState::YourTurn: name = "work:your-turn"; break;
        case SState::Error: name = "work:error"; break;
        default:
            if (!strcmp(tool, "Bash")) name = "work:bash";
            else if (any(tool, EDIT)) name = "work:edit";
            else if (any(tool, READ)) name = "work:read";
            else if (any(tool, AGENT)) name = "work:agents";
            else if (any(tool, WEB)) name = "work:web";
            else name = (variant % 2) ? "work:laptop" : "work:walk";
    }
    return sceneIndexByName(name);
}

int pickWorkScene(const WorkCtx &w) {
    static const char *const EDIT[] = {"Edit", "Write", "MultiEdit", "NotebookEdit", nullptr};
    static const char *const AGENT[] = {"Task", "Agent", nullptr};
    const char *name = nullptr;
    if (w.greeting) name = w.morning ? "work:morning" : "work:back";
    else if (w.reactionAgeMs <= 10000) {
        switch (w.reaction) {
            case Reaction::TestsOk: name = "work:tests-ok"; break;
            case Reaction::TestsFail: name = "work:tests-fail"; break;
            case Reaction::Commit: name = "work:commit"; break;
            case Reaction::Push: name = "work:push"; break;
            case Reaction::Install: name = "work:install"; break;
            case Reaction::Docker: name = "work:docker"; break;
            default: break;
        }
    }
    if (!name && w.screen == Screen::Done) name = w.doneDurationMs >= 120000 ? "work:done" : "work:done-small";
    if (!name && w.screen == Screen::Hello) name = w.morning ? "work:morning" : "work:back";
    if (!name && w.state == SState::Working) {
        if (!strcmp(w.tool, "Bash") && w.toolAgeMs >= 60000) name = "work:long";
        else if (any(w.tool, EDIT) && w.edits >= 10) name = "work:edits";
        else if (any(w.tool, AGENT) && w.agents >= 3) name = "work:orchestra";
    }
    if (!name && w.state == SState::Thinking && w.synced && (w.hour >= 23 || w.hour < 5)) name = "work:late";
    return name ? sceneIndexByName(name) : workScene(w.state, w.tool, w.variant);
}
