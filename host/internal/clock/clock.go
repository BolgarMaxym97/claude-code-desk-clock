// Package clock talks to the clock's HTTP API on the local network.
package clock

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"mime/multipart"
	"net"
	"net/http"
	"strings"
	"sync"
	"time"
)

// MDNSName is the name the clock announces on the local network.
const MDNSName = "claude-clock.local"

// Status is the part of GET /status this tool uses.
type Status struct {
	FW       string `json:"fw"`
	IP       string `json:"ip"`
	Screen   string `json:"screen"`
	Time     string `json:"time"`
	Sessions int    `json:"sessions"`
	TZ       string `json:"tz"`
	Heap     int    `json:"heap"`
	Uptime   int    `json:"uptime"`
	SSID     string `json:"ssid"`
	Weather  string `json:"weather"`
	Alerts   string `json:"alerts"`
	Alert    string `json:"alert"`
}

// Post sends JSON with the token; any non-2xx is an error.
func Post(url, token, path string, body []byte, timeout time.Duration) error {
	req, err := http.NewRequest("POST", strings.TrimRight(url, "/")+path, bytes.NewReader(body))
	if err != nil {
		return err
	}
	req.Header.Set("Content-Type", "application/json")
	if token != "" {
		req.Header.Set("X-Clock-Token", token)
	}
	resp, err := (&http.Client{Timeout: timeout}).Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	io.Copy(io.Discard, io.LimitReader(resp.Body, 4096))
	if resp.StatusCode/100 != 2 {
		return &HTTPError{resp.StatusCode}
	}
	return nil
}

// HTTPError is a non-2xx answer from the clock.
type HTTPError struct{ Code int }

func (e *HTTPError) Error() string { return fmt.Sprintf("clock answered HTTP %d", e.Code) }

// GetStatus reads /status (no token needed); fails if the answer is not from a Claude Clock.
func GetStatus(url string, timeout time.Duration) (Status, error) {
	var s Status
	resp, err := (&http.Client{Timeout: timeout}).Get(strings.TrimRight(url, "/") + "/status")
	if err != nil {
		return s, err
	}
	defer resp.Body.Close()
	if resp.StatusCode != 200 {
		return s, &HTTPError{resp.StatusCode}
	}
	if err := json.NewDecoder(io.LimitReader(resp.Body, 16384)).Decode(&s); err != nil {
		var ne net.Error
		if errors.As(err, &ne) && ne.Timeout() {
			return s, errors.New("the clock answered too slowly (weak Wi-Fi?)")
		}
		return s, errors.New("not a Claude Clock")
	}
	if s.FW == "" {
		return s, errors.New("not a Claude Clock")
	}
	return s, nil
}

// StatusRetry asks a few times: an ESP8266 on weak Wi-Fi sometimes misses one request.
func StatusRetry(url string, timeout time.Duration, tries int) (Status, error) {
	var st Status
	var err error
	for i := 0; i < tries; i++ {
		if st, err = GetStatus(url, timeout); err == nil {
			return st, nil
		}
	}
	return st, err
}

// PairStart asks the clock to show a pairing code on its screen.
func PairStart(url string) error {
	return Post(url, "", "/api/pair/start", []byte("{}"), 5*time.Second)
}

// Pair sends the code from the screen and returns the clock's token.
func Pair(url, code string) (string, error) {
	body, _ := json.Marshal(map[string]string{"code": strings.ToUpper(strings.TrimSpace(code))})
	resp, err := (&http.Client{Timeout: 5 * time.Second}).Post(strings.TrimRight(url, "/")+"/api/pair", "application/json", bytes.NewReader(body))
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()
	switch resp.StatusCode {
	case 200:
	case 403:
		return "", errors.New("wrong code")
	case 410:
		return "", errors.New("the code expired: run setup again")
	case 423:
		return "", errors.New("too many wrong codes: unplug the clock, plug it back in and run setup again")
	default:
		return "", &HTTPError{resp.StatusCode}
	}
	var out struct {
		Token string `json:"token"`
	}
	if err := json.NewDecoder(io.LimitReader(resp.Body, 4096)).Decode(&out); err != nil || out.Token == "" {
		return "", errors.New("clock sent no token")
	}
	return out.Token, nil
}

// Upload sends a firmware image to /update (HTTP basic auth clock:<token>), as the clock's own update page does.
func Upload(url, token string, image []byte, timeout time.Duration) error {
	var b bytes.Buffer
	w := multipart.NewWriter(&b)
	fw, _ := w.CreateFormFile("firmware", "firmware.bin")
	fw.Write(image)
	w.Close()
	req, err := http.NewRequest("POST", strings.TrimRight(url, "/")+"/update", &b)
	if err != nil {
		return err
	}
	req.Header.Set("Content-Type", w.FormDataContentType())
	req.SetBasicAuth("clock", token)
	resp, err := (&http.Client{Timeout: timeout}).Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	msg, _ := io.ReadAll(io.LimitReader(resp.Body, 4096))
	if resp.StatusCode != 200 || !strings.Contains(string(msg), "Update Success") {
		return fmt.Errorf("update failed (HTTP %d): %s", resp.StatusCode, strings.TrimSpace(string(msg)))
	}
	return nil
}

// Discover finds clocks: first by mDNS name, then by asking every address of the local /24 networks.
func Discover(timeout time.Duration) []string {
	ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
	defer cancel()
	if addrs, err := net.DefaultResolver.LookupIPAddr(ctx, MDNSName); err == nil {
		for _, a := range addrs {
			if ip4 := a.IP.To4(); ip4 != nil {
				u := "http://" + ip4.String()
				if _, err := GetStatus(u, timeout); err == nil {
					return []string{u}
				}
			}
		}
	}
	return scan(LocalNets(), timeout)
}

// LocalNets lists the private IPv4 /24 prefixes of this computer's interfaces ("192.168.1.").
func LocalNets() []string {
	var out []string
	seen := map[string]bool{}
	ifs, _ := net.Interfaces()
	for _, i := range ifs {
		if i.Flags&net.FlagUp == 0 || i.Flags&net.FlagLoopback != 0 {
			continue
		}
		addrs, _ := i.Addrs()
		for _, a := range addrs {
			n, ok := a.(*net.IPNet)
			if !ok {
				continue
			}
			ip := n.IP.To4()
			if ip == nil || !ip.IsPrivate() {
				continue
			}
			p := fmt.Sprintf("%d.%d.%d.", ip[0], ip[1], ip[2])
			if !seen[p] {
				seen[p] = true
				out = append(out, p)
			}
		}
	}
	return out
}

func scan(prefixes []string, timeout time.Duration) []string {
	var mu sync.Mutex
	var found []string
	sem := make(chan struct{}, 64)
	var wg sync.WaitGroup
	for _, p := range prefixes {
		for h := 1; h < 255; h++ {
			u := fmt.Sprintf("http://%s%d", p, h)
			wg.Add(1)
			sem <- struct{}{}
			go func() {
				defer wg.Done()
				defer func() { <-sem }()
				if _, err := GetStatus(u, timeout); err == nil {
					mu.Lock()
					found = append(found, u)
					mu.Unlock()
				}
			}()
		}
	}
	wg.Wait()
	return found
}
