// Package event converts Claude Code hook / statusline JSON into the small payloads the clock accepts.
package event

import (
	"encoding/json"
	"math"
	"regexp"
	"strings"
	"unicode/utf8"
)

// Event is POSTed to the clock's /api/event.
type Event struct {
	Session string `json:"session"`
	Event   string `json:"event"`
	Project string `json:"project"`
	Tool    string `json:"tool"`
	Kind    string `json:"kind"`
	Target  string `json:"target"`
	Branch  string `json:"branch,omitempty"`
}

type hookIn struct {
	SessionID        string         `json:"session_id"`
	HookEventName    string         `json:"hook_event_name"`
	Cwd              string         `json:"cwd"`
	ToolName         string         `json:"tool_name"`
	NotificationType string         `json:"notification_type"`
	Prompt           string         `json:"prompt"`
	ToolInput        map[string]any `json:"tool_input"`
}

const targetMax = 22

var scheme = regexp.MustCompile(`^[a-zA-Z]+://`)

// FromHook builds the event; projectDir is $CLAUDE_PROJECT_DIR (preferred over the current subdirectory).
func FromHook(stdin []byte, projectDir string) (Event, bool) {
	var h hookIn
	if json.Unmarshal(stdin, &h) != nil || h.HookEventName == "" {
		return Event{}, false
	}
	dir := projectDir
	if dir == "" {
		dir = h.Cwd
	}
	e := Event{Session: h.SessionID, Event: h.HookEventName, Project: base(strings.TrimRight(dir, `/\`)), Tool: h.ToolName}
	if h.HookEventName == "Notification" {
		switch h.NotificationType {
		case "permission_prompt":
			e.Kind = "permission"
		case "idle_prompt":
			e.Kind = "idle"
		}
	}
	e.Target = cut(strings.NewReplacer("\n", " ", "\t", " ", "\r", " ").Replace(target(h)), targetMax)
	return e, true
}

// WantsBranch: the git branch is shown only on these events (cheap, local lookup).
func WantsBranch(event string) bool { return event == "SessionStart" || event == "UserPromptSubmit" }

func target(h hookIn) string {
	if h.HookEventName == "UserPromptSubmit" {
		return h.Prompt
	}
	ti := h.ToolInput
	str := func(k string) string { s, _ := ti[k].(string); return s }
	switch {
	case ti == nil:
		return ""
	case str("file_path") != "":
		return base(str("file_path"))
	case str("notebook_path") != "":
		return base(str("notebook_path"))
	case str("command") != "":
		return str("command")
	case str("url") != "":
		u := scheme.ReplaceAllString(str("url"), "")
		if i := strings.IndexByte(u, '/'); i >= 0 {
			u = u[:i]
		}
		return u
	case str("query") != "":
		return str("query")
	case str("pattern") != "":
		return str("pattern")
	case str("description") != "":
		return str("description")
	}
	return ""
}

func base(p string) string {
	if i := strings.LastIndexAny(p, `/\`); i >= 0 {
		return p[i+1:]
	}
	return p
}

func cut(s string, n int) string {
	if utf8.RuneCountInString(s) <= n {
		return s
	}
	r := []rune(s)
	return string(r[:n]) + "…"
}

type window struct {
	UsedPercentage *float64 `json:"used_percentage"`
	ResetsAt       int64    `json:"resets_at"`
}

// Limits builds the /api/limits body from the statusline JSON; false when Claude sent no rate limits.
func Limits(stdin []byte) ([]byte, bool) {
	var in struct {
		RateLimits struct {
			FiveHour window `json:"five_hour"`
			SevenDay window `json:"seven_day"`
		} `json:"rate_limits"`
	}
	if json.Unmarshal(stdin, &in) != nil {
		return nil, false
	}
	r := in.RateLimits
	if r.FiveHour.UsedPercentage == nil && r.SevenDay.UsedPercentage == nil {
		return nil, false
	}
	type w struct {
		Pct      int   `json:"pct"`
		ResetsAt int64 `json:"resets_at"`
	}
	conv := func(x window) w {
		p := 0.0
		if x.UsedPercentage != nil {
			p = *x.UsedPercentage
		}
		return w{int(math.Round(p)), x.ResetsAt}
	}
	b, _ := json.Marshal(struct {
		H5 w `json:"h5"`
		D7 w `json:"d7"`
	}{conv(r.FiveHour), conv(r.SevenDay)})
	return b, true
}
