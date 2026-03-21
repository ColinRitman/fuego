package app

import (
	"strings"
	"time"

	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
)

// Fire + ETH diamond ASCII art
const splashArt = `
                         .
                        /|\
                       / | \
                      /  |  \
                     /   |   \
                    / .--+--, \
                   / /   |   \ \
                  / /    |    \ \
                 / /     |     \ \
                / /      |      \ \
               /  '------+------'  \
              /  /        |        \  \
             /  /         |         \  \
            '--'          |          '--'
                          |
                     .-~~~|~~~-.
                   .'  .~~|~~.  '.
                  / .~' . | . '~. \
                 ( ( '~./\|/\.~' ) )
                  \ '.  '|||'  .' /
                   '.  '~===~'  .'
                     '~~~===~~~'
                      '..|..'
                        |||
                        |||
                     ~~~^^^~~~
`

const splashTitle = `
  ██╗  ██╗███████╗ ██████╗     ███████╗████████╗██╗  ██╗
  ╚██╗██╔╝██╔════╝██╔════╝     ██╔════╝╚══██╔══╝██║  ██║
   ╚███╔╝ █████╗  ██║  ███╗    █████╗     ██║   ███████║
   ██╔██╗ ██╔══╝  ██║   ██║    ██╔══╝     ██║   ██╔══██║
  ██╔╝ ██╗██║     ╚██████╔╝    ███████╗   ██║   ██║  ██║
  ╚═╝  ╚═╝╚═╝      ╚═════╝     ╚══════╝   ╚═╝   ╚═╝  ╚═╝
`

var (
	fireColors = []lipgloss.Color{
		lipgloss.Color("#FF4500"), // orange-red
		lipgloss.Color("#FF6347"), // tomato
		lipgloss.Color("#FF8C00"), // dark orange
		lipgloss.Color("#FFA500"), // orange
		lipgloss.Color("#FFD700"), // gold
	}

	ethColor    = lipgloss.Color("#627EEA") // ETH blue
	titleColor  = lipgloss.Color("#FF6347")
	dimColor    = lipgloss.Color("#555555")
)

type splashModel struct {
	width    int
	height   int
	frame    int
	done     bool
}

type splashTickMsg time.Time

func splashTick() tea.Cmd {
	return tea.Tick(120*time.Millisecond, func(t time.Time) tea.Msg {
		return splashTickMsg(t)
	})
}

func newSplashModel() splashModel {
	return splashModel{}
}

func (m splashModel) Init() tea.Cmd {
	return tea.Batch(splashTick(), tea.EnterAltScreen)
}

func (m splashModel) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	switch msg := msg.(type) {
	case tea.KeyMsg:
		m.done = true
		return m, nil
	case tea.WindowSizeMsg:
		m.width = msg.Width
		m.height = msg.Height
		return m, nil
	case splashTickMsg:
		m.frame++
		if m.frame > 25 {
			m.done = true
			return m, nil
		}
		return m, splashTick()
	case tea.MouseMsg:
		_ = msg
	}
	return m, nil
}

func (m splashModel) View() string {
	if m.width == 0 {
		return ""
	}

	// Animate fire colors on the diamond
	artLines := strings.Split(splashArt, "\n")
	var coloredArt strings.Builder

	for i, line := range artLines {
		if len(strings.TrimSpace(line)) == 0 {
			coloredArt.WriteString("\n")
			continue
		}

		// Bottom portion = fire, top = ETH blue, middle = gradient
		var c lipgloss.Color
		ratio := float64(i) / float64(len(artLines))
		if ratio < 0.55 {
			c = ethColor
		} else {
			fireIdx := (i + m.frame) % len(fireColors)
			c = fireColors[fireIdx]
		}

		style := lipgloss.NewStyle().Foreground(c)
		coloredArt.WriteString(style.Render(line))
		coloredArt.WriteString("\n")
	}

	// Title
	titleStyle := lipgloss.NewStyle().
		Foreground(titleColor).
		Bold(true)

	// Subtitle
	subStyle := lipgloss.NewStyle().
		Foreground(dimColor)

	content := lipgloss.JoinVertical(lipgloss.Center,
		coloredArt.String(),
		titleStyle.Render(splashTitle),
		"",
		subStyle.Render("atomic swap client — press any key"),
	)

	// Center on screen
	return lipgloss.Place(m.width, m.height,
		lipgloss.Center, lipgloss.Center,
		content,
	)
}
