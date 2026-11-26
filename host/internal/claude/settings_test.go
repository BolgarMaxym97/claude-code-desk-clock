package claude

import (
	"encoding/json"
	"strings"
	"testing"
)

const user = `{
  "model": "opus",
  "hooks": {
    "PreToolUse": [{"matcher": "Bash", "hooks": [{"type": "command", "command": "my-guard.sh"}]}],
    "Stop": [{"hooks": [{"type": "command", "command": "/old/mac/clock-hook.sh", "timeout": 5}]}]
  },
  "statusLine": {"type": "command", "command": "npx claude-hud"},
  "theme": "dark"
}`

func hookCmds(t *testing.T, b []byte, ev string) []string {
	var s struct {
		Hooks map[string][]struct {
			Hooks []struct{ Command string } `json:"hooks"`
		} `json:"hooks"`
	}
	if err := json.Unmarshal(b, &s); err != nil {
		t.Fatal(err)
	}
	var out []string
	for _, g := range s.Hooks[ev] {
		for _, h := range g.Hooks {
			out = append(out, h.Command)
		}
	}
	return out
}

func TestInstallAddsHooksKeepsOthersAndOrder(t *testing.T) {
	p, err := Install([]byte(user), `"/opt/cc/claude-clock"`, "")
	if err != nil {
		t.Fatal(err)
	}
	s := string(p.New)
	if !(strings.Index(s, `"model"`) < strings.Index(s, `"hooks"`) && strings.Index(s, `"hooks"`) < strings.Index(s, `"theme"`)) {
		t.Fatal("key order changed:\n" + s)
	}
	pre := hookCmds(t, p.New, "PreToolUse")
	if len(pre) != 2 || pre[0] != "my-guard.sh" || pre[1] != `"/opt/cc/claude-clock" hook` {
		t.Fatal(pre)
	}
	if stop := hookCmds(t, p.New, "Stop"); len(stop) != 1 || stop[0] != `"/opt/cc/claude-clock" hook` {
		t.Fatal("old shell hook not replaced:", stop)
	}
	for _, ev := range Events {
		if len(hookCmds(t, p.New, ev)) == 0 {
			t.Fatal("missing", ev)
		}
	}
	if p.OrigStatus != "npx claude-hud" || !strings.Contains(s, `claude-clock\" statusline`) {
		t.Fatal(p.OrigStatus, s)
	}
	again, _ := Install(p.New, `"/opt/cc/claude-clock"`, p.OrigStatus) // idempotent
	if again.Changed || again.OrigStatus != "npx claude-hud" {
		t.Fatal("second install changed something")
	}
}

func TestUninstallRestoresStatuslineAndRemovesOnlyOurs(t *testing.T) {
	p, _ := Install([]byte(user), "claude-clock", "")
	u, err := Uninstall(p.New, p.OrigStatus)
	if err != nil {
		t.Fatal(err)
	}
	if pre := hookCmds(t, u.New, "PreToolUse"); len(pre) != 1 || pre[0] != "my-guard.sh" {
		t.Fatal(pre)
	}
	if strings.Contains(string(u.New), "claude-clock") || !strings.Contains(string(u.New), "npx claude-hud") {
		t.Fatal(string(u.New))
	}
	if strings.Contains(string(u.New), `"Stop"`) {
		t.Fatal("empty event left behind")
	}
}

func TestEmptyOrMissingSettings(t *testing.T) {
	p, err := Install(nil, "claude-clock", "")
	if err != nil || len(hookCmds(t, p.New, "Stop")) != 1 {
		t.Fatal(err, string(p.New))
	}
	if _, err := Install([]byte(`[1,2]`), "x", ""); err == nil {
		t.Fatal("non-object accepted")
	}
}
