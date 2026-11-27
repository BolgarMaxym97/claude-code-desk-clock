package main

import "testing"

func TestAlertLevel(t *testing.T) {
	cases := []struct{ name, body, region, want string }{
		{"none", `{"alerts":[]}`, "м. Київ", "none"},
		{"other oblast", `{"alerts":[{"location_title":"Львівська область","location_type":"oblast","location_oblast":"Львівська область","alert_type":"air_raid"}]}`, "м. Київ", "none"},
		{"kyiv city, drones", `{"alerts":[{"location_title":"м. Київ","location_type":"city","location_oblast":"м. Київ","alert_type":"air_raid","alert_level":"red","threats":[{"threat_type":"drones"}]}]}`, "м. Київ", "drone"},
		{"oblast, missiles win", `{"alerts":[{"location_title":"Київська область","location_type":"oblast","location_oblast":"Київська область","alert_type":"air_raid","threats":[{"threat_type":"drones"},{"threat_type":"ballistic_missiles"}]}]}`, "Київська область", "missile"},
		{"oblast, no threats", `{"alerts":[{"location_title":"Одеська область","location_type":"oblast","location_oblast":"Одеська область","alert_type":"air_raid"}]}`, "Одеська область", "alert"},
		{"one raion only", `{"alerts":[{"location_title":"Бучанський район","location_type":"raion","location_oblast":"Київська область","alert_type":"air_raid","alert_level":"red"}]}`, "Київська область", "partial"},
		{"yellow only", `{"alerts":[{"location_title":"Київська область","location_type":"oblast","location_oblast":"Київська область","alert_type":"air_raid","alert_level":"yellow"}]}`, "Київська область", "threat"},
		{"artillery is not an air raid", `{"alerts":[{"location_title":"Херсонська область","location_type":"oblast","location_oblast":"Херсонська область","alert_type":"artillery_shelling"}]}`, "Херсонська область", "none"},
		{"sevastopol names differ", `{"alerts":[{"location_title":"м. Севастополь","location_type":"city","location_oblast":"м. Севастополь","alert_type":"air_raid"}]}`, "Севастополь", "alert"},
	}
	for _, c := range cases {
		got, err := alertLevel([]byte(c.body), c.region)
		if err != nil || got != c.want {
			t.Errorf("%s: got %q, %v; want %q", c.name, got, err, c.want)
		}
	}
	if _, err := alertLevel([]byte("<html>"), "м. Київ"); err == nil {
		t.Error("broken answer accepted")
	}
}

func TestRefusedAnswersBackOff(t *testing.T) {
	for code, want := range map[string]bool{"HTTP 401": true, "HTTP 429": true, "HTTP 500": false} {
		if got := refused(errorString(code)); got != want {
			t.Errorf("%s: %v", code, got)
		}
	}
}

type errorString string

func (e errorString) Error() string { return string(e) }
