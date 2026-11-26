package main

import "testing"

func TestLangAnswer(t *testing.T) {
	for in, want := range map[string]string{"": "en", "e": "en", "EN": "en", "English": "en",
		"u": "uk", "uk": "uk", "ua": "uk", "укр": "uk", "Українська": "uk"} {
		got, ok := langAnswer(in)
		if !ok || got != want {
			t.Errorf("%q -> %q %v, want %q", in, got, ok, want)
		}
	}
	if _, ok := langAnswer("de"); ok {
		t.Error("unknown language accepted")
	}
}
