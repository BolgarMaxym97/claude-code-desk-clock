package main

import (
	"io"
	"strings"
	"testing"
	"time"

	"github.com/BolgarMaxym97/claude-code-desk-clock/host/internal/cfg"
)

func TestCutTitle(t *testing.T) {
	if got := cutTitle("  Daily   standup ", 40); got != "Daily standup" {
		t.Fatalf("%q", got)
	}
	got := cutTitle("Щотижнева синхронізація команди розробки", 20)
	if !strings.HasSuffix(got, "…") || len(got) > 20+len("…") || !strings.HasPrefix(got, "Щотижнева") {
		t.Fatalf("%q", got)
	}
}

func TestUpcomingFromFeed(t *testing.T) {
	old := fetchCalendar
	defer func() { fetchCalendar = old }()
	now := time.Now().UTC().Truncate(time.Minute)
	ts := func(d time.Duration) string { return now.Add(d).Format("20060102T150405Z") }
	feed := "BEGIN:VCALENDAR\r\n" +
		"BEGIN:VEVENT\r\nUID:a\r\nDTSTART:" + ts(-10*time.Minute) + "\r\nDTEND:" + ts(20*time.Minute) + "\r\nSUMMARY:Ongoing\r\nEND:VEVENT\r\n" +
		"BEGIN:VEVENT\r\nUID:b\r\nDTSTART:" + ts(time.Hour) + "\r\nDTEND:" + ts(2*time.Hour) + "\r\nSUMMARY:Declined\r\n" +
		"ATTENDEE;PARTSTAT=DECLINED:mailto:me@work.com\r\nEND:VEVENT\r\n" +
		"BEGIN:VEVENT\r\nUID:c\r\nDTSTART:" + ts(3*time.Hour) + "\r\nDTEND:" + ts(4*time.Hour) + "\r\nSUMMARY:Planning\r\nEND:VEVENT\r\n" +
		"BEGIN:VEVENT\r\nUID:d\r\nDTSTART:" + ts(13*time.Hour) + "\r\nDTEND:" + ts(14*time.Hour) + "\r\nSUMMARY:Tomorrow\r\nEND:VEVENT\r\n" +
		"END:VCALENDAR\r\n"
	fetchCalendar = func(string) (io.ReadCloser, error) { return io.NopCloser(strings.NewReader(feed)), nil }
	ms, err := upcoming(cfg.Config{Calendar: "https://x/basic.ics", CalEmail: "ME@work.com"}, now)
	if err != nil {
		t.Fatal(err)
	}
	if len(ms) != 2 || ms[0].T != "Ongoing" || ms[1].T != "Planning" {
		t.Fatalf("%+v", ms)
	}
	if ms[1].S != now.Add(3*time.Hour).Unix() || ms[1].HM != now.Add(3*time.Hour).Local().Format("15:04") {
		t.Fatalf("times %+v", ms[1])
	}
	ms, _ = upcoming(cfg.Config{Calendar: "https://x/basic.ics", CalNoTitles: true}, now)
	if len(ms) != 3 || ms[0].T != "" {
		t.Fatalf("titles off / no email: %+v", ms)
	}
	fetchCalendar = func(string) (io.ReadCloser, error) { return io.NopCloser(strings.NewReader("<html>")), nil }
	if _, err := upcoming(cfg.Config{Calendar: "https://x"}, now); err == nil {
		t.Fatal("html accepted")
	}
}

func TestCalendarURL(t *testing.T) {
	if !validCalendarURL("https://calendar.google.com/calendar/ical/x/private-y/basic.ics") {
		t.Fatal("google url")
	}
	for _, bad := range []string{"http://x/basic.ics", "file:///etc/passwd", "https://x y"} {
		if validCalendarURL(bad) {
			t.Fatalf("%q accepted", bad)
		}
	}
}

func TestAgoText(t *testing.T) {
	for d, want := range map[time.Duration]string{30 * time.Second: "just now", 60 * time.Second: "1 min ago",
		59 * time.Minute: "59 min ago", 125 * time.Minute: "2 h 5 min ago"} {
		if got := agoText(d); got != want {
			t.Errorf("%v: %q, want %q", d, got, want)
		}
	}
}
