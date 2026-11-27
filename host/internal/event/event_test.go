package event

import (
	"encoding/json"
	"strings"
	"testing"
	"unicode/utf8"
)

func in(ev string, kv map[string]any) []byte {
	m := map[string]any{"session_id": "s-1", "hook_event_name": ev, "cwd": "/tmp/work/smart-clock-claude"}
	for k, v := range kv {
		m[k] = v
	}
	b, _ := json.Marshal(m)
	return b
}

func TestHookEvents(t *testing.T) {
	cases := []struct {
		stdin []byte
		want  map[string]string
	}{
		{in("SessionStart", map[string]any{"source": "startup"}), map[string]string{"event": "SessionStart", "project": "smart-clock-claude"}},
		{in("UserPromptSubmit", map[string]any{"prompt": "додай екран лімітів"}), map[string]string{"target": "додай екран лімітів"}},
		{in("PreToolUse", map[string]any{"tool_name": "Edit", "tool_input": map[string]any{"file_path": "/a/b/main.cpp"}}), map[string]string{"tool": "Edit", "target": "main.cpp"}},
		{in("PreToolUse", map[string]any{"tool_name": "Edit", "tool_input": map[string]any{"file_path": `C:\work\app\main.go`}}), map[string]string{"target": "main.go"}},
		{in("PreToolUse", map[string]any{"tool_name": "Bash", "tool_input": map[string]any{"command": "npm\ntest"}}), map[string]string{"target": "npm test"}},
		{in("PreToolUse", map[string]any{"tool_name": "WebFetch", "tool_input": map[string]any{"url": "https://devs.alerts.in.ua/docs"}}), map[string]string{"target": "devs.alerts.in.ua"}},
		{in("PreToolUse", map[string]any{"tool_name": "WebSearch", "tool_input": map[string]any{"query": "esp8266 ota"}}), map[string]string{"target": "esp8266 ota"}},
		{in("Notification", map[string]any{"notification_type": "permission_prompt"}), map[string]string{"kind": "permission"}},
		{in("Notification", map[string]any{"notification_type": "idle_prompt"}), map[string]string{"kind": "idle"}},
		// an MCP server asks the user something: the clock must show that Claude waits for you
		{in("Notification", map[string]any{"notification_type": "elicitation_dialog"}), map[string]string{"kind": "permission"}},
		// MCP tools: the server's name instead of the tool input
		{in("PreToolUse", map[string]any{"tool_name": "mcp__clickup__clickup_create_task", "tool_input": map[string]any{"name": "secret plan", "description": "x"}}), map[string]string{"target": "clickup"}},
		{in("PreToolUse", map[string]any{"tool_name": "mcp__claude_ai_Google_Drive__search_files", "tool_input": map[string]any{"query": "q"}}), map[string]string{"target": "Google Drive"}},
		{in("PreToolUse", map[string]any{"tool_name": "mcp__plugin_figma_figma__use_figma"}), map[string]string{"target": "figma"}},
		{in("SessionEnd", nil), map[string]string{"event": "SessionEnd", "session": "s-1"}},
		// usage limit: the turn ends with a rate_limit error (field name differs between Claude Code versions)
		{in("StopFailure", map[string]any{"error": "rate_limit"}), map[string]string{"event": "StopFailure", "kind": "limit"}},
		{in("StopFailure", map[string]any{"error_type": "rate_limit"}), map[string]string{"kind": "limit"}},
		{in("StopFailure", map[string]any{"error": "server_error"}), map[string]string{"kind": ""}},
	}
	for _, c := range cases {
		e, ok := FromHook(c.stdin, "")
		if !ok {
			t.Fatalf("not parsed: %s", c.stdin)
		}
		m := map[string]string{"event": e.Event, "project": e.Project, "target": e.Target, "tool": e.Tool, "kind": e.Kind, "session": e.Session}
		for k, v := range c.want {
			if m[k] != v {
				t.Errorf("%s: %s = %q, want %q", c.stdin, k, m[k], v)
			}
		}
	}
}

func TestLongUnicodeTargetCutOnRuneBoundary(t *testing.T) {
	cmd := "echo 'привіт 😀 " + strings.Repeat("x", 200) + "'"
	e, _ := FromHook(in("PreToolUse", map[string]any{"tool_name": "Bash", "tool_input": map[string]any{"command": cmd}}), "")
	if !strings.HasSuffix(e.Target, "…") || utf8.RuneCountInString(e.Target) > 23 || !utf8.ValidString(e.Target) {
		t.Fatalf("bad cut: %q", e.Target)
	}
	if !strings.HasPrefix(e.Target, "echo 'привіт 😀") {
		t.Fatalf("prefix lost: %q", e.Target)
	}
}

func TestProjectFromClaudeProjectDir(t *testing.T) {
	e, _ := FromHook(in("Stop", map[string]any{"cwd": "/tmp/work/smart-clock-claude/firmware"}), "/tmp/work/smart-clock-claude/")
	if e.Project != "smart-clock-claude" {
		t.Fatal(e.Project)
	}
}

func TestGarbageIsRejected(t *testing.T) {
	if _, ok := FromHook([]byte("not json"), ""); ok {
		t.Fatal("garbage accepted")
	}
}

func TestWantsBranch(t *testing.T) {
	if !WantsBranch("SessionStart") || !WantsBranch("UserPromptSubmit") || WantsBranch("PreToolUse") {
		t.Fatal()
	}
}

func TestLimits(t *testing.T) {
	b, ok := Limits([]byte(`{"rate_limits":{"five_hour":{"used_percentage":72.4,"resets_at":1764100000},"seven_day":{"used_percentage":28.0,"resets_at":1764500000}}}`))
	if !ok || string(b) != `{"h5":{"pct":72,"resets_at":1764100000},"d7":{"pct":28,"resets_at":1764500000}}` {
		t.Fatalf("%v %s", ok, b)
	}
	if _, ok := Limits([]byte(`{"session_id":"s"}`)); ok {
		t.Fatal("no limits -> nothing to send")
	}
	b, ok = Limits([]byte(`{"session_id":"abc","context_window":{"used_percentage":41.6},"rate_limits":{"five_hour":{"used_percentage":10,"resets_at":5}}}`))
	if !ok || string(b) != `{"h5":{"pct":10,"resets_at":5},"d7":{"pct":0,"resets_at":0},"ctx":{"session":"abc","pct":42}}` {
		t.Fatalf("with context: %v %s", ok, b)
	}
	b, ok = Limits([]byte(`{"session_id":"abc","context_window":{"used_percentage":12}}`))
	if !ok || string(b) != `{"ctx":{"session":"abc","pct":12}}` {
		t.Fatalf("API key user, context only: %v %s", ok, b)
	}
}
