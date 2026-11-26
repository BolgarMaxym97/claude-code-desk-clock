package clock

import (
	"encoding/json"
	"io"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"
)

func fake(t *testing.T) *httptest.Server {
	return httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		switch r.URL.Path {
		case "/status":
			w.Write([]byte(`{"fw":"1.0.0","ip":"10.0.0.9","screen":"Clock"}`))
		case "/api/pair/start":
			w.Write([]byte(`{"ok":true}`))
		case "/api/pair":
			var b struct{ Code string }
			json.NewDecoder(r.Body).Decode(&b)
			if b.Code != "K7Q2MX" {
				w.WriteHeader(403)
				return
			}
			w.Write([]byte(`{"token":"secret-token-123456"}`))
		case "/api/event":
			if r.Header.Get("X-Clock-Token") != "secret-token-123456" {
				w.WriteHeader(401)
				return
			}
			w.Write([]byte(`{"ok":true}`))
		case "/update":
			u, p, _ := r.BasicAuth()
			f, _, err := r.FormFile("firmware")
			if u != "clock" || p != "secret-token-123456" || err != nil {
				w.WriteHeader(401)
				return
			}
			b, _ := io.ReadAll(f)
			if string(b) != "IMAGE" {
				w.WriteHeader(400)
				return
			}
			w.Write([]byte("Update Success! Rebooting..."))
		}
	}))
}

func TestPairStatusPostUpload(t *testing.T) {
	s := fake(t)
	defer s.Close()
	st, err := GetStatus(s.URL, time.Second)
	if err != nil || st.FW != "1.0.0" {
		t.Fatal(st, err)
	}
	if err := PairStart(s.URL); err != nil {
		t.Fatal(err)
	}
	if _, err := Pair(s.URL, "AAAAAA"); err == nil || !strings.Contains(err.Error(), "wrong code") {
		t.Fatal(err)
	}
	tok, err := Pair(s.URL, " k7q2mx ")
	if err != nil || tok != "secret-token-123456" {
		t.Fatal(tok, err)
	}
	if err := Post(s.URL, "bad", "/api/event", []byte("{}"), time.Second); err == nil {
		t.Fatal("bad token accepted")
	}
	if err := Post(s.URL, tok, "/api/event", []byte("{}"), time.Second); err != nil {
		t.Fatal(err)
	}
	if err := Upload(s.URL, tok, []byte("IMAGE"), 5*time.Second); err != nil {
		t.Fatal(err)
	}
}

func TestNotAClock(t *testing.T) {
	s := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) { w.Write([]byte(`<html>router</html>`)) }))
	defer s.Close()
	if _, err := GetStatus(s.URL, time.Second); err == nil {
		t.Fatal("router taken for a clock")
	}
}
