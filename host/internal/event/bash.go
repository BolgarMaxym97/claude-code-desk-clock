package event

import (
	"path/filepath"
	"regexp"
	"strings"
)

var segSplit = regexp.MustCompile(`&&|\|\||[;|\n]`)

// BashKinds lists what a finished shell command did, in this order: tests, commit, push, install, docker.
// It reads the whole command (the clock only ever receives these words, never the command itself).
func BashKinds(cmd string) []string {
	found := map[string]bool{}
	for _, seg := range segSplit.Split(cmd, -1) {
		f := strings.Fields(seg)
		i := 0
		for i < len(f) && (strings.Contains(f[i], "=") && !strings.HasPrefix(f[i], "-") || f[i] == "sudo" || f[i] == "time") {
			i++ // VAR=1 cmd, sudo cmd
		}
		if i >= len(f) {
			continue
		}
		prog := filepath.Base(f[i])
		args := f[i+1:]
		switch {
		case prog == "git":
			if sub := gitSub(args); sub == "commit" || sub == "push" {
				found[sub] = true
			}
		case prog == "docker" || prog == "docker-compose":
			found["docker"] = true
		case isPM(prog) && hasAny(args, "install", "i", "add", "require"):
			found["install"] = true
		}
		if prog != "echo" && prog != "printf" && isTests(prog, args) {
			found["tests"] = true
		}
	}
	var out []string
	for _, k := range []string{"tests", "commit", "push", "install", "docker"} {
		if found[k] {
			out = append(out, k)
		}
	}
	return out
}

func gitSub(args []string) string {
	for j := 0; j < len(args); j++ {
		a := args[j]
		if a == "-C" || a == "-c" || a == "--git-dir" || a == "--work-tree" {
			j++
			continue
		}
		if strings.HasPrefix(a, "-") {
			continue
		}
		return a
	}
	return ""
}

func isPM(p string) bool {
	switch p {
	case "npm", "pip", "pip3", "brew", "yarn", "pnpm", "bun", "composer", "apt", "apt-get", "cargo", "uv", "poetry", "gem":
		return true
	}
	return false
}

func hasAny(args []string, words ...string) bool {
	for _, a := range args {
		for _, w := range words {
			if a == w {
				return true
			}
		}
	}
	return false
}

var testWords = map[string]bool{"test": true, "tests": true, "pytest": true, "phpunit": true, "pest": true, "jest": true,
	"vitest": true, "unittest": true}

func isTests(prog string, args []string) bool {
	for _, t := range append([]string{prog}, args...) {
		b := filepath.Base(strings.Trim(t, `"'`))
		if testWords[b] || strings.HasPrefix(b, "test.") || strings.HasPrefix(b, "test_") || strings.HasPrefix(b, "test:") {
			return true
		}
	}
	return false
}
