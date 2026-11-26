// Package cfg stores where the clock is and its token (file mode 0600), plus small state files.
package cfg

import (
	"bufio"
	"encoding/json"
	"errors"
	"os"
	"path/filepath"
	"runtime"
	"strings"
)

// Config of this computer's connection to the clock.
type Config struct {
	URL   string `json:"url"`            // http://<ip>  (IP: name lookups are too slow for hooks)
	Host  string `json:"host,omitempty"` // mDNS name used to find the clock again if its IP changes
	Token string `json:"token"`
}

// Dir: $CLOCK_CONFIG_DIR, else %APPDATA%\claude-clock (Windows) or $XDG_CONFIG_HOME|~/.config/claude-clock.
func Dir() string {
	if d := os.Getenv("CLOCK_CONFIG_DIR"); d != "" {
		return d
	}
	if runtime.GOOS == "windows" {
		if d, err := os.UserConfigDir(); err == nil {
			return filepath.Join(d, "claude-clock")
		}
	}
	if d := os.Getenv("XDG_CONFIG_HOME"); d != "" {
		return filepath.Join(d, "claude-clock")
	}
	home, _ := os.UserHomeDir()
	return filepath.Join(home, ".config", "claude-clock")
}

func path() string { return filepath.Join(Dir(), "config.json") }

// Load reads config.json; an older KEY=VALUE "config" file (first versions) is read too. Never executed.
func Load() (Config, error) {
	var c Config
	b, err := os.ReadFile(path())
	if err == nil {
		err = json.Unmarshal(b, &c)
		return c, err
	}
	f, err2 := os.Open(filepath.Join(Dir(), "config"))
	if err2 != nil {
		return c, err
	}
	defer f.Close()
	sc := bufio.NewScanner(f)
	for sc.Scan() {
		k, v, ok := strings.Cut(sc.Text(), "=")
		if !ok {
			continue
		}
		switch k {
		case "CLOCK_URL":
			c.URL = v
		case "CLOCK_TOKEN":
			c.Token = v
		}
	}
	if c.URL == "" {
		return c, errors.New("old config without CLOCK_URL")
	}
	return c, nil
}

// Save writes config.json readable only by this user.
func Save(c Config) error {
	if err := os.MkdirAll(Dir(), 0o700); err != nil {
		return err
	}
	b, _ := json.MarshalIndent(c, "", "  ")
	tmp := path() + ".tmp"
	if err := os.WriteFile(tmp, append(b, '\n'), 0o600); err != nil {
		return err
	}
	return os.Rename(tmp, path())
}

// File returns a path inside the config dir (state: statusline.orig, throttle stamps).
func File(name string) string { return filepath.Join(Dir(), name) }
