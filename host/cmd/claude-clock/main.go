// claude-clock: connects Claude Code on this computer (macOS, Linux, Windows) to a Claude Clock on the
// local network. `claude-clock setup` finds and pairs the clock and registers Claude Code hooks; the hooks
// call `claude-clock hook` / `claude-clock statusline`, which must never slow down or break Claude Code.
package main

import (
	"bufio"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
	"time"

	"github.com/BolgarMaxym97/claude-code-desk-clock/host/internal/cfg"
	"github.com/BolgarMaxym97/claude-code-desk-clock/host/internal/claude"
	"github.com/BolgarMaxym97/claude-code-desk-clock/host/internal/clock"
	"github.com/BolgarMaxym97/claude-code-desk-clock/host/internal/event"
	"github.com/BolgarMaxym97/claude-code-desk-clock/host/internal/tz"
)

var version = "dev" // set by the release build: -ldflags "-X main.version=x.y.z"

const (
	sendTimeout = 1500 * time.Millisecond
	downFor     = 30 * time.Second // after a failed send, skip sends for a while (clock off / away)
	limitsEvery = 30 * time.Second
	githubRepo  = "BolgarMaxym97/claude-code-desk-clock"
)

func main() {
	if len(os.Args) < 2 {
		usage()
		os.Exit(2)
	}
	args := os.Args[2:]
	var err error
	switch os.Args[1] {
	case "hook":
		hook() // never fails
		return
	case "statusline":
		statusline()
		return
	case "send":
		send(args)
		return
	case "setup":
		err = setup(args)
	case "status":
		err = status()
	case "update":
		err = update(args)
	case "config":
		err = config(args)
	case "uninstall":
		err = uninstall(args)
	case "version", "--version", "-v":
		fmt.Println("claude-clock", version)
	case "help", "--help", "-h":
		usage()
	default:
		usage()
		os.Exit(2)
	}
	if err != nil {
		fmt.Fprintln(os.Stderr, "Error:", err)
		os.Exit(1)
	}
}

func usage() {
	fmt.Print(`claude-clock ` + version + ` — Claude Code activity on a desk clock

  claude-clock setup [--ip 192.168.1.50] [--yes]   find & pair the clock, register Claude Code hooks
  claude-clock status                              clock address, firmware, what it shows
  claude-clock update [--file firmware.bin]        install the latest clock firmware from GitHub
  claude-clock config --tz auto|<IANA name> | --birthday MM-DD|off
  claude-clock uninstall [--yes]                   remove hooks, restore your statusline
`)
}

// ---------- hot path: called by Claude Code on every hook event ----------

func hook() {
	defer func() { recover() }() // a hook must never break Claude Code
	in, _ := io.ReadAll(io.LimitReader(os.Stdin, 1<<20))
	c, err := cfg.Load()
	if err != nil || c.URL == "" || recentlyDown() {
		return
	}
	e, ok := event.FromHook(in, os.Getenv("CLAUDE_PROJECT_DIR"))
	if !ok {
		return
	}
	var h struct {
		Cwd string `json:"cwd"`
	}
	json.Unmarshal(in, &h)
	job, _ := json.Marshal(sendJob{Path: "/api/event", Event: &e, Cwd: h.Cwd})
	spawnSend(job)
}

func statusline() {
	in, _ := io.ReadAll(io.LimitReader(os.Stdin, 1<<20))
	if orig, err := os.ReadFile(cfg.File("statusline.orig")); err == nil && strings.TrimSpace(string(orig)) != "" {
		cmd := shellCommand(strings.TrimSpace(string(orig)))
		cmd.Stdin = strings.NewReader(string(in))
		cmd.Stdout, cmd.Stderr = os.Stdout, os.Stderr
		cmd.Run()
	}
	defer func() { recover() }()
	body, ok := event.Limits(in)
	if !ok || recentlyDown() || !throttle("limits.stamp", limitsEvery) {
		return
	}
	job, _ := json.Marshal(sendJob{Path: "/api/limits", Body: body})
	spawnSend(job)
}

type sendJob struct {
	Path  string          `json:"path"`
	Event *event.Event    `json:"event,omitempty"`
	Cwd   string          `json:"cwd,omitempty"`
	Body  json.RawMessage `json:"body,omitempty"`
}

// spawnSend hands the request to a detached child so Claude Code never waits for the network.
func spawnSend(job []byte) {
	exe, err := os.Executable()
	if err != nil {
		return
	}
	cmd := exec.Command(exe, "send")
	detach(cmd)
	w, err := cmd.StdinPipe()
	if err != nil || cmd.Start() != nil {
		return
	}
	w.Write(job)
	w.Close()
	cmd.Process.Release()
}

func send(_ []string) {
	defer func() { recover() }()
	var job sendJob
	if json.NewDecoder(io.LimitReader(os.Stdin, 1<<20)).Decode(&job) != nil {
		return
	}
	c, err := cfg.Load()
	if err != nil {
		return
	}
	body := []byte(job.Body)
	if job.Event != nil {
		if job.Cwd != "" && event.WantsBranch(job.Event.Event) {
			job.Event.Branch = gitBranch(job.Cwd)
		}
		body, _ = json.Marshal(job.Event)
	}
	if clock.Post(c.URL, c.Token, job.Path, body, sendTimeout) == nil {
		return
	}
	markDown()
	if c.Host != "" { // the clock may have a new IP address: look it up by name and remember it
		if u := relocate(c); u != "" && clock.Post(u, c.Token, job.Path, body, sendTimeout) == nil {
			c.URL = u
			cfg.Save(c)
			os.Remove(cfg.File("down.stamp"))
		}
	}
}

func relocate(c cfg.Config) string {
	for _, u := range clock.Discover(800 * time.Millisecond) {
		if u != c.URL {
			return u
		}
	}
	return ""
}

// gitBranch reads the branch without running repository hooks / fsmonitor (the repo may be untrusted).
func gitBranch(dir string) string {
	nul := "/dev/null"
	if runtime.GOOS == "windows" {
		nul = "NUL"
	}
	cmd := exec.Command("git", "-C", dir, "-c", "core.fsmonitor=false", "-c", "core.hooksPath="+nul, "branch", "--show-current")
	cmd.Env = append(os.Environ(), "GIT_OPTIONAL_LOCKS=0")
	out, err := cmd.Output()
	if err != nil {
		return ""
	}
	return strings.TrimSpace(string(out))
}

func stampAge(name string) (time.Duration, bool) {
	st, err := os.Stat(cfg.File(name))
	if err != nil {
		return 0, false
	}
	return time.Since(st.ModTime()), true
}

func touch(name string) {
	os.MkdirAll(cfg.Dir(), 0o700)
	p := cfg.File(name)
	if os.Chtimes(p, time.Now(), time.Now()) != nil {
		os.WriteFile(p, nil, 0o600)
	}
}

func recentlyDown() bool { age, ok := stampAge("down.stamp"); return ok && age < downFor }
func markDown()          { touch("down.stamp") }

func throttle(name string, every time.Duration) bool {
	if age, ok := stampAge(name); ok && age < every {
		return false
	}
	touch(name)
	return true
}

// ---------- interactive commands ----------

var stdin = bufio.NewReader(os.Stdin)

func ask(q string) string {
	fmt.Print(q)
	s, _ := stdin.ReadString('\n')
	return strings.TrimSpace(s)
}

func yes(q string, auto bool) bool {
	if auto {
		return true
	}
	a := strings.ToLower(ask(q + " [Y/n] "))
	return a == "" || a == "y" || a == "yes" || a == "т" || a == "так"
}

func flag(args []string, name string) (string, bool) {
	for i, a := range args {
		if a == name && i+1 < len(args) {
			return args[i+1], true
		}
		if v, ok := strings.CutPrefix(a, name+"="); ok {
			return v, true
		}
	}
	return "", false
}

func has(args []string, name string) bool {
	for _, a := range args {
		if a == name {
			return true
		}
	}
	return false
}

func findClock(args []string) (string, error) {
	if ip, ok := flag(args, "--ip"); ok {
		u := "http://" + strings.TrimPrefix(strings.TrimPrefix(ip, "http://"), "https://")
		if _, err := clock.GetStatus(u, 3*time.Second); err != nil {
			return "", fmt.Errorf("no Claude Clock at %s: %v", u, err)
		}
		return u, nil
	}
	fmt.Println("Looking for the clock on your network…")
	found := clock.Discover(1200 * time.Millisecond)
	switch len(found) {
	case 0:
		ip := ask("Clock not found. Type its IP address (your router's device list shows it as \"claude-clock\"): ")
		if ip == "" {
			return "", errors.New("no clock")
		}
		return findClock([]string{"--ip", ip})
	case 1:
		return found[0], nil
	}
	for i, u := range found {
		fmt.Printf("  %d) %s\n", i+1, u)
	}
	var n int
	fmt.Sscan(ask("Several clocks found. Which one? "), &n)
	if n < 1 || n > len(found) {
		return "", errors.New("no clock chosen")
	}
	return found[n-1], nil
}

func setup(args []string) error {
	auto := has(args, "--yes")
	u, err := findClock(args)
	if err != nil {
		return err
	}
	st, _ := clock.GetStatus(u, 3*time.Second)
	fmt.Printf("Found Claude Clock %s at %s\n", st.FW, u)
	if err := clock.PairStart(u); err != nil {
		return fmt.Errorf("could not start pairing: %v", err)
	}
	var token string
	for try := 0; try < 3; try++ {
		code := ask("Enter the 6-character code shown on the clock: ")
		token, err = clock.Pair(u, code)
		if err == nil {
			break
		}
		fmt.Println(" ", err)
		if !strings.Contains(err.Error(), "wrong code") {
			return err
		}
	}
	if token == "" {
		return errors.New("pairing failed")
	}
	c := cfg.Config{URL: u, Host: clock.MDNSName, Token: token}
	if err := cfg.Save(c); err != nil {
		return err
	}
	fmt.Println("Paired. Settings saved in", cfg.Dir())
	if err := sendTZ(c, ""); err != nil {
		fmt.Println("  (timezone not sent:", err, ")")
	}
	return registerHooks(auto)
}

func exePath() (string, error) {
	exe, err := os.Executable()
	if err != nil {
		return "", err
	}
	if r, err := filepath.EvalSymlinks(exe); err == nil {
		exe = r
	}
	exe = filepath.ToSlash(exe) // forward slashes work in Git Bash and cmd.exe alike
	return `"` + exe + `"`, nil
}

func registerHooks(auto bool) error {
	exe, err := exePath()
	if err != nil {
		return err
	}
	path := claude.SettingsPath()
	cur, err := os.ReadFile(path)
	if err != nil && !os.IsNotExist(err) {
		return err
	}
	saved, _ := os.ReadFile(cfg.File("statusline.orig"))
	plan, err := claude.Install(cur, exe, strings.TrimSpace(string(saved)))
	if err != nil {
		return fmt.Errorf("%s: %v", path, err)
	}
	if !plan.Changed {
		fmt.Println("Claude Code hooks already installed.")
		return nil
	}
	fmt.Printf("\nClaude Code settings (%s) will get:\n  - a `claude-clock hook` entry for %d events (your other hooks stay)\n", path, len(claude.Events))
	if plan.OrigStatus != "" {
		fmt.Printf("  - a statusline wrapper that still shows your statusline (%q)\n", plan.OrigStatus)
	} else {
		fmt.Println("  - a statusline that only forwards your 5h / 7d limits (shows nothing)")
	}
	if !yes("Apply? A backup is made first.", auto) {
		return errors.New("not applied; run `claude-clock setup` again any time")
	}
	if err := writeSettings(path, cur, plan.New); err != nil {
		return err
	}
	if plan.OrigStatus != "" {
		os.WriteFile(cfg.File("statusline.orig"), []byte(plan.OrigStatus), 0o600)
	}
	fmt.Println("Done. Start (or restart) Claude Code and watch the clock.")
	return nil
}

func writeSettings(path string, cur, next []byte) error {
	if err := os.MkdirAll(filepath.Dir(path), 0o700); err != nil {
		return err
	}
	if cur != nil {
		bak := path + ".bak-" + time.Now().Format("20060102-150405")
		if err := os.WriteFile(bak, cur, 0o600); err != nil {
			return err
		}
		fmt.Println("Backup:", bak)
	}
	tmp := path + ".tmp"
	if err := os.WriteFile(tmp, next, 0o600); err != nil {
		return err
	}
	return os.Rename(tmp, path)
}

func uninstall(args []string) error {
	path := claude.SettingsPath()
	cur, err := os.ReadFile(path)
	if err != nil && !os.IsNotExist(err) {
		return err
	}
	saved, _ := os.ReadFile(cfg.File("statusline.orig"))
	plan, err := claude.Uninstall(cur, strings.TrimSpace(string(saved)))
	if err != nil {
		return err
	}
	if plan.Changed {
		if !yes("Remove the clock's hooks from "+path+"?", has(args, "--yes")) {
			return errors.New("aborted")
		}
		if err := writeSettings(path, cur, plan.New); err != nil {
			return err
		}
	}
	os.RemoveAll(cfg.Dir())
	fmt.Println("Removed. You can delete the claude-clock program itself now.")
	return nil
}

func loadPaired() (cfg.Config, error) {
	c, err := cfg.Load()
	if err != nil || c.URL == "" {
		return c, errors.New("not set up yet: run `claude-clock setup`")
	}
	return c, nil
}

func status() error {
	c, err := loadPaired()
	if err != nil {
		return err
	}
	st, err := clock.GetStatus(c.URL, 3*time.Second)
	if err != nil {
		return fmt.Errorf("clock not reachable at %s (%v); run `claude-clock setup` if its address changed", c.URL, err)
	}
	fmt.Printf("Clock      %s\nFirmware   %s\nShows      %s  %s\nSessions   %d\nWi-Fi      %s\nTimezone   %s\nThis tool  %s\n",
		c.URL, st.FW, st.Screen, st.Time, st.Sessions, st.SSID, st.TZ, version)
	return nil
}

func sendTZ(c cfg.Config, name string) error {
	loc := time.Local
	if name != "" && name != "auto" {
		l, err := time.LoadLocation(name)
		if err != nil {
			return err
		}
		loc = l
	}
	s := tz.Posix(loc, time.Now().Year())
	if !tz.Valid(s) {
		return fmt.Errorf("cannot describe timezone %q for the clock", loc)
	}
	body, _ := json.Marshal(map[string]string{"tz": s})
	if err := clock.Post(c.URL, c.Token, "/api/settings", body, 5*time.Second); err != nil {
		return err
	}
	if loc.String() == "Local" {
		fmt.Printf("Timezone set: %s\n", s)
	} else {
		fmt.Printf("Timezone set: %s (%s)\n", loc, s)
	}
	return nil
}

func config(args []string) error {
	c, err := loadPaired()
	if err != nil {
		return err
	}
	done := false
	if v, ok := flag(args, "--tz"); ok {
		if err := sendTZ(c, v); err != nil {
			return err
		}
		done = true
	}
	if v, ok := flag(args, "--birthday"); ok {
		if v == "off" {
			v = ""
		}
		body, _ := json.Marshal(map[string]string{"birthday": v})
		if err := clock.Post(c.URL, c.Token, "/api/settings", body, 5*time.Second); err != nil {
			return fmt.Errorf("birthday: %v (use MM-DD, e.g. 11-20)", err)
		}
		fmt.Println("Birthday set.")
		done = true
	}
	if !done {
		usage()
	}
	return nil
}
