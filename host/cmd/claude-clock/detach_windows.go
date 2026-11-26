//go:build windows

package main

import (
	"os/exec"
	"syscall"
)

const (
	detachedProcess = 0x00000008
	createNoWindow  = 0x08000000
	newProcessGroup = 0x00000200
)

func detach(c *exec.Cmd) {
	c.SysProcAttr = &syscall.SysProcAttr{CreationFlags: detachedProcess | createNoWindow | newProcessGroup, HideWindow: true}
}

// shellCommand: Claude Code on Windows runs commands through Git Bash; fall back to cmd.exe.
func shellCommand(cmd string) *exec.Cmd {
	if p, err := exec.LookPath("bash"); err == nil {
		return exec.Command(p, "-c", cmd)
	}
	return exec.Command("cmd", "/C", cmd)
}
