package main

// Display preferences (sleep hours, brightness, clock face), the pinned idle scene and messages on the clock.

import (
	"encoding/json"
	"fmt"
	"regexp"
	"strconv"
	"strings"
	"time"

	"github.com/BolgarMaxym97/claude-code-desk-clock/host/internal/cfg"
	"github.com/BolgarMaxym97/claude-code-desk-clock/host/internal/clock"
)

var (
	nightRe  = regexp.MustCompile(`^(\d{1,2})-(\d{1,2})$`)
	brightRe = regexp.MustCompile(`^(\d{1,3})/(\d{1,3})$`)
)

// nightValue checks "23-7" / "off" (the clock checks again).
func nightValue(v string) (string, error) {
	if v == "off" {
		return v, nil
	}
	m := nightRe.FindStringSubmatch(v)
	if m == nil {
		return "", fmt.Errorf("sleep hours %q: use FROM-TO in hours, e.g. 23-7, or off", v)
	}
	a, _ := strconv.Atoi(m[1])
	b, _ := strconv.Atoi(m[2])
	if a > 23 || b > 23 || a == b {
		return "", fmt.Errorf("sleep hours %q: hours are 0..23 and must differ", v)
	}
	return fmt.Sprintf("%d-%d", a, b), nil
}

// brightValue checks "80/20" (day/night %, 5..100) or a single number for both.
func brightValue(v string) (string, error) {
	if n, err := strconv.Atoi(v); err == nil {
		v = fmt.Sprintf("%d/%d", n, n)
	}
	m := brightRe.FindStringSubmatch(v)
	if m == nil {
		return "", fmt.Errorf("brightness %q: use DAY/NIGHT in %%, e.g. 80/20", v)
	}
	a, _ := strconv.Atoi(m[1])
	b, _ := strconv.Atoi(m[2])
	if a < 5 || a > 100 || b < 5 || b > 100 {
		return "", fmt.Errorf("brightness %q: each value 5..100", v)
	}
	return fmt.Sprintf("%d/%d", a, b), nil
}

var faces = []string{"classic", "minimal", "pixel", "segment", "flip", "terminal", "binary"}

func faceValue(v string) (string, error) {
	for i, f := range faces {
		if v == f || v == strconv.Itoa(i) {
			return strconv.Itoa(i), nil
		}
	}
	return "", fmt.Errorf("clock face %q: use %s", v, strings.Join(faces, ", "))
}

func setPref(c cfg.Config, key, value, done string) error {
	body, _ := json.Marshal(map[string]string{key: value})
	if err := clock.Post(c.URL, c.Token, "/api/settings", body, 5*time.Second); err != nil {
		return fmt.Errorf("%s: %v", key, err)
	}
	fmt.Println(done)
	return nil
}

// configPrefs handles --night, --brightness and --face; done reports whether any was given.
func configPrefs(c cfg.Config, args []string) (bool, error) {
	done := false
	for _, p := range []struct {
		flag, key string
		check     func(string) (string, error)
		msg       string
	}{
		{"--night", "night", nightValue, "Sleep hours set."},
		{"--brightness", "bright", brightValue, "Brightness set."},
		{"--face", "face", faceValue, "Clock face set."},
	} {
		v, ok := flag(args, p.flag)
		if !ok {
			continue
		}
		val, err := p.check(v)
		if err != nil {
			return done, err
		}
		if err := setPref(c, p.key, val, p.msg); err != nil {
			return done, err
		}
		done = true
	}
	return done, nil
}

type sceneInfo struct {
	Name  string `json:"n"`
	Title string `json:"t"`
	Kind  string `json:"k"`
}

// pinValue makes the clock's "name,untilEpoch" from a duration: "always", "30m", "2h", "1h30m".
func pinValue(name, dur string, now time.Time) (string, error) {
	if dur == "" || dur == "always" {
		return name + ",0", nil
	}
	d, err := time.ParseDuration(dur)
	if err != nil || d < time.Minute || d > 30*24*time.Hour {
		return "", fmt.Errorf("--for %q: use always, or a time like 30m, 2h, 1h30m (1 minute .. 30 days)", dur)
	}
	return fmt.Sprintf("%s,%d", name, now.Add(d).Unix()), nil
}

// scene: list the idle animations, play one now, keep one playing for a while / always, or go back to random.
func scene(args []string) error {
	c, err := loadPaired()
	if err != nil {
		return err
	}
	if len(args) == 0 || args[0] == "list" {
		var list []sceneInfo
		if err := clock.GetJSON(c.URL, c.Token, "/api/scenes", &list, 5*time.Second); err != nil {
			return fmt.Errorf("scene list: %v (update the clock firmware: claude-clock update)", err)
		}
		for _, s := range list {
			fmt.Printf("  %-16s %-8s %s\n", s.Name, s.Kind, s.Title)
		}
		fmt.Println("\nclaude-clock scene <name> [--for 30m|2h|always]   keep it playing;  scene <name> --once;  scene off")
		return nil
	}
	name := args[0]
	if name == "off" {
		return setPref(c, "pin", "", "Random animations again.")
	}
	if has(args, "--once") {
		if err := clock.Post(c.URL, c.Token, "/api/preview-scene?name="+name, nil, 5*time.Second); err != nil {
			return fmt.Errorf("no animation %q (claude-clock scene list)", name)
		}
		fmt.Println("Playing", name, "once.")
		return nil
	}
	dur, _ := flag(args, "--for")
	pin, err := pinValue(name, dur, time.Now())
	if err != nil {
		return err
	}
	if err := setPref(c, "pin", pin, ""); err != nil {
		return fmt.Errorf("no animation %q (claude-clock scene list)", name)
	}
	if dur == "" || dur == "always" {
		fmt.Printf("%s keeps playing on the clock screen (claude-clock scene off to stop).\n", name)
	} else {
		fmt.Printf("%s keeps playing for %s.\n", name, dur)
	}
	return nil
}

// say shows a message on the clock's info card: say "Build done" ["detail"] [--for 2m]; say off clears it.
func say(args []string) error {
	c, err := loadPaired()
	if err != nil {
		return err
	}
	var text []string
	dur := 2 * time.Minute
	for i := 0; i < len(args); i++ {
		if args[i] == "--for" && i+1 < len(args) {
			d, err := time.ParseDuration(args[i+1])
			if err != nil || d < 5*time.Second || d > 24*time.Hour {
				return fmt.Errorf("--for %q: a time like 30s, 2m, 1h (5 s .. 24 h)", args[i+1])
			}
			dur = d
			i++
			continue
		}
		text = append(text, args[i])
	}
	if len(text) == 0 {
		return fmt.Errorf(`usage: claude-clock say "Build done" ["detail"] [--for 2m]   |   claude-clock say off`)
	}
	body := map[string]any{"text": text[0], "for": int(dur.Seconds())}
	if text[0] == "off" && len(text) == 1 {
		body = map[string]any{"text": "", "for": 0}
	}
	if len(text) > 1 {
		body["detail"] = strings.Join(text[1:], " ")
	}
	if n := len([]rune(body["text"].(string))); n > 40 {
		return fmt.Errorf("the message is %d characters; keep it to 40 (put the rest in the detail)", n)
	}
	if d, _ := body["detail"].(string); len([]rune(d)) > 40 {
		return fmt.Errorf("the detail is %d characters; keep it to 40", len([]rune(d)))
	}
	b, _ := json.Marshal(body)
	if err := clock.Post(c.URL, c.Token, "/api/say", b, 5*time.Second); err != nil {
		return fmt.Errorf("say: %v (update the clock firmware: claude-clock update)", err)
	}
	return nil
}
