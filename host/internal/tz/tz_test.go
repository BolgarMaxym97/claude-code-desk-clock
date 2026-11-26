package tz

import (
	"testing"
	"time"
)

func TestPosixFromIANA(t *testing.T) {
	cases := map[string]string{
		"Europe/Kyiv":       "EET-2EEST,M3.5.0/3,M10.5.0/4",
		"Europe/London":     "GMT0BST,M3.5.0/1,M10.5.0/2",
		"America/New_York":  "EST5EDT,M3.2.0/2,M11.1.0/2",
		"Australia/Sydney":  "AEST-10AEDT,M10.1.0/2,M4.1.0/3",
		"Asia/Kolkata":      "IST-5:30",
		"UTC":               "UTC0",
		"America/Sao_Paulo": "<-03>3",
		"Asia/Tokyo":        "JST-9",
		"Pacific/Chatham":   "<+1245>-12:45<+1345>,M9.5.0/2:45,M4.1.0/3:45",
	}
	for name, want := range cases {
		loc, err := time.LoadLocation(name)
		if err != nil {
			t.Fatal(err)
		}
		got := Posix(loc, 2026)
		if got != want {
			t.Errorf("%s: got %q want %q", name, got, want)
		}
		if !Valid(got) {
			t.Errorf("%s: %q not accepted by the clock", name, got)
		}
	}
}

func TestValid(t *testing.T) {
	if Valid("") || Valid("EET-2 EEST") || Valid("A;B") {
		t.Fatal("bad tz accepted")
	}
}
