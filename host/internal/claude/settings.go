// Package claude registers the clock's hooks and statusline in Claude Code's settings.json.
// It only adds / removes its own entries, keeps everything else (and the key order) as it was,
// and remembers the user's original statusline so the clock wrapper can run it unchanged.
package claude

import (
	"bytes"
	"encoding/json"
	"errors"
	"os"
	"path/filepath"
	"strings"
)

// Events the clock listens to.
var Events = []string{"SessionStart", "SessionEnd", "UserPromptSubmit", "PreToolUse", "PostToolUse",
	"PostToolUseFailure", "Notification", "Stop", "StopFailure", "SubagentStart", "SubagentStop", "PreCompact"}

// SettingsPath: $CLAUDE_SETTINGS, else $CLAUDE_CONFIG_DIR/settings.json, else ~/.claude/settings.json.
func SettingsPath() string {
	if p := os.Getenv("CLAUDE_SETTINGS"); p != "" {
		return p
	}
	if d := os.Getenv("CLAUDE_CONFIG_DIR"); d != "" {
		return filepath.Join(d, "settings.json")
	}
	home, _ := os.UserHomeDir()
	return filepath.Join(home, ".claude", "settings.json")
}

// Plan is the result of Install/Uninstall: the new file content and the statusline to remember (if any).
type Plan struct {
	New         []byte
	OrigStatus  string // user's original statusline command, to keep in statusline.orig ("" = none)
	RestoreOrig bool   // uninstall: statusline restored from OrigStatus
	Changed     bool
}

type hookCmd struct {
	Type    string `json:"type"`
	Command string `json:"command"`
	Timeout int    `json:"timeout,omitempty"`
}
type group struct {
	Matcher string            `json:"matcher,omitempty"`
	Hooks   []json.RawMessage `json:"hooks"`
}

// Ours: entries written by this tool or by the first shell-script version (clock-hook.sh / clock-statusline.sh).
func Ours(cmd string) bool {
	return strings.Contains(cmd, "claude-clock") && (strings.HasSuffix(cmd, " hook") || strings.HasSuffix(cmd, " statusline")) ||
		strings.HasSuffix(cmd, "clock-hook.sh") || strings.HasSuffix(cmd, "clock-statusline.sh")
}

// Install adds exe's hook for every event and wraps the statusline. savedOrig: statusline already remembered.
func Install(cur []byte, exe, savedOrig string) (Plan, error) {
	obj, err := parse(cur)
	if err != nil {
		return Plan{}, err
	}
	hooks, err := parse(obj.get("hooks"))
	if err != nil {
		return Plan{}, errors.New("settings.json: \"hooks\" is not an object")
	}
	hookCommand := exe + " hook"
	for _, ev := range Events {
		groups, _ := filterGroups(hooks.get(ev))
		g := group{Hooks: []json.RawMessage{mustJSON(hookCmd{"command", hookCommand, 5})}}
		groups = append(groups, mustJSON(g))
		hooks.set(ev, mustJSON(groups))
	}
	obj.set("hooks", hooks.encode())
	plan := Plan{OrigStatus: savedOrig}
	var sl struct {
		Command string `json:"command"`
	}
	json.Unmarshal(obj.get("statusLine"), &sl)
	if sl.Command != "" && !Ours(sl.Command) {
		plan.OrigStatus = sl.Command
	}
	obj.set("statusLine", mustJSON(map[string]any{"type": "command", "command": exe + " statusline"}))
	plan.New = obj.pretty()
	plan.Changed = !bytes.Equal(bytes.TrimSpace(plan.New), bytes.TrimSpace(cur))
	return plan, nil
}

// Uninstall removes our hooks; the statusline goes back to savedOrig (or is removed if it was ours only).
func Uninstall(cur []byte, savedOrig string) (Plan, error) {
	obj, err := parse(cur)
	if err != nil {
		return Plan{}, err
	}
	if h := obj.get("hooks"); h != nil {
		hooks, err := parse(h)
		if err != nil {
			return Plan{}, err
		}
		for _, k := range hooks.keys() {
			groups, _ := filterGroups(hooks.get(k))
			if len(groups) == 0 {
				hooks.del(k)
			} else {
				hooks.set(k, mustJSON(groups))
			}
		}
		if len(hooks) == 0 {
			obj.del("hooks")
		} else {
			obj.set("hooks", hooks.encode())
		}
	}
	var sl struct {
		Command string `json:"command"`
	}
	json.Unmarshal(obj.get("statusLine"), &sl)
	plan := Plan{}
	if Ours(sl.Command) {
		if savedOrig != "" {
			obj.set("statusLine", mustJSON(map[string]any{"type": "command", "command": savedOrig}))
			plan.RestoreOrig = true
		} else {
			obj.del("statusLine")
		}
	}
	plan.New = obj.pretty()
	plan.Changed = !bytes.Equal(bytes.TrimSpace(plan.New), bytes.TrimSpace(cur))
	return plan, nil
}

// filterGroups drops our hook commands from an event's groups (and groups left empty).
func filterGroups(raw json.RawMessage) ([]json.RawMessage, bool) {
	var groups []json.RawMessage
	if raw == nil || json.Unmarshal(raw, &groups) != nil {
		return nil, false
	}
	var out []json.RawMessage
	removed := false
	for _, gr := range groups {
		g, err := parse(gr)
		if err != nil {
			out = append(out, gr)
			continue
		}
		var hs []json.RawMessage
		json.Unmarshal(g.get("hooks"), &hs)
		var keep []json.RawMessage
		for _, h := range hs {
			var c hookCmd
			if json.Unmarshal(h, &c) == nil && Ours(c.Command) {
				removed = true
				continue
			}
			keep = append(keep, h)
		}
		if len(keep) == 0 && len(hs) > 0 {
			continue
		}
		if len(keep) != len(hs) {
			g.set("hooks", mustJSON(keep))
			out = append(out, g.encode())
		} else {
			out = append(out, gr)
		}
	}
	return out, removed
}

func mustJSON(v any) json.RawMessage {
	b, _ := json.Marshal(v)
	return b
}
