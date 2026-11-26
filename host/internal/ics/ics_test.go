package ics

import (
	"strings"
	"testing"
	"time"
)

// A Google-style export: folded lines, TZID with IANA names, a weekly series with an exception, a moved instance,
// a cancelled instance, an all-day event, a declined meeting and a cancelled event.
const sample = "BEGIN:VCALENDAR\r\n" +
	"VERSION:2.0\r\n" +
	"X-WR-TIMEZONE:Europe/Kiev\r\n" +
	"BEGIN:VEVENT\r\n" +
	"DTSTART;TZID=Europe/Kiev:20261102T100000\r\n" +
	"DTEND;TZID=Europe/Kiev:20261102T101500\r\n" +
	"RRULE:FREQ=WEEKLY;BYDAY=MO,WE,FR\r\n" +
	"EXDATE;TZID=Europe/Kiev:20261127T100000\r\n" +
	"UID:standup@google.com\r\n" +
	"SUMMARY:Daily standup\\, team\r\n" +
	"END:VEVENT\r\n" +
	"BEGIN:VEVENT\r\n" +
	"DTSTART;TZID=Europe/Kiev:20261130T113000\r\n" +
	"DTEND;TZID=Europe/Kiev:20261130T114500\r\n" +
	"RECURRENCE-ID;TZID=Europe/Kiev:20261130T100000\r\n" +
	"UID:standup@google.com\r\n" +
	"SUMMARY:Daily standup (moved)\r\n" +
	"END:VEVENT\r\n" +
	"BEGIN:VEVENT\r\n" +
	"DTSTART;TZID=Europe/Kiev:20261202T100000\r\n" +
	"DTEND;TZID=Europe/Kiev:20261202T101500\r\n" +
	"RECURRENCE-ID;TZID=Europe/Kiev:20261202T100000\r\n" +
	"STATUS:CANCELLED\r\n" +
	"UID:standup@google.com\r\n" +
	"END:VEVENT\r\n" +
	"BEGIN:VEVENT\r\n" +
	"DTSTART:20261126T130000Z\r\n" +
	"DTEND:20261126T140000Z\r\n" +
	"UID:review@google.com\r\n" +
	"SUMMARY:Design review with a very long title that goes on and o\r\n" +
	" n and on\r\n" +
	"ATTENDEE;CN=Max;PARTSTAT=ACCEPTED:mailto:me@example.com\r\n" +
	"END:VEVENT\r\n" +
	"BEGIN:VEVENT\r\n" +
	"DTSTART:20261126T150000Z\r\n" +
	"DTEND:20261126T153000Z\r\n" +
	"UID:declined@google.com\r\n" +
	"SUMMARY:Declined one\r\n" +
	"ATTENDEE;CUTYPE=INDIVIDUAL;ROLE=REQ-PARTICIPANT;PARTSTAT=DECLINED;CN=me@exa\r\n" +
	" mple.com;X-NUM-GUESTS=0:mailto:ME@example.com\r\n" +
	"END:VEVENT\r\n" +
	"BEGIN:VEVENT\r\n" +
	"DTSTART:20261126T160000Z\r\n" +
	"DTEND:20261126T163000Z\r\n" +
	"UID:cancelled@google.com\r\n" +
	"STATUS:CANCELLED\r\n" +
	"SUMMARY:Cancelled\r\n" +
	"END:VEVENT\r\n" +
	"BEGIN:VEVENT\r\n" +
	"DTSTART;VALUE=DATE:20261126\r\n" +
	"DTEND;VALUE=DATE:20261127\r\n" +
	"UID:allday@google.com\r\n" +
	"SUMMARY:Holiday\r\n" +
	"END:VEVENT\r\n" +
	"END:VCALENDAR\r\n"

var kyiv, _ = time.LoadLocation("Europe/Kyiv")

func at(s string) time.Time {
	t, err := time.ParseInLocation("2006-01-02 15:04", s, kyiv)
	if err != nil {
		panic(err)
	}
	return t
}

func titles(occ []Occurrence) []string {
	var out []string
	for _, o := range occ {
		out = append(out, o.Start.In(kyiv).Format("01-02 15:04")+" "+o.Title)
	}
	return out
}

func TestExpandWindow(t *testing.T) {
	cal, err := Parse(strings.NewReader(sample))
	if err != nil {
		t.Fatal(err)
	}
	got := titles(Expand(cal, at("2026-11-26 00:00"), at("2026-12-03 00:00"), "me@example.com"))
	want := []string{
		"11-26 15:00 Design review with a very long title that goes on and on and on", // 13:00Z = 15:00 Kyiv
		// 11-27 (Fri) is an EXDATE; declined, cancelled and all-day events are skipped
		"11-30 11:30 Daily standup (moved)",
		// 12-02 (Wed) instance cancelled
	}
	if strings.Join(got, "\n") != strings.Join(want, "\n") {
		t.Fatalf("got\n%s\nwant\n%s", strings.Join(got, "\n"), strings.Join(want, "\n"))
	}
	// without the email the declined meeting shows
	if n := len(Expand(cal, at("2026-11-26 00:00"), at("2026-11-27 00:00"), "")); n != 2 {
		t.Fatalf("without email: %d", n)
	}
	// text escapes are undone
	got = titles(Expand(cal, at("2026-11-23 00:00"), at("2026-11-24 00:00"), ""))
	if len(got) != 1 || got[0] != "11-23 10:00 Daily standup, team" {
		t.Fatalf("monday: %v", got)
	}
}

func TestRecurrenceRules(t *testing.T) {
	mk := func(rule, start string) Calendar {
		return Calendar{Events: []Event{{UID: "x", Summary: "x", Start: at(start), End: at(start).Add(30 * time.Minute), RRule: rule}}}
	}
	count := func(c Calendar, from, to string) []string {
		var out []string
		for _, o := range Expand(c, at(from), at(to), "") {
			out = append(out, o.Start.In(kyiv).Format("2006-01-02 15:04"))
		}
		return out
	}
	cases := []struct {
		rule, start, from, to string
		want                  []string
	}{
		{"FREQ=DAILY;COUNT=3", "2026-11-25 09:00", "2026-11-20 00:00", "2026-12-31 00:00",
			[]string{"2026-11-25 09:00", "2026-11-26 09:00", "2026-11-27 09:00"}},
		{"FREQ=DAILY;INTERVAL=2;UNTIL=20261130T235959Z", "2026-11-25 09:00", "2026-11-20 00:00", "2026-12-31 00:00",
			[]string{"2026-11-25 09:00", "2026-11-27 09:00", "2026-11-29 09:00"}},
		{"FREQ=WEEKLY;INTERVAL=2;BYDAY=TU,TH", "2026-11-03 14:00", "2026-11-16 00:00", "2026-12-01 00:00",
			[]string{"2026-11-17 14:00", "2026-11-19 14:00"}},
		{"FREQ=MONTHLY;BYDAY=-1FR", "2026-10-30 17:00", "2026-11-01 00:00", "2027-01-01 00:00",
			[]string{"2026-11-27 17:00", "2026-12-25 17:00"}},
		{"FREQ=MONTHLY;BYDAY=2TU", "2026-10-13 10:00", "2026-11-01 00:00", "2026-12-01 00:00",
			[]string{"2026-11-10 10:00"}},
		{"FREQ=MONTHLY;BYMONTHDAY=31", "2026-10-31 10:00", "2026-11-01 00:00", "2027-01-01 00:00",
			[]string{"2026-12-31 10:00"}}, // November has no 31st
		{"FREQ=YEARLY", "2025-11-26 12:00", "2026-11-01 00:00", "2026-12-01 00:00",
			[]string{"2026-11-26 12:00"}},
		// a weekly series started before a DST change keeps its local time
		{"FREQ=WEEKLY", "2026-10-19 10:00", "2026-10-26 00:00", "2026-11-03 00:00",
			[]string{"2026-10-26 10:00", "2026-11-02 10:00"}},
	}
	for _, c := range cases {
		got := count(mk(c.rule, c.start), c.from, c.to)
		if strings.Join(got, ",") != strings.Join(c.want, ",") {
			t.Errorf("%s: got %v want %v", c.rule, got, c.want)
		}
	}
}

func TestOngoingMeetingIsIncludedAndWindowsTimezoneNames(t *testing.T) {
	src := "BEGIN:VCALENDAR\r\nBEGIN:VEVENT\r\nDTSTART;TZID=FLE Standard Time:20261126T140000\r\n" +
		"DTEND;TZID=FLE Standard Time:20261126T150000\r\nUID:o\r\nSUMMARY:Outlook one\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n"
	cal, err := Parse(strings.NewReader(src))
	if err != nil {
		t.Fatal(err)
	}
	got := titles(Expand(cal, at("2026-11-26 14:30"), at("2026-11-26 20:00"), ""))
	if len(got) != 1 || got[0] != "11-26 14:00 Outlook one" { // still running at 14:30
		t.Fatalf("%v", got)
	}
}

func TestGarbageIsAnError(t *testing.T) {
	if _, err := Parse(strings.NewReader("<html>Sign in</html>")); err == nil {
		t.Fatal("html accepted as a calendar")
	}
}
