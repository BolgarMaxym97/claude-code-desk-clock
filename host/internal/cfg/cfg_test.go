package cfg

import (
	"os"
	"path/filepath"
	"testing"
)

func TestSaveLoadAndPrivateMode(t *testing.T) {
	t.Setenv("CLOCK_CONFIG_DIR", t.TempDir())
	if err := Save(Config{URL: "http://10.0.0.5", Host: "claude-clock.local", Token: "tok"}); err != nil {
		t.Fatal(err)
	}
	c, err := Load()
	if err != nil || c.URL != "http://10.0.0.5" || c.Token != "tok" {
		t.Fatal(c, err)
	}
	st, _ := os.Stat(filepath.Join(Dir(), "config.json"))
	if st.Mode().Perm()&0o077 != 0 && os.PathSeparator == '/' {
		t.Fatalf("config readable by others: %v", st.Mode())
	}
}

func TestOldKeyValueConfigIsParsedNotExecuted(t *testing.T) {
	d := t.TempDir()
	t.Setenv("CLOCK_CONFIG_DIR", d)
	marker := filepath.Join(d, "pwned")
	os.WriteFile(filepath.Join(d, "config"), []byte("CLOCK_URL=http://1.2.3.4\nCLOCK_TOKEN=abc\n$(touch "+marker+")\n"), 0o600)
	c, err := Load()
	if err != nil || c.URL != "http://1.2.3.4" || c.Token != "abc" {
		t.Fatal(c, err)
	}
	if _, err := os.Stat(marker); err == nil {
		t.Fatal("config executed")
	}
}
