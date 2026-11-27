// Readable scene names (EN / UK, current language) for the settings page and `claude-clock scene list`.
#pragma once
#include <string>

std::string sceneTitle(const char *name);  // unknown name: the name itself
#include <stdint.h>
// Pinned idle scene from the settings value "name,untilEpoch" (0 = always): its index while it holds, else -1
// (none, unknown, a work scene, time is up, or a timed pin before the clock is synced).
int pinnedScene(const char *pin, uint32_t epoch);
