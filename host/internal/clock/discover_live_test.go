//go:build live

package clock

import (
	"testing"
	"time"
)

// go test -tags live ./internal/clock -run Live -v   (needs a clock on the network)
func TestLiveDiscover(t *testing.T) {
	t0 := time.Now()
	t.Log(Discover(1200*time.Millisecond), time.Since(t0))
}
