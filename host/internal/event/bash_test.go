package event

import (
	"reflect"
	"testing"
)

func TestBashKinds(t *testing.T) {
	cases := map[string][]string{
		"git commit -q -F - <<'EOF'\nmsg\nEOF\ngit push -q origin main && git tag v1": {"commit", "push"},
		"git -C repo push --tags":         {"push"},
		"git -c user.name=x commit -m hi": {"commit"},
		"git status && git log --oneline": nil,
		"echo 'git push is great'":        nil, // only a word inside a string, not a git command
		"cd firmware && ./tools/test.sh":  {"tests"},
		"go test ./...":                   {"tests"},
		"npm run test:unit":               {"tests"},
		"pytest -q tests/":                {"tests"},
		"npm install && npm test":         {"tests", "install"},
		"pip3 install requests":           {"install"},
		"brew install jq":                 {"install"},
		"docker compose up -d":            {"docker"},
		"ls -la | grep push":              nil,
		"latest-tests-report.txt":         nil,
	}
	for cmd, want := range cases {
		if got := BashKinds(cmd); !reflect.DeepEqual(got, want) {
			t.Errorf("%q: got %v want %v", cmd, got, want)
		}
	}
}

func TestPostBashEventCarriesKinds(t *testing.T) {
	e, _ := FromHook(in("PostToolUse", map[string]any{"tool_name": "Bash", "tool_input": map[string]any{"command": "git commit -m x && git push"}}), "")
	if e.Kinds != "commit,push" {
		t.Fatalf("kinds %q", e.Kinds)
	}
	e, _ = FromHook(in("PreToolUse", map[string]any{"tool_name": "Bash", "tool_input": map[string]any{"command": "git push"}}), "")
	if e.Kinds != "" {
		t.Fatal("kinds only matter when the command finished")
	}
}
