// Package ics reads an iCalendar feed (a calendar's secret iCal address) and lists the meetings in a time window.
// Supported: TZID (IANA and common Windows names), UTC and floating times, all-day events (skipped), DURATION,
// RRULE with FREQ DAILY/WEEKLY/MONTHLY/YEARLY, INTERVAL, COUNT, UNTIL, BYDAY (with ordinals), BYMONTHDAY,
// EXDATE, RECURRENCE-ID overrides, STATUS:CANCELLED and the user's own PARTSTAT=DECLINED.
package ics

import (
	"bufio"
	"errors"
	"io"
	"sort"
	"strconv"
	"strings"
	"time"
	_ "time/tzdata" // Windows has no zoneinfo database of its own
)

type Event struct {
	UID, Summary, Status, RRule string
	Start, End                  time.Time
	AllDay                      bool
	RecurrenceID                time.Time // zero: the series (or a single event)
	ExDates                     []time.Time
	Attendees                   map[string]string // lower-case email -> PARTSTAT
}

type Calendar struct{ Events []Event }

type Occurrence struct {
	Start, End time.Time
	Title      string
}

var windowsZones = map[string]string{
	"FLE Standard Time": "Europe/Kyiv", "GTB Standard Time": "Europe/Bucharest", "E. Europe Standard Time": "Europe/Chisinau",
	"W. Europe Standard Time": "Europe/Berlin", "Central European Standard Time": "Europe/Warsaw",
	"Central Europe Standard Time": "Europe/Budapest", "Romance Standard Time": "Europe/Paris",
	"GMT Standard Time": "Europe/London", "Greenwich Standard Time": "Atlantic/Reykjavik", "UTC": "UTC",
	"Eastern Standard Time": "America/New_York", "Central Standard Time": "America/Chicago",
	"Mountain Standard Time": "America/Denver", "Pacific Standard Time": "America/Los_Angeles",
	"Russian Standard Time": "Europe/Moscow", "Turkey Standard Time": "Europe/Istanbul", "Israel Standard Time": "Asia/Jerusalem",
	"India Standard Time": "Asia/Kolkata", "China Standard Time": "Asia/Shanghai", "Tokyo Standard Time": "Asia/Tokyo",
	"AUS Eastern Standard Time": "Australia/Sydney",
}

func location(name string, def *time.Location) *time.Location {
	name = strings.Trim(name, `"`)
	if w, ok := windowsZones[name]; ok {
		name = w
	}
	if l, err := time.LoadLocation(name); err == nil && name != "" {
		return l
	}
	return def
}

type prop struct {
	name   string
	params map[string]string
	value  string
}

func parseProp(line string) prop {
	p := prop{params: map[string]string{}}
	inQ := false
	colon := -1
	for i, r := range line {
		if r == '"' {
			inQ = !inQ
		} else if r == ':' && !inQ {
			colon = i
			break
		}
	}
	if colon < 0 {
		return p
	}
	head := line[:colon]
	p.value = line[colon+1:]
	parts := strings.Split(head, ";")
	p.name = strings.ToUpper(parts[0])
	for _, kv := range parts[1:] {
		if k, v, ok := strings.Cut(kv, "="); ok {
			p.params[strings.ToUpper(k)] = strings.Trim(v, `"`)
		}
	}
	return p
}

func unescape(s string) string {
	r := strings.NewReplacer(`\n`, " ", `\N`, " ", `\,`, ",", `\;`, ";", `\\`, `\`)
	return r.Replace(s)
}

// parseTime returns the instant and whether it is a date (all-day) value.
func parseTime(p prop, def *time.Location) (time.Time, bool) {
	v := strings.TrimSpace(p.value)
	if p.params["VALUE"] == "DATE" || len(v) == 8 {
		t, err := time.ParseInLocation("20060102", v, def)
		return t, err == nil
	}
	if strings.HasSuffix(v, "Z") {
		t, _ := time.Parse("20060102T150405Z", v)
		return t, false
	}
	loc := def
	if tz := p.params["TZID"]; tz != "" {
		loc = location(tz, def)
	}
	t, _ := time.ParseInLocation("20060102T150405", v, loc)
	return t, false
}

func parseDuration(s string) time.Duration {
	s = strings.TrimPrefix(strings.TrimPrefix(s, "+"), "P")
	var d time.Duration
	num := 0
	for _, r := range s {
		switch {
		case r >= '0' && r <= '9':
			num = num*10 + int(r-'0')
		case r == 'W':
			d += time.Duration(num) * 7 * 24 * time.Hour
			num = 0
		case r == 'D':
			d += time.Duration(num) * 24 * time.Hour
			num = 0
		case r == 'H':
			d += time.Duration(num) * time.Hour
			num = 0
		case r == 'M':
			d += time.Duration(num) * time.Minute
			num = 0
		case r == 'S':
			d += time.Duration(num) * time.Second
			num = 0
		}
	}
	return d
}

// Parse reads a VCALENDAR. Anything that is not one (a login page, an error) is an error.
func Parse(r io.Reader) (Calendar, error) {
	var lines []string
	sc := bufio.NewScanner(r)
	sc.Buffer(make([]byte, 64*1024), 4*1024*1024)
	for sc.Scan() {
		l := strings.TrimRight(sc.Text(), "\r")
		if (strings.HasPrefix(l, " ") || strings.HasPrefix(l, "\t")) && len(lines) > 0 {
			lines[len(lines)-1] += l[1:] // folded line
			continue
		}
		lines = append(lines, l)
	}
	if err := sc.Err(); err != nil {
		return Calendar{}, err
	}
	if len(lines) == 0 || !strings.EqualFold(strings.TrimSpace(strings.TrimPrefix(lines[0], "\ufeff")), "BEGIN:VCALENDAR") {
		return Calendar{}, errors.New("not an iCalendar feed (is the address right?)")
	}
	def := time.Local
	var cal Calendar
	var ev *Event
	var dur time.Duration
	for _, l := range lines {
		p := parseProp(l)
		switch {
		case p.name == "X-WR-TIMEZONE":
			def = location(p.value, def)
		case p.name == "BEGIN" && strings.EqualFold(p.value, "VEVENT"):
			ev = &Event{Attendees: map[string]string{}}
			dur = 0
		case p.name == "END" && strings.EqualFold(p.value, "VEVENT") && ev != nil:
			if ev.End.IsZero() {
				if dur == 0 && ev.AllDay {
					dur = 24 * time.Hour
				}
				ev.End = ev.Start.Add(dur)
			}
			if !ev.Start.IsZero() {
				cal.Events = append(cal.Events, *ev)
			}
			ev = nil
		case ev == nil:
		case p.name == "UID":
			ev.UID = p.value
		case p.name == "SUMMARY":
			ev.Summary = strings.TrimSpace(unescape(p.value))
		case p.name == "STATUS":
			ev.Status = strings.ToUpper(p.value)
		case p.name == "RRULE":
			ev.RRule = strings.ToUpper(p.value)
		case p.name == "DTSTART":
			ev.Start, ev.AllDay = parseTime(p, def)
		case p.name == "DTEND":
			ev.End, _ = parseTime(p, def)
		case p.name == "DURATION":
			dur = parseDuration(p.value)
		case p.name == "RECURRENCE-ID":
			ev.RecurrenceID, _ = parseTime(p, def)
		case p.name == "EXDATE":
			for _, v := range strings.Split(p.value, ",") {
				q := p
				q.value = v
				t, _ := parseTime(q, def)
				ev.ExDates = append(ev.ExDates, t)
			}
		case p.name == "ATTENDEE":
			mail := strings.ToLower(strings.TrimPrefix(strings.TrimPrefix(p.value, "mailto:"), "MAILTO:"))
			ev.Attendees[mail] = strings.ToUpper(p.params["PARTSTAT"])
		}
	}
	return cal, nil
}

func (e Event) skipped(me string) bool {
	return e.Status == "CANCELLED" || e.AllDay || (me != "" && e.Attendees[strings.ToLower(me)] == "DECLINED")
}

// Expand lists the meetings overlapping [from, to), sorted by start. me (the user's email, optional) hides meetings
// the user declined.
func Expand(cal Calendar, from, to time.Time, me string) []Occurrence {
	overrides := map[string]bool{} // uid + instant of instances replaced or cancelled by a RECURRENCE-ID event
	for _, e := range cal.Events {
		if !e.RecurrenceID.IsZero() {
			overrides[e.UID+"|"+strconv.FormatInt(e.RecurrenceID.Unix(), 10)] = true
		}
	}
	var out []Occurrence
	add := func(e Event, s time.Time) {
		end := s.Add(e.End.Sub(e.Start))
		if s.Before(to) && end.After(from) {
			out = append(out, Occurrence{s, end, e.Summary})
		}
	}
	for _, e := range cal.Events {
		if e.skipped(me) {
			continue
		}
		if !e.RecurrenceID.IsZero() || e.RRule == "" {
			add(e, e.Start)
			continue
		}
		dur := e.End.Sub(e.Start)
		for _, s := range starts(e, from.Add(-dur), to) {
			if overrides[e.UID+"|"+strconv.FormatInt(s.Unix(), 10)] || excluded(e, s) {
				continue
			}
			add(e, s)
		}
	}
	sort.Slice(out, func(i, j int) bool { return out[i].Start.Before(out[j].Start) })
	return out
}

func excluded(e Event, s time.Time) bool {
	for _, x := range e.ExDates {
		if x.Equal(s) {
			return true
		}
	}
	return false
}

var weekdays = map[string]time.Weekday{"SU": time.Sunday, "MO": time.Monday, "TU": time.Tuesday, "WE": time.Wednesday,
	"TH": time.Thursday, "FR": time.Friday, "SA": time.Saturday}

type byDay struct {
	n  int // 0: every such weekday; 2: the 2nd; -1: the last
	wd time.Weekday
}

// starts lists the series' start instants from the first one up to `to`, keeping those at or after `from`.
func starts(e Event, from, to time.Time) []time.Time {
	rule := map[string]string{}
	for _, kv := range strings.Split(e.RRule, ";") {
		if k, v, ok := strings.Cut(kv, "="); ok {
			rule[k] = v
		}
	}
	base := e.Start
	loc := base.Location()
	interval, _ := strconv.Atoi(rule["INTERVAL"])
	if interval < 1 {
		interval = 1
	}
	count, _ := strconv.Atoi(rule["COUNT"])
	var until time.Time
	if u := rule["UNTIL"]; u != "" {
		if strings.HasSuffix(u, "Z") {
			until, _ = time.Parse("20060102T150405Z", u)
		} else if len(u) == 8 {
			until, _ = time.ParseInLocation("20060102", u, loc)
			until = until.Add(24*time.Hour - time.Second)
		} else {
			until, _ = time.ParseInLocation("20060102T150405", u, loc)
		}
	}
	var days []byDay
	for _, d := range strings.Split(rule["BYDAY"], ",") {
		if len(d) < 2 {
			continue
		}
		n, _ := strconv.Atoi(strings.TrimPrefix(d[:len(d)-2], "+"))
		if wd, ok := weekdays[d[len(d)-2:]]; ok {
			days = append(days, byDay{n, wd})
		}
	}
	var monthDays []int
	for _, d := range strings.Split(rule["BYMONTHDAY"], ",") {
		if n, err := strconv.Atoi(d); err == nil && n != 0 {
			monthDays = append(monthDays, n)
		}
	}
	b := base.In(loc)
	at := func(y int, m time.Month, d int) time.Time {
		return time.Date(y, m, d, b.Hour(), b.Minute(), b.Second(), 0, loc)
	}
	var out []time.Time
	n := 0
	// emit returns false when the series is over
	emit := func(t time.Time) bool {
		if t.Before(base) {
			return true
		}
		if (!until.IsZero() && t.After(until)) || !t.Before(to) || (count > 0 && n >= count) {
			return false
		}
		n++
		if !t.Before(from) {
			out = append(out, t)
		}
		return true
	}
	for k := 0; k < 100000; k++ {
		var cands []time.Time
		switch rule["FREQ"] {
		case "DAILY":
			cands = []time.Time{at(b.Year(), b.Month(), b.Day()+k*interval)}
		case "WEEKLY":
			monday := b.Day() - (int(b.Weekday())+6)%7 + 7*k*interval
			wds := []time.Weekday{b.Weekday()}
			if len(days) > 0 {
				wds = nil
				for _, d := range days {
					wds = append(wds, d.wd)
				}
			}
			for _, wd := range wds {
				cands = append(cands, at(b.Year(), b.Month(), monday+(int(wd)+6)%7))
			}
		case "MONTHLY":
			first := time.Date(b.Year(), b.Month()+time.Month(k*interval), 1, 0, 0, 0, 0, loc)
			y, m := first.Year(), first.Month()
			last := time.Date(y, m+1, 0, 0, 0, 0, 0, loc).Day()
			switch {
			case len(monthDays) > 0:
				for _, d := range monthDays {
					if d < 0 {
						d = last + 1 + d
					}
					if d >= 1 && d <= last {
						cands = append(cands, at(y, m, d))
					}
				}
			case len(days) > 0:
				for _, d := range days {
					var hits []int
					for day := 1; day <= last; day++ {
						if time.Date(y, m, day, 0, 0, 0, 0, loc).Weekday() == d.wd {
							hits = append(hits, day)
						}
					}
					switch {
					case d.n == 0:
						for _, h := range hits {
							cands = append(cands, at(y, m, h))
						}
					case d.n > 0 && d.n <= len(hits):
						cands = append(cands, at(y, m, hits[d.n-1]))
					case d.n < 0 && -d.n <= len(hits):
						cands = append(cands, at(y, m, hits[len(hits)+d.n]))
					}
				}
			default:
				if b.Day() <= last {
					cands = append(cands, at(y, m, b.Day()))
				}
			}
		case "YEARLY":
			t := at(b.Year()+k*interval, b.Month(), b.Day())
			if t.Day() == b.Day() { // Feb 29 only in leap years
				cands = []time.Time{t}
			}
		default:
			return out
		}
		sort.Slice(cands, func(i, j int) bool { return cands[i].Before(cands[j]) })
		for _, c := range cands {
			if !emit(c) {
				return out
			}
		}
	}
	return out
}
