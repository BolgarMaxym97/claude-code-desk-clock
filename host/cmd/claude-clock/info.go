package main

// Weather (OpenWeather) and air-raid alerts (Ukraine, ubilling): optional, both can be switched off.

import (
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"os"
	"path/filepath"
	"regexp"
	"runtime"
	"sort"
	"strconv"
	"strings"
	"time"

	"github.com/BolgarMaxym97/claude-code-desk-clock/host/internal/cfg"
	"github.com/BolgarMaxym97/claude-code-desk-clock/host/internal/clock"
)

const alertsURL = "https://ubilling.net.ua/aerialalerts/"

var (
	owmKeyRe = regexp.MustCompile(`^[0-9a-f]{32}$`)
	latLonRe = regexp.MustCompile(`^\s*(-?\d{1,2}(?:\.\d+)?)\s*,\s*(-?\d{1,3}(?:\.\d+)?)\s*$`)
)

func postSettings(c cfg.Config, kv map[string]string) error {
	body, _ := json.Marshal(kv)
	err := clock.Post(c.URL, c.Token, "/api/settings", body, 5*time.Second)
	var he *clock.HTTPError
	if errors.As(err, &he) && he.Code == 400 {
		return fmt.Errorf("the clock rejected it (this clock firmware may be too old: run `claude-clock update`)")
	}
	return err
}

// normKey: OpenWeather keys are 32 hex chars ("off" / "" switch the weather off).
func normKey(s string) (string, bool) {
	s = strings.ToLower(strings.TrimSpace(s))
	if s == "off" || s == "" {
		return "", true
	}
	return s, owmKeyRe.MatchString(s)
}

// parseLatLon accepts "50.45, 30.52" and returns the clock form "50.45,30.52" (2 decimals ≈ 1 km).
func parseLatLon(s string) (string, bool) {
	m := latLonRe.FindStringSubmatch(s)
	if m == nil {
		return "", false
	}
	lat, _ := strconv.ParseFloat(m[1], 64)
	lon, _ := strconv.ParseFloat(m[2], 64)
	if lat < -90 || lat > 90 || lon < -180 || lon > 180 {
		return "", false
	}
	return strconv.FormatFloat(lat, 'f', 2, 64) + "," + strconv.FormatFloat(lon, 'f', 2, 64), true
}

var getURL = func(u string) ([]byte, int, error) {
	cl := http.Client{Timeout: 8 * time.Second}
	r, err := cl.Get(u)
	if err != nil {
		return nil, 0, err
	}
	defer r.Body.Close()
	b, err := io.ReadAll(io.LimitReader(r.Body, 1<<20))
	return b, r.StatusCode, err
}

// geocode: city name -> "lat,lon" via OpenWeather (needs an active key).
func geocode(city, key string) (string, string, error) {
	b, code, err := getURL("https://api.openweathermap.org/geo/1.0/direct?limit=1&q=" + url.QueryEscape(city) + "&appid=" + key)
	if err != nil {
		return "", "", err
	}
	if code == 401 {
		return "", "", errors.New("OpenWeather does not accept this key yet (new keys work after up to 2 hours)")
	}
	var r []struct {
		Name    string  `json:"name"`
		Country string  `json:"country"`
		Lat     float64 `json:"lat"`
		Lon     float64 `json:"lon"`
	}
	if code != 200 || json.Unmarshal(b, &r) != nil {
		return "", "", fmt.Errorf("OpenWeather answered HTTP %d", code)
	}
	if len(r) == 0 {
		return "", "", fmt.Errorf("city %q not found", city)
	}
	ll, _ := parseLatLon(strconv.FormatFloat(r[0].Lat, 'f', 4, 64) + "," + strconv.FormatFloat(r[0].Lon, 'f', 4, 64))
	return ll, r[0].Name + ", " + r[0].Country, nil
}

// setCity: "lat,lon" directly, or a city name looked up with the key.
func setCity(c cfg.Config, city string) error {
	ll, name := "", ""
	if v, ok := parseLatLon(city); ok {
		ll, name = v, v
	} else {
		if c.OWM == "" {
			return errors.New("set the OpenWeather key first: claude-clock config --weather-key <key>")
		}
		var err error
		if ll, name, err = geocode(city, c.OWM); err != nil {
			return fmt.Errorf("%v; you can give coordinates instead: --city 50.45,30.52", err)
		}
	}
	if err := postSettings(c, map[string]string{"loc": ll}); err != nil {
		return err
	}
	fmt.Println("Weather for", name)
	return nil
}

func setWeatherKey(c *cfg.Config, v string) error {
	key, ok := normKey(v)
	if !ok {
		return fmt.Errorf("that is not an OpenWeather API key (32 letters and digits)")
	}
	if err := postSettings(*c, map[string]string{"owm": key}); err != nil {
		return err
	}
	c.OWM = key // kept locally (file mode 0600) for city look-ups
	if err := cfg.Save(*c); err != nil {
		return err
	}
	if key == "" {
		fmt.Println("Weather off.")
	} else {
		fmt.Println("OpenWeather key saved on the clock.")
	}
	return nil
}

// alertRegions: the region names the alert source knows (Ukrainian names, e.g. "м. Київ").
func alertRegions() ([]string, error) {
	b, code, err := getURL(alertsURL)
	if err == nil && code == 429 { // the source allows ~2 requests/s per address: wait and try once more
		time.Sleep(2 * time.Second)
		b, code, err = getURL(alertsURL)
	}
	if err != nil {
		return nil, err
	}
	var r struct {
		States map[string]json.RawMessage `json:"states"`
	}
	if code != 200 || json.Unmarshal(b, &r) != nil || len(r.States) == 0 {
		return nil, fmt.Errorf("alert source answered HTTP %d", code)
	}
	var out []string
	for k := range r.States {
		out = append(out, k)
	}
	sort.Strings(out)
	return out, nil
}

// matchRegion: exact name, a Kyiv alias, or a unique part of a name ("львів" -> "Львівська область").
func matchRegion(in string, regions []string) (string, error) {
	q := strings.ToLower(strings.TrimSpace(in))
	switch q {
	case "kyiv", "kiev", "київ", "киев":
		q = "м. київ"
	}
	var hits []string
	for _, r := range regions {
		l := strings.ToLower(r)
		if l == q {
			return r, nil
		}
		if strings.Contains(l, q) {
			hits = append(hits, r)
		}
	}
	if len(hits) == 1 {
		return hits[0], nil
	}
	if len(hits) > 1 {
		return "", fmt.Errorf("%q matches several regions: %s", in, strings.Join(hits, ", "))
	}
	return "", fmt.Errorf("unknown region %q (see: claude-clock config --alerts list)", in)
}

func setAlerts(c cfg.Config, v string) error {
	if strings.EqualFold(strings.TrimSpace(v), "off") {
		if err := postSettings(c, map[string]string{"region": ""}); err != nil {
			return err
		}
		fmt.Println("Air-raid alerts off.")
		return nil
	}
	regions, err := alertRegions()
	if err != nil {
		return err
	}
	if strings.EqualFold(strings.TrimSpace(v), "list") {
		fmt.Println(strings.Join(regions, "\n"))
		return nil
	}
	r, err := matchRegion(v, regions)
	if err != nil {
		return err
	}
	if err := postSettings(c, map[string]string{"region": r}); err != nil {
		return err
	}
	fmt.Println("Air-raid alerts for", r)
	return nil
}

// zoneName: the IANA name of this computer's time zone ("" if unknown).
func zoneName() string {
	if z := os.Getenv("TZ"); z != "" {
		return strings.TrimPrefix(z, ":")
	}
	if runtime.GOOS != "windows" {
		if p, err := filepath.EvalSymlinks("/etc/localtime"); err == nil {
			if i := strings.Index(p, "zoneinfo/"); i >= 0 {
				return p[i+9:]
			}
		}
		if b, err := os.ReadFile("/etc/timezone"); err == nil {
			return strings.TrimSpace(string(b))
		}
	}
	return ""
}

func inUkraine(zone string) bool {
	switch zone {
	case "Europe/Kyiv", "Europe/Kiev", "Europe/Uzhgorod", "Europe/Zaporozhye", "Europe/Simferopol":
		return true
	}
	return false
}

// askInfo: optional weather and alerts during setup (Enter skips; --yes skips unless flags are given).
func askInfo(c *cfg.Config, args []string, auto bool) {
	key, keyFlag := flag(args, "--weather-key")
	if !keyFlag && !auto {
		key = ask("Weather: OpenWeather API key (free at openweathermap.org/api; Enter = no weather): ")
	}
	if strings.TrimSpace(key) != "" {
		if err := setWeatherKey(c, key); err != nil {
			fmt.Println(" ", err)
		} else {
			city, cityFlag := flag(args, "--city")
			def := cityFromZone(zoneName())
			if !cityFlag && !auto {
				city = ask(fmt.Sprintf("City for the weather, or lat,lon [%s]: ", def))
			}
			if strings.TrimSpace(city) == "" {
				city = def
			}
			if city != "" {
				if err := setCity(*c, city); err != nil {
					fmt.Println(" ", err)
				}
			}
		}
	}
	region, regFlag := flag(args, "--alerts")
	if !regFlag && !auto && inUkraine(zoneName()) {
		region = ask("Air-raid alerts (Ukraine): region [м. Київ], 'list' to see all, 'off' = none: ")
		if strings.TrimSpace(region) == "" {
			region = "м. Київ"
		}
		for strings.EqualFold(strings.TrimSpace(region), "list") {
			setAlerts(*c, "list")
			region = ask("Region: ")
		}
	}
	if strings.TrimSpace(region) != "" {
		if err := setAlerts(*c, region); err != nil {
			fmt.Println(" ", err)
		}
	}
}

func cityFromZone(z string) string {
	if i := strings.LastIndex(z, "/"); i >= 0 && !strings.HasPrefix(z, "Etc/") {
		return strings.ReplaceAll(z[i+1:], "_", " ")
	}
	return ""
}
