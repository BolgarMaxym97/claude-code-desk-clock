// Package tz turns the computer's timezone into a POSIX TZ string the clock understands
// (the clock has no timezone database; e.g. Europe/Kyiv -> "EET-2EEST,M3.5.0/3,M10.5.0/4").
package tz

import (
	"fmt"
	"time"
	_ "time/tzdata" // IANA names work on Windows too
)

// Posix describes the rules loc follows in the given year.
func Posix(loc *time.Location, year int) string {
	start := time.Date(year, 1, 1, 0, 0, 0, 0, time.UTC)
	var trans []time.Time
	prev := offset(start, loc)
	for d := 1; d <= 366; d++ {
		t := start.Add(time.Duration(d) * 24 * time.Hour)
		if t.Year() != year && d > 365 {
			break
		}
		if o := offset(t, loc); o != prev {
			trans = append(trans, findChange(t.Add(-24*time.Hour), t, loc))
			prev = o
		}
	}
	if len(trans) != 2 {
		name, off := start.In(loc).Zone()
		return abbr(name) + hm(-off)
	}
	var toDst, toStd time.Time
	if trans[0].In(loc).IsDST() {
		toDst, toStd = trans[0], trans[1]
	} else {
		toDst, toStd = trans[1], trans[0]
	}
	stdName, stdOff := toStd.In(loc).Zone()
	dstName, dstOff := toDst.In(loc).Zone()
	s := abbr(stdName) + hm(-stdOff) + abbr(dstName)
	if dstOff-stdOff != 3600 {
		s += hm(-dstOff)
	}
	// wall-clock time of each change, in the offset in force just before it
	return s + "," + rule(toDst, stdOff) + "," + rule(toStd, dstOff)
}

// Valid matches the clock's own check (firmware settings.cpp validTz).
func Valid(s string) bool {
	if len(s) == 0 || len(s) > 48 {
		return false
	}
	for _, c := range s {
		ok := c >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z' || c >= '0' && c <= '9' ||
			c == ',' || c == '.' || c == ':' || c == '/' || c == '+' || c == '-' || c == '<' || c == '>'
		if !ok {
			return false
		}
	}
	return true
}

func offset(t time.Time, loc *time.Location) int {
	_, o := t.In(loc).Zone()
	return o
}

func findChange(lo, hi time.Time, loc *time.Location) time.Time { // first instant with the new offset
	before := offset(lo, loc)
	for hi.Sub(lo) > time.Minute {
		mid := lo.Add(hi.Sub(lo) / 2).Truncate(time.Minute)
		if offset(mid, loc) == before {
			lo = mid
		} else {
			hi = mid
		}
	}
	return hi
}

func rule(t time.Time, offBefore int) string {
	w := t.UTC().Add(time.Duration(offBefore) * time.Second) // wall clock before the change
	days := time.Date(w.Year(), w.Month()+1, 0, 0, 0, 0, 0, time.UTC).Day()
	week := (w.Day()-1)/7 + 1
	if w.Day()+7 > days {
		week = 5 // "last <weekday> of the month"
	}
	secs := w.Hour()*3600 + w.Minute()*60
	return fmt.Sprintf("M%d.%d.%d/%s", int(w.Month()), week, int(w.Weekday()), hm(secs))
}

// hm formats seconds as [-]h[:mm] (POSIX offsets are west-positive, callers pass the sign).
func hm(secs int) string {
	sign := ""
	if secs < 0 {
		sign, secs = "-", -secs
	}
	h, m := secs/3600, secs%3600/60
	if m == 0 {
		return fmt.Sprintf("%s%d", sign, h)
	}
	return fmt.Sprintf("%s%d:%02d", sign, h, m)
}

func abbr(name string) string {
	alpha := len(name) >= 3
	for _, c := range name {
		if !(c >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z') {
			alpha = false
		}
	}
	if alpha {
		return name
	}
	return "<" + name + ">"
}
