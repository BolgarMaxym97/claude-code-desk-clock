package main

import (
	"bytes"
	"encoding/json"
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"testing"
	"time"
)

type got struct {
	path, token string
	body        map[string]any
}

func build(t *testing.T) string {
	bin := filepath.Join(t.TempDir(), "claude-clock")
	if runtime.GOOS == "windows" {
		bin += ".exe"
	}
	if out, err := exec.Command("go", "build", "-o", bin, ".").CombinedOutput(); err != nil {
		t.Fatal(string(out))
	}
	return bin
}

func fakeClock(t *testing.T) (*httptest.Server, chan got) {
	ch := make(chan got, 10)
	s := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		b, _ := io.ReadAll(r.Body)
		var m map[string]any
		json.Unmarshal(b, &m)
		ch <- got{r.URL.Path, r.Header.Get("X-Clock-Token"), m}
		w.Write([]byte(`{"ok":true}`))
	}))
	t.Cleanup(s.Close)
	return s, ch
}

func env(t *testing.T, url string) []string {
	d := t.TempDir()
	c, _ := json.Marshal(map[string]string{"url": url, "token": "tok123"})
	os.WriteFile(filepath.Join(d, "config.json"), c, 0o600)
	return append(os.Environ(), "CLOCK_CONFIG_DIR="+d, "CLAUDE_PROJECT_DIR=")
}

func run(t *testing.T, bin string, e []string, stdin string, args ...string) ([]byte, time.Duration) {
	cmd := exec.Command(bin, args...)
	cmd.Env = e
	cmd.Stdin = bytes.NewBufferString(stdin)
	var out, errb bytes.Buffer
	cmd.Stdout, cmd.Stderr = &out, &errb
	t0 := time.Now()
	if err := cmd.Run(); err != nil {
		t.Fatalf("%v: %s", err, errb.String())
	}
	if errb.Len() > 0 {
		t.Fatalf("stderr: %s", errb.String())
	}
	return out.Bytes(), time.Since(t0)
}

func wait(t *testing.T, ch chan got) got {
	select {
	case g := <-ch:
		return g
	case <-time.After(3 * time.Second):
		t.Fatal("clock got nothing")
	}
	return got{}
}

func TestHookIsSilentAndEventArrives(t *testing.T) {
	bin := build(t)
	s, ch := fakeClock(t)
	out, _ := run(t, bin, env(t, s.URL), `{"session_id":"s-1","hook_event_name":"PreToolUse","cwd":"/w/proj","tool_name":"Bash","tool_input":{"command":"npm test"}}`, "hook")
	if len(out) != 0 {
		t.Fatalf("hook printed %q", out)
	}
	g := wait(t, ch)
	if g.path != "/api/event" || g.token != "tok123" || g.body["target"] != "npm test" || g.body["project"] != "proj" {
		t.Fatal(g)
	}
}

func TestHookDoesNotWaitForAnUnreachableClock(t *testing.T) {
	bin := build(t)
	_, dt := run(t, bin, env(t, "http://10.255.255.1"), `{"session_id":"s","hook_event_name":"Stop"}`, "hook")
	if dt > 500*time.Millisecond {
		t.Fatalf("hook took %v", dt)
	}
}

func TestHookWithoutConfigOrWithGarbageExitsQuietly(t *testing.T) {
	bin := build(t)
	e := append(os.Environ(), "CLOCK_CONFIG_DIR="+t.TempDir())
	if out, _ := run(t, bin, e, "not json", "hook"); len(out) != 0 {
		t.Fatal(out)
	}
}

func TestStatuslinePassesOriginalOutputAndForwardsLimits(t *testing.T) {
	if runtime.GOOS == "windows" {
		t.Skip("uses a POSIX shell command as the original statusline")
	}
	bin := build(t)
	s, ch := fakeClock(t)
	e := env(t, s.URL)
	var dir string
	for _, kv := range e {
		if v, ok := bytes.CutPrefix([]byte(kv), []byte("CLOCK_CONFIG_DIR=")); ok {
			dir = string(v)
		}
	}
	os.WriteFile(filepath.Join(dir, "statusline.orig"), []byte(`cat >/dev/null; printf "\033[1mHUD\033[0m ✻\n"`), 0o600)
	in := `{"rate_limits":{"five_hour":{"used_percentage":72.4,"resets_at":1764100000},"seven_day":{"used_percentage":28,"resets_at":1764500000}}}`
	out, _ := run(t, bin, e, in, "statusline")
	if string(out) != "\033[1mHUD\033[0m ✻\n" {
		t.Fatalf("%q", out)
	}
	g := wait(t, ch)
	if g.path != "/api/limits" || g.body["h5"].(map[string]any)["pct"].(float64) != 72 {
		t.Fatal(g)
	}
	run(t, bin, e, in, "statusline") // throttled: no second post within 30 s
	select {
	case g := <-ch:
		t.Fatal("not throttled", g)
	case <-time.After(700 * time.Millisecond):
	}
}
