package main

import (
	"strings"
	"testing"
)

const fakeKey = "0123456789abcdef0123456789abcdef"

func TestNormKey(t *testing.T) {
	if k, ok := normKey("  " + strings.ToUpper(fakeKey) + " "); !ok || k != fakeKey {
		t.Fatalf("got %q %v", k, ok)
	}
	if k, ok := normKey("off"); !ok || k != "" {
		t.Fatal("off must clear the key")
	}
	if _, ok := normKey("abc"); ok {
		t.Fatal("short key accepted")
	}
	if _, ok := normKey(fakeKey + "&x=1"); ok {
		t.Fatal("junk accepted")
	}
}

func TestParseLatLon(t *testing.T) {
	cases := map[string]string{"50.4501, 30.5234": "50.45,30.52", "-33.87,151.2": "-33.87,151.20", "0,0": "0.00,0.00"}
	for in, want := range cases {
		if got, ok := parseLatLon(in); !ok || got != want {
			t.Fatalf("%q -> %q %v, want %q", in, got, ok, want)
		}
	}
	for _, bad := range []string{"Kyiv", "95,30", "50,190", "50.4"} {
		if _, ok := parseLatLon(bad); ok {
			t.Fatalf("%q accepted", bad)
		}
	}
}

var regions = []string{"Київська область", "Львівська область", "м. Київ", "Харківська область", "Херсонська область"}

func TestMatchRegion(t *testing.T) {
	for in, want := range map[string]string{"м. Київ": "м. Київ", "kyiv": "м. Київ", "Київ": "м. Київ",
		"львів": "Львівська область", "київська": "Київська область"} {
		if got, err := matchRegion(in, regions); err != nil || got != want {
			t.Fatalf("%q -> %q %v, want %q", in, got, err, want)
		}
	}
	if _, err := matchRegion("х", regions); err == nil || !strings.Contains(err.Error(), "several") {
		t.Fatalf("ambiguous: %v", err)
	}
	if _, err := matchRegion("Atlantis", regions); err == nil {
		t.Fatal("unknown accepted")
	}
}

func TestZoneHelpers(t *testing.T) {
	if !inUkraine("Europe/Kyiv") || !inUkraine("Europe/Kiev") || inUkraine("Europe/Helsinki") {
		t.Fatal("inUkraine")
	}
	if cityFromZone("America/New_York") != "New York" || cityFromZone("Etc/UTC") != "" || cityFromZone("") != "" {
		t.Fatal("cityFromZone")
	}
}

func TestGeocode(t *testing.T) {
	old := getURL
	defer func() { getURL = old }()
	getURL = func(u string) ([]byte, int, error) {
		if !strings.Contains(u, "q=Kyiv") {
			t.Fatalf("url %s", u)
		}
		return []byte(`[{"name":"Kyiv","country":"UA","lat":50.4500336,"lon":30.5241361}]`), 200, nil
	}
	ll, name, err := geocode("Kyiv", fakeKey)
	if err != nil || ll != "50.45,30.52" || name != "Kyiv, UA" {
		t.Fatalf("%q %q %v", ll, name, err)
	}
	getURL = func(string) ([]byte, int, error) { return []byte(`{"cod":401}`), 401, nil }
	if _, _, err := geocode("Kyiv", fakeKey); err == nil || !strings.Contains(err.Error(), "2 hours") {
		t.Fatalf("401: %v", err)
	}
	getURL = func(string) ([]byte, int, error) { return []byte(`[]`), 200, nil }
	if _, _, err := geocode("Nowhere", fakeKey); err == nil {
		t.Fatal("empty answer accepted")
	}
}
