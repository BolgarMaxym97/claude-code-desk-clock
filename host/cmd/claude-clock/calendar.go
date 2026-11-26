package main

// Next meetings from the calendar's secret iCal address. The address stays in this computer's config (it opens the
// whole calendar without a password); the clock only gets start / end / title of the next meetings.

import (
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"os"
	"strings"
	"time"
	"unicode/utf8"

	"github.com/BolgarMaxym97/claude-code-desk-clock/host/internal/cfg"
	"github.com/BolgarMaxym97/claude-code-desk-clock/host/internal/clock"
	"github.com/BolgarMaxym97/claude-code-desk-clock/host/internal/ics"
)

const (
	calendarEvery  = 5 * time.Minute
	calendarAhead  = 12 * time.Hour
	calendarMax    = 8
	titleMaxBytes  = 40
	calendarLimit  = 32 << 20 // big calendars export years of history
	calendarFetchT = 20 * time.Second
)

type meeting struct {
	S  int64  `json:"s"`  // start, unix seconds
	E  int64  `json:"e"`  // end
	T  string `json:"t"`  // title ("" when titles are hidden)
	HM string `json:"hm"` // local start time "14:00"
}

// cutTitle shortens to n bytes without breaking a UTF-8 letter.
func cutTitle(s string, n int) string {
	s = strings.Join(strings.Fields(s), " ")
	if len(s) <= n {
		return s
	}
	s = s[:n]
	for !utf8.ValidString(s) {
		s = s[:len(s)-1]
	}
	return strings.TrimSpace(s) + "…"
}

var fetchCalendar = func(url string) (io.ReadCloser, error) {
	cl := http.Client{Timeout: calendarFetchT}
	r, err := cl.Get(url)
	if err != nil {
		return nil, errors.New("calendar not reachable")
	}
	if r.StatusCode != 200 {
		r.Body.Close()
		return nil, fmt.Errorf("calendar answered HTTP %d (is the secret address still valid?)", r.StatusCode)
	}
	return r.Body, nil
}

// upcoming: the next meetings (ongoing included) for the clock.
func upcoming(c cfg.Config, now time.Time) ([]meeting, error) {
	body, err := fetchCalendar(c.Calendar)
	if err != nil {
		return nil, err
	}
	defer body.Close()
	cal, err := ics.Parse(io.LimitReader(body, calendarLimit))
	if err != nil {
		return nil, err
	}
	var out []meeting
	for _, o := range ics.Expand(cal, now, now.Add(calendarAhead), c.CalEmail) {
		m := meeting{S: o.Start.Unix(), E: o.End.Unix(), HM: o.Start.Local().Format("15:04")}
		if !c.CalNoTitles {
			m.T = cutTitle(o.Title, titleMaxBytes)
		}
		out = append(out, m)
		if len(out) == calendarMax {
			break
		}
	}
	return out, nil
}

type calState struct {
	At    time.Time `json:"at"`
	Count int       `json:"count"`
	Next  string    `json:"next,omitempty"`
	Error string    `json:"error,omitempty"`
}

// syncCalendar fetches and sends; the outcome goes to calendar.state for `claude-clock status`.
func syncCalendar(c cfg.Config, timeout time.Duration) (calState, error) {
	st := calState{At: time.Now()}
	ms, err := upcoming(c, time.Now())
	if err == nil {
		body, _ := json.Marshal(map[string]any{"meetings": nonNil(ms)})
		err = clock.Post(c.URL, c.Token, "/api/info", body, timeout)
	}
	if err != nil {
		st.Error = err.Error()
	} else {
		st.Count = len(ms)
		if len(ms) > 0 {
			st.Next = ms[0].HM
		}
	}
	b, _ := json.Marshal(st)
	os.WriteFile(cfg.File("calendar.state"), b, 0o600)
	return st, err
}

func nonNil(ms []meeting) []meeting {
	if ms == nil {
		return []meeting{}
	}
	return ms
}

// maybeCalendar: from hooks and the statusline, at most every 5 min, in the detached sender.
func maybeCalendar(c cfg.Config) {
	if c.Calendar != "" && throttle("calendar.stamp", calendarEvery) {
		job, _ := json.Marshal(sendJob{Path: "calendar"})
		spawnSend(job)
	}
}

func validCalendarURL(s string) bool {
	return strings.HasPrefix(s, "https://") && !strings.ContainsAny(s, " \t\n")
}

func setCalendar(c *cfg.Config, v string) error {
	v = strings.TrimSpace(v)
	if strings.EqualFold(v, "off") {
		c.Calendar = ""
		if err := cfg.Save(*c); err != nil {
			return err
		}
		body, _ := json.Marshal(map[string]any{"meetings": []meeting{}})
		clock.Post(c.URL, c.Token, "/api/info", body, 5*time.Second)
		os.Remove(cfg.File("calendar.state"))
		fmt.Println("Calendar off.")
		return nil
	}
	if strings.HasPrefix(v, "webcal://") {
		v = "https://" + strings.TrimPrefix(v, "webcal://")
	}
	if !validCalendarURL(v) {
		return errors.New("use the calendar's secret address in iCal format (https://…/basic.ics)")
	}
	c.Calendar = v
	st, err := syncCalendar(*c, 5*time.Second)
	if err != nil {
		return err
	}
	if err := cfg.Save(*c); err != nil {
		return err
	}
	touch("calendar.stamp")
	if st.Count == 0 {
		fmt.Println("Calendar connected: no meetings in the next 12 hours.")
	} else {
		fmt.Printf("Calendar connected: %d meetings in the next 12 hours, the next at %s.\n", st.Count, st.Next)
	}
	return nil
}

func setCalendarEmail(c *cfg.Config, v string) error {
	v = strings.ToLower(strings.TrimSpace(v))
	if v == "off" {
		v = ""
	}
	if v != "" && (!strings.Contains(v, "@") || strings.ContainsAny(v, " \t")) {
		return errors.New("that is not an email address")
	}
	c.CalEmail = v
	if err := cfg.Save(*c); err != nil {
		return err
	}
	if v == "" {
		fmt.Println("Declined meetings are shown.")
	} else {
		fmt.Println("Meetings you declined are hidden.")
	}
	return nil
}

func setCalendarTitles(c *cfg.Config, v string) error {
	switch strings.ToLower(strings.TrimSpace(v)) {
	case "on":
		c.CalNoTitles = false
	case "off":
		c.CalNoTitles = true
	default:
		return errors.New("use --calendar-titles on or off")
	}
	if err := cfg.Save(*c); err != nil {
		return err
	}
	if c.Calendar != "" {
		syncCalendar(*c, 5*time.Second)
	}
	fmt.Println("Meeting titles on the clock:", strings.ToLower(v))
	return nil
}

// askCalendar: optional during setup (Enter skips).
func askCalendar(c *cfg.Config, args []string, auto bool) {
	url, ok := flag(args, "--calendar")
	if !ok && !auto {
		url = ask("Calendar: its secret address in iCal format, for the next meeting (Enter = skip): ")
	}
	if strings.TrimSpace(url) == "" {
		return
	}
	if err := setCalendar(c, url); err != nil {
		fmt.Println(" ", err)
		return
	}
	mail, ok := flag(args, "--calendar-email")
	if !ok && !auto {
		mail = ask("Your email in that calendar, to hide meetings you declined (Enter = show all): ")
	}
	if strings.TrimSpace(mail) != "" {
		if err := setCalendarEmail(c, mail); err != nil {
			fmt.Println(" ", err)
		}
	}
}

func calendarStatus(c cfg.Config) string {
	if c.Calendar == "" {
		return "off"
	}
	var st calState
	b, err := os.ReadFile(cfg.File("calendar.state"))
	if err != nil || json.Unmarshal(b, &st) != nil {
		return "connected, not synced yet"
	}
	ago := agoText(time.Since(st.At))
	if st.Error != "" {
		return fmt.Sprintf("error %s: %s", ago, st.Error)
	}
	if st.Count == 0 {
		return fmt.Sprintf("synced %s, no meetings in the next 12 h", ago)
	}
	return fmt.Sprintf("synced %s, next meeting at %s", ago, st.Next)
}

// agoText: "just now", "4 min", "2 h 5 min".
func agoText(d time.Duration) string {
	m := int(d / time.Minute)
	switch {
	case m < 1:
		return "just now"
	case m < 60:
		return fmt.Sprintf("%d min ago", m)
	}
	return fmt.Sprintf("%d h %d min ago", m/60, m%60)
}
