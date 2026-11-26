//go:build !windows

package main

import (
	"os/exec"
	"syscall"
)

func detach(c *exec.Cmd) { c.SysProcAttr = &syscall.SysProcAttr{Setsid: true} }

// shellCommand runs the user's original statusline the way Claude Code would.
func shellCommand(cmd string) *exec.Cmd {
	if p, err := exec.LookPath("bash"); err == nil {
		return exec.Command(p, "-c", cmd)
	}
	return exec.Command("/bin/sh", "-c", cmd)
}
