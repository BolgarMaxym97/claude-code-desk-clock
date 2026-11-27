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
	"strings"
	"testing"
	"time"

	"github.com/BolgarMaxym97/claude-code-desk-clock/host/internal/clock"
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
		if r.Method == "GET" { // settings reads (alert levels job): no alerts token on this clock
			w.Write([]byte(`{"region":"","atok":""}`))
			return
		}
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
	// no alert levels job: its detached sender would write into d after the test, racing the TempDir cleanup
	os.WriteFile(filepath.Join(d, "alerts.off"), nil, 0o600)
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

func TestHealthLine(t *testing.T) {
	st := clock.Status{RSSI: -83, Uptime: 3700, Heap: 25000, HeapMin: 21000, Reset: "Power On", WxCode: 401, Alert: "unknown", AlCode: -1}
	got := health(st)
	for _, want := range []string{"-83 dBm (weak)", "up 1h2m0s", "24 KB free (lowest 20 KB)", "last restart: power on",
		"weather: key rejected (401)", "alerts: no answer for 5+ min (no connection)"} {
		if !strings.Contains(got, want) {
			t.Errorf("health() = %q, missing %q", got, want)
		}
	}
	if h := health(clock.Status{RSSI: -60, WxCode: 200}); strings.Contains(h, "weather") || !strings.Contains(h, "(good)") {
		t.Errorf("healthy clock: %q", h)
	}
}

func TestPrefValues(t *testing.T) {
	for in, want := range map[string]string{"23-7": "23-7", "off": "off", "07-1": "7-1"} {
		if got, err := nightValue(in); err != nil || got != want {
			t.Errorf("nightValue(%q) = %q, %v", in, got, err)
		}
	}
	for _, bad := range []string{"24-7", "5-5", "x"} {
		if _, err := nightValue(bad); err == nil {
			t.Errorf("nightValue(%q) accepted", bad)
		}
	}
	if got, _ := brightValue("60"); got != "60/60" {
		t.Errorf("brightValue(60) = %q", got)
	}
	if _, err := brightValue("80/2"); err == nil {
		t.Error("brightness 2% accepted")
	}
	if got, _ := faceValue("pixel"); got != "2" {
		t.Errorf("faceValue(pixel) = %q", got)
	}
	now := time.Unix(1000, 0)
	if got, _ := pinValue("coffee", "2h", now); got != "coffee,8200" {
		t.Errorf("pinValue 2h = %q", got)
	}
	if got, _ := pinValue("coffee", "always", now); got != "coffee,0" {
		t.Errorf("pinValue always = %q", got)
	}
	if _, err := pinValue("coffee", "10s", now); err == nil {
		t.Error("10s accepted")
	}
}
