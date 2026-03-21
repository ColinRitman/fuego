package app

import (
	"fmt"

	tea "github.com/charmbracelet/bubbletea"
)

// Run starts the swap client: splash screen → main TUI.
func Run(cfg Config) error {
	// Phase 1: splash screen
	splash := newSplashModel()
	p := tea.NewProgram(splash)
	result, err := p.Run()
	if err != nil {
		return fmt.Errorf("splash: %w", err)
	}

	_ = result

	// Phase 2: main TUI
	client := NewFuegoClient(cfg.EfierRPC)
	tui := newTuiModel(client)

	p2 := tea.NewProgram(tui, tea.WithAltScreen())
	_, err = p2.Run()
	if err != nil {
		return fmt.Errorf("tui: %w", err)
	}

	return nil
}
