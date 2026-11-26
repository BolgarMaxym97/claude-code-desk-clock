package main

import (
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"os"
	"strings"
	"time"

	"github.com/BolgarMaxym97/claude-code-desk-clock/host/internal/clock"
)

type release struct {
	TagName string `json:"tag_name"`
	Assets  []struct {
		Name string `json:"name"`
		URL  string `json:"browser_download_url"`
	} `json:"assets"`
}

const maxImage = 1044464 // OTA slot of the 4M2M layout (firmware must fit below 0x200000 with room to copy)

// update installs firmware over the network: the latest GitHub release (sha256-checked) or a local file.
func update(args []string) error {
	c, err := loadPaired()
	if err != nil {
		return err
	}
	st, err := clock.GetStatus(c.URL, 3*time.Second)
	if err != nil {
		return fmt.Errorf("clock not reachable at %s: %v", c.URL, err)
	}
	var image []byte
	var ver string
	if f, ok := flag(args, "--file"); ok {
		image, err = os.ReadFile(f)
		ver = f
	} else {
		image, ver, err = latestImage(st.FW, has(args, "--force"))
	}
	if err != nil || image == nil {
		return err
	}
	if len(image) < 100000 || len(image) > maxImage || image[0] != 0xE9 {
		return errors.New("this file does not look like clock firmware (ESP8266 image)")
	}
	if !yes(fmt.Sprintf("Install %s on the clock (now %s)? The screen goes blank for ~30 s.", ver, st.FW), has(args, "--yes")) {
		return errors.New("aborted")
	}
	fmt.Println("Uploading…")
	if err := clock.Upload(c.URL, c.Token, image, 120*time.Second); err != nil {
		return err
	}
	fmt.Print("Clock is restarting")
	for i := 0; i < 45; i++ {
		time.Sleep(2 * time.Second)
		fmt.Print(".")
		if s, err := clock.GetStatus(c.URL, 2*time.Second); err == nil && i > 2 {
			fmt.Printf("\nDone: firmware %s\n", s.FW)
			return nil
		}
	}
	return errors.New("\nthe clock did not come back yet; check its screen (Wi-Fi setup?) and run `claude-clock status`")
}

func latestImage(current string, force bool) ([]byte, string, error) {
	var r release
	if err := getJSON("https://api.github.com/repos/"+githubRepo+"/releases/latest", &r); err != nil {
		return nil, "", fmt.Errorf("cannot read the latest release: %v", err)
	}
	ver := strings.TrimPrefix(r.TagName, "v")
	if ver == current && !force {
		fmt.Println("The clock already has the latest firmware", ver)
		return nil, "", nil
	}
	var binURL, sumURL string
	for _, a := range r.Assets {
		switch {
		case strings.HasPrefix(a.Name, "SDP_") && strings.HasSuffix(a.Name, ".bin"):
			binURL = a.URL
		case strings.HasPrefix(a.Name, "SDP_") && strings.HasSuffix(a.Name, ".bin.sha256"):
			sumURL = a.URL
		}
	}
	if binURL == "" || sumURL == "" {
		return nil, "", errors.New("release has no firmware file")
	}
	image, err := get(binURL, 4<<20)
	if err != nil {
		return nil, "", err
	}
	sum, err := get(sumURL, 1024)
	if err != nil {
		return nil, "", err
	}
	h := sha256.Sum256(image)
	if f := strings.Fields(string(sum)); len(f) == 0 || !strings.EqualFold(f[0], hex.EncodeToString(h[:])) {
		return nil, "", errors.New("downloaded firmware is damaged (sha256 mismatch); try again")
	}
	return image, ver, nil
}

func get(url string, limit int64) ([]byte, error) {
	resp, err := (&http.Client{Timeout: 60 * time.Second}).Get(url)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if resp.StatusCode != 200 {
		return nil, fmt.Errorf("%s: HTTP %d", url, resp.StatusCode)
	}
	return io.ReadAll(io.LimitReader(resp.Body, limit))
}

func getJSON(url string, v any) error {
	b, err := get(url, 1<<20)
	if err != nil {
		return err
	}
	return json.Unmarshal(b, v)
}
