package main

import (
	"encoding/json"
	"fmt"
	"strings"
	"time"

	"github.com/BolgarMaxym97/claude-code-desk-clock/host/internal/cfg"
	"github.com/BolgarMaxym97/claude-code-desk-clock/host/internal/clock"
)

// langAnswer maps what people type for the clock language to its code ("" = English, the default).
func langAnswer(s string) (string, bool) {
	switch strings.ToLower(strings.TrimSpace(s)) {
	case "", "e", "en", "eng", "english":
		return "en", true
	case "u", "uk", "ua", "ukr", "укр", "українська", "ukrainian":
		return "uk", true
	}
	return "", false
}

func sendLang(c cfg.Config, code string) error {
	body, _ := json.Marshal(map[string]string{"lang": code})
	if err := clock.Post(c.URL, c.Token, "/api/settings", body, 5*time.Second); err != nil {
		return fmt.Errorf("language: %v (this clock firmware may be too old: run `claude-clock update`)", err)
	}
	name := map[string]string{"en": "English", "uk": "Українська"}[code]
	fmt.Println("Clock language:", name)
	return nil
}

// askLang: --lang flag, otherwise ask (Enter = English).
func askLang(args []string, auto bool) string {
	if v, ok := flag(args, "--lang"); ok {
		if code, ok := langAnswer(v); ok {
			return code
		}
	}
	if auto {
		return "en"
	}
	for {
		code, ok := langAnswer(ask("Clock language: [E]nglish / [U]krainian (Українська)? "))
		if ok {
			return code
		}
	}
}
