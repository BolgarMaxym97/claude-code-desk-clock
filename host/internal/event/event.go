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
	Kinds   string `json:"kinds,omitempty"` // finished Bash command: "tests,commit,push,install,docker" subset
	Now     int64  `json:"now,omitempty"`   // this computer's time (Unix): the clock uses it when NTP is blocked
}

type hookIn struct {
	SessionID        string         `json:"session_id"`
	HookEventName    string         `json:"hook_event_name"`
	Cwd              string         `json:"cwd"`
	ToolName         string         `json:"tool_name"`
	NotificationType string         `json:"notification_type"`
	Prompt           string         `json:"prompt"`
	Error            string         `json:"error"`      // StopFailure (newer Claude Code)
	ErrorType        string         `json:"error_type"` // StopFailure (some versions)
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
		case "elicitation_dialog": // an MCP server asks the user something: Claude waits like on a permission
			e.Kind = "permission"
		}
	}
	if h.HookEventName == "StopFailure" && (h.Error == "rate_limit" || h.ErrorType == "rate_limit") {
		e.Kind = "limit" // usage limit reached: the clock shows the limit screen, not an error
	}
	e.Target = cut(strings.NewReplacer("\n", " ", "\t", " ", "\r", " ").Replace(target(h)), targetMax)
	if strings.HasPrefix(h.ToolName, "mcp__") {
		e.Target = cut(mcpServer(h.ToolName), targetMax) // the server, never its input (task texts, queries)
	}
	if h.ToolName == "Bash" && (h.HookEventName == "PostToolUse" || h.HookEventName == "PostToolUseFailure") {
		if cmd, _ := h.ToolInput["command"].(string); cmd != "" {
			e.Kinds = strings.Join(BashKinds(cmd), ",")
		}
	}
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
		SessionID  string `json:"session_id"`
		RateLimits struct {
			FiveHour window `json:"five_hour"`
			SevenDay window `json:"seven_day"`
		} `json:"rate_limits"`
		Context struct {
			UsedPercentage *float64 `json:"used_percentage"`
		} `json:"context_window"`
	}
	if json.Unmarshal(stdin, &in) != nil {
		return nil, false
	}
	type w struct {
		Pct      int   `json:"pct"`
		ResetsAt int64 `json:"resets_at"`
	}
	type ctx struct {
		Session string `json:"session"`
		Pct     int    `json:"pct"`
	}
	var out struct {
		H5  *w   `json:"h5,omitempty"`
		D7  *w   `json:"d7,omitempty"`
		Ctx *ctx `json:"ctx,omitempty"`
	}
	conv := func(x window) *w {
		p := 0.0
		if x.UsedPercentage != nil {
			p = *x.UsedPercentage
		}
		return &w{int(math.Round(p)), x.ResetsAt}
	}
	r := in.RateLimits
	if r.FiveHour.UsedPercentage != nil || r.SevenDay.UsedPercentage != nil {
		out.H5, out.D7 = conv(r.FiveHour), conv(r.SevenDay)
	}
	if p := in.Context.UsedPercentage; p != nil && in.SessionID != "" {
		out.Ctx = &ctx{in.SessionID, int(math.Round(*p))}
	}
	if out.H5 == nil && out.Ctx == nil {
		return nil, false
	}
	b, _ := json.Marshal(out)
	return b, true
}

// mcpServer: "mcp__claude_ai_Google_Drive__search_files" -> "Google Drive", "mcp__plugin_figma_figma__x" -> "figma".
func mcpServer(tool string) string {
	rest := strings.TrimPrefix(tool, "mcp__")
	if i := strings.Index(rest, "__"); i >= 0 {
		rest = rest[:i]
	}
	rest = strings.TrimPrefix(rest, "claude_ai_")
	if strings.HasPrefix(rest, "plugin_") { // plugin_<plugin>_<server>
		parts := strings.SplitN(strings.TrimPrefix(rest, "plugin_"), "_", 2)
		rest = parts[len(parts)-1]
	}
	return strings.ReplaceAll(rest, "_", " ")
}
