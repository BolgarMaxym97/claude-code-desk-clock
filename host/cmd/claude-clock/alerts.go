package main

// Alert levels from alerts.in.ua (drones / missiles, threat, partial alert). The service is HTTPS-only, which is
// too heavy for the clock, so the computer polls it while Claude Code runs and sends the level to the clock.
// The token is stored on the clock (settings page or `config --alerts-token`); the clock itself keeps polling
// ubilling, which says only whether there is an alert.

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"strings"
	"time"

	"github.com/BolgarMaxym97/claude-code-desk-clock/host/internal/cfg"
	"github.com/BolgarMaxym97/claude-code-desk-clock/host/internal/clock"
)

const (
	alertsAPI   = "https://api.alerts.in.ua/v1/alerts/active.json"
	alertsEvery = 30 * time.Second // the service allows 8-12 requests a minute per address
	alertsIdle  = 10 * time.Minute // no token / no region on the clock: ask the clock again later
)

type activeAlert struct {
	LocationTitle  string `json:"location_title"`
	LocationType   string `json:"location_type"`
	LocationOblast string `json:"location_oblast"`
	AlertType      string `json:"alert_type"`
	AlertLevel     string `json:"alert_level"`
	Threats        []struct {
		ThreatType string `json:"threat_type"`
	} `json:"threats"`
}

func sameRegion(a, b string) bool {
	n := func(s string) string { return strings.TrimPrefix(strings.TrimSpace(s), "м. ") }
	return a != "" && n(a) == n(b)
}

// alertLevel turns the active alerts into the clock's level for one region:
// none | threat (yellow only) | partial (only some districts) | missile | drone | alert.
func alertLevel(body []byte, region string) (string, error) {
	var in struct {
		Alerts []activeAlert `json:"alerts"`
	}
	if err := json.Unmarshal(body, &in); err != nil {
		return "", err
	}
	found, whole, red, missiles, drones := false, false, false, false, false
	for _, a := range in.Alerts {
		if a.AlertType != "air_raid" || !(sameRegion(a.LocationOblast, region) || sameRegion(a.LocationTitle, region)) {
			continue
		}
		found = true
		if a.LocationType == "oblast" || (a.LocationType == "city" && sameRegion(a.LocationTitle, region)) {
			whole = true
		}
		if a.AlertLevel != "yellow" {
			red = true
		}
		for _, t := range a.Threats {
			switch {
			case strings.Contains(t.ThreatType, "missile"):
				missiles = true
			case t.ThreatType == "drones":
				drones = true
			}
		}
	}
	switch {
	case !found:
		return "none", nil
	case !red:
		return "threat", nil
	case !whole:
		return "partial", nil
	case missiles:
		return "missile", nil
	case drones:
		return "drone", nil
	}
	return "alert", nil
}

func maybeAlerts(c cfg.Config) {
	for _, f := range []string{"alerts.off", "alerts.wait"} { // no token / region, or the service refused: wait
		if st, err := os.Stat(cfg.File(f)); err == nil && time.Since(st.ModTime()) < alertsIdle {
			return
		}
	}
	if throttle("alerts.stamp", alertsEvery) {
		job, _ := json.Marshal(sendJob{Path: "alerts"})
		spawnSend(job)
	}
}

// syncAlerts runs in the detached sender: token and region from the clock, the level back to it.
func syncAlerts(c cfg.Config) {
	var s struct {
		Region string `json:"region"`
		Atok   any    `json:"atok"`
	}
	if clock.GetJSON(c.URL, c.Token, "/api/settings?secret=1", &s, 5*time.Second) != nil {
		return
	}
	tok, _ := s.Atok.(string)
	if s.Region == "" || tok == "" {
		os.WriteFile(cfg.File("alerts.off"), nil, 0o600)
		return
	}
	os.Remove(cfg.File("alerts.off"))
	level, err := fetchAlertLevel(tok, s.Region)
	os.WriteFile(cfg.File("alerts.state"), []byte(alertsState(level, err)), 0o600)
	if err != nil && refused(err) {
		os.WriteFile(cfg.File("alerts.wait"), nil, 0o600) // bad token or too many requests: never hammer the service
	} else {
		os.Remove(cfg.File("alerts.wait"))
	}
	if err != nil {
		return // the clock falls back to ubilling 3 minutes after the last level
	}
	body, _ := json.Marshal(map[string]string{"alert": level})
	clock.Post(c.URL, c.Token, "/api/info", body, 5*time.Second)
}

func refused(err error) bool {
	m := err.Error()
	return m == "HTTP 401" || m == "HTTP 403" || m == "HTTP 429"
}

func fetchAlertLevel(token, region string) (string, error) {
	req, _ := http.NewRequest("GET", alertsAPI, nil)
	req.Header.Set("Authorization", "Bearer "+token)
	resp, err := (&http.Client{Timeout: 10 * time.Second}).Do(req)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()
	if resp.StatusCode != 200 {
		return "", fmt.Errorf("HTTP %d", resp.StatusCode)
	}
	b, err := io.ReadAll(io.LimitReader(resp.Body, 4<<20))
	if err != nil {
		return "", err
	}
	return alertLevel(b, region)
}

// alertsState is one line for `claude-clock status`: "drone, 12:04" or "error HTTP 401, 12:04".
func alertsState(level string, err error) string {
	at := time.Now().Format("15:04")
	if err != nil {
		return "error " + err.Error() + ", " + at
	}
	return level + ", " + at
}

func alertsStatus() string {
	if _, err := os.Stat(cfg.File("alerts.off")); err == nil {
		return "no token"
	}
	b, err := os.ReadFile(cfg.File("alerts.state"))
	if err != nil {
		return "not checked yet (runs while Claude Code is active)"
	}
	s := strings.TrimSpace(string(b))
	if strings.Contains(s, "HTTP 401") {
		return s + " (token rejected)"
	}
	return s
}

// setAlertsToken stores the alerts.in.ua token on the clock ("off" removes it).
func setAlertsToken(c *cfg.Config, v string) error {
	v = strings.TrimSpace(v)
	if strings.EqualFold(v, "off") {
		v = ""
	}
	if err := setPref(*c, "atok", v, ""); err != nil {
		return fmt.Errorf("alerts token: %v (16-64 letters, digits, - or _)", err)
	}
	os.Remove(cfg.File("alerts.off"))
	os.Remove(cfg.File("alerts.wait"))
	os.Remove(cfg.File("alerts.state"))
	if v == "" {
		fmt.Println("Alert levels off: the clock shows only whether there is an alert.")
	} else {
		fmt.Println("Alerts token saved on the clock. Levels show while Claude Code runs on this computer.")
	}
	return nil
}
