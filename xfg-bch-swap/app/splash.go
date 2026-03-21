package app

import (
	"math"
	"math/rand"
	"strings"
	"time"

	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
)

// ── BCH circle logo geometry (green circle + ₿) ────────────────
const (
	logoR  = 10
	logoD  = logoR*2 + 1
	logoCX = logoR
	logoCY = logoR

	fW   = 38
	fH   = 10
	fLap = 3
)

// ── BCH palette (green) ────────────────────────────────────────
var (
	bchCircle = lipgloss.Color("#0AC18E") // BCH green
	bchCirclD = lipgloss.Color("#078F68") // darker edge
	bchLetter = lipgloss.Color("#FFFFFF") // white ₿
	bchGlow   = lipgloss.Color("#88FFCC") // shimmer
)

// Fire palette
var fPal = [11]lipgloss.Color{
	"#180400", "#3D0A00", "#6D1400", "#9E2000",
	"#D43800", "#FF5500", "#FF7F11", "#FFA940",
	"#FFD066", "#FFE899", "#FFFADD",
}
var fCh = [11]rune{' ', '.', ':', '*', '░', '▒', '▓', '█', '█', '█', '█'}

const splashTitle = `  ██╗  ██╗███████╗ ██████╗     ██████╗  ██████╗██╗  ██╗
  ╚██╗██╔╝██╔════╝██╔════╝     ██╔══██╗██╔════╝██║  ██║
   ╚███╔╝ █████╗  ██║  ███╗    ██████╔╝██║     ███████║
   ██╔██╗ ██╔══╝  ██║   ██║    ██╔══██╗██║     ██╔══██║
  ██╔╝ ██╗██║     ╚██████╔╝    ██████╔╝╚██████╗██║  ██║
  ╚═╝  ╚═╝╚═╝      ╚═════╝     ╚═════╝  ╚═════╝╚═╝  ╚═╝`

type splashModel struct {
	width, height int
	frame         int
	fire          [][]float64
}

type splashTickMsg time.Time

func splashTick() tea.Cmd {
	return tea.Tick(80*time.Millisecond, func(t time.Time) tea.Msg {
		return splashTickMsg(t)
	})
}

func newSplashModel() splashModel {
	f := make([][]float64, fH)
	for i := range f {
		f[i] = make([]float64, fW)
	}
	return splashModel{fire: f}
}

func (m splashModel) Init() tea.Cmd {
	return splashTick()
}

func (m splashModel) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	switch msg := msg.(type) {
	case tea.KeyMsg:
		return m, tea.Quit
	case tea.MouseMsg:
		_ = msg
	case tea.WindowSizeMsg:
		m.width = msg.Width
		m.height = msg.Height
	case splashTickMsg:
		m.frame++
		if m.frame > 40 {
			return m, tea.Quit
		}
		m.stepFire()
		return m, splashTick()
	}
	return m, nil
}

func (m *splashModel) stepFire() {
	cx := float64(fW) / 2.0
	for x := 0; x < fW; x++ {
		d := math.Abs(float64(x)-cx) / cx
		pk := 1.0 - d*0.4
		m.fire[fH-1][x] = pk * (0.6 + rand.Float64()*0.4)
		if fH > 1 {
			m.fire[fH-2][x] = pk * (0.25 + rand.Float64()*0.45)
		}
	}
	for y := 0; y < fH-2; y++ {
		for x := 0; x < fW; x++ {
			s := m.fire[y+1][x] * 3.0
			if x > 0 {
				s += m.fire[y+1][x-1]
			} else {
				s += m.fire[y+1][x]
			}
			if x < fW-1 {
				s += m.fire[y+1][x+1]
			} else {
				s += m.fire[y+1][x]
			}
			if y+2 < fH {
				s += m.fire[y+2][x]
			} else {
				s += m.fire[y+1][x]
			}
			m.fire[y][x] = math.Max(0, s/6.0-0.05-rand.Float64()*0.07)
		}
	}
}

// ── Circle with ₿ ──────────────────────────────────────────────

func inCircle(x, y int) bool {
	dx := float64(x-logoCX) * 0.5
	dy := float64(y - logoCY)
	return dx*dx+dy*dy <= float64(logoR)*float64(logoR)*0.25
}

func isEdge(x, y int) bool {
	dx := float64(x-logoCX) * 0.5
	dy := float64(y - logoCY)
	r2 := dx*dx + dy*dy
	outer := float64(logoR) * float64(logoR) * 0.25
	inner := float64(logoR-1) * float64(logoR-1) * 0.25
	return r2 <= outer && r2 > inner
}

// isB checks the BCH-logo ₿ tilted ~14° clockwise.
func isB(x, y int) bool {
	cx := float64(logoCX)
	cy := float64(logoCY)
	vx := (float64(x) - cx) * 0.5 // aspect-correct for terminal chars
	vy := float64(y) - cy
	const rad = 14.0 * math.Pi / 180.0
	c, s := math.Cos(rad), math.Sin(rad)
	rx := vx*c + vy*s
	ry := -vx*s + vy*c
	return isBUpright(int(math.Round(rx/0.5+cx)), int(math.Round(ry+cy)))
}

// isBUpright defines the upright ₿ geometry.
func isBUpright(x, y int) bool {
	bTop := logoCY - logoR*5/10
	bBot := logoCY + logoR*5/10
	bLeft := logoCX - logoR*6/10
	bRight := logoCX + logoR*8/10
	bMid := (bTop + bBot) / 2

	if y < bTop-1 || y > bBot+1 || x < bLeft || x > bRight {
		return false
	}

	bW := bRight - bLeft
	lc := x - bLeft

	// Vertical serif strokes extending above top and below bottom
	if y == bTop-1 || y == bBot+1 {
		return lc >= 2 && lc <= 4
	}

	// Vertical spine (left bar, 3 chars wide)
	if lc <= 2 {
		return true
	}

	// Top horizontal bar (2 rows)
	if y >= bTop && y <= bTop+1 {
		return lc <= bW-2
	}

	// Middle horizontal bar (2 rows)
	if y >= bMid && y <= bMid+1 {
		return lc <= bW-1
	}

	// Bottom horizontal bar (2 rows)
	if y >= bBot-1 && y <= bBot {
		return lc <= bW-2
	}

	// Top bump (right side curve, between top bar and middle bar)
	if y > bTop+1 && y < bMid {
		bumpRows := bMid - bTop - 2
		bumpMid := bTop + 2 + bumpRows/2
		dist := y - bumpMid
		if dist < 0 {
			dist = -dist
		}
		// Parabolic curve: widest at bumpMid
		maxExt := bW
		ext := maxExt - dist*2
		if ext < bW-3 {
			ext = bW - 3
		}
		if lc >= bW-4 && lc <= ext {
			return true
		}
	}

	// Bottom bump (right side curve, between middle and bottom bar)
	if y > bMid+1 && y < bBot-1 {
		bumpRows := bBot - 1 - bMid - 2
		bumpMid := bMid + 2 + bumpRows/2
		dist := y - bumpMid
		if dist < 0 {
			dist = -dist
		}
		maxExt := bW
		ext := maxExt - dist*2
		if ext < bW-3 {
			ext = bW - 3
		}
		if lc >= bW-4 && lc <= ext {
			return true
		}
	}

	return false
}

func logoPixel(row, col, frame int) (bool, lipgloss.Color) {
	if !inCircle(col, row) {
		return false, ""
	}

	// Shimmer wave
	if wp := frame % (logoD + 8); row == wp && !isEdge(col, row) {
		return true, bchGlow
	}

	// ₿ letter
	if isB(col, row) {
		return true, bchLetter
	}

	// Circle edge
	if isEdge(col, row) {
		return true, bchCirclD
	}

	// Circle interior
	return true, bchCircle
}

func (m splashModel) View() string {
	if m.width == 0 {
		return ""
	}

	artW := fW
	logoOff := (artW - logoD) / 2
	fStart := logoD - fLap
	totalH := fStart + fH

	lines := make([]string, 0, totalH)
	for row := 0; row < totalH; row++ {
		var b strings.Builder
		for col := 0; col < artW; col++ {
			// Logo circle pixel
			lc := col - logoOff
			if row < logoD && lc >= 0 && lc < logoD {
				if ok, c := logoPixel(row, lc, m.frame); ok {
					b.WriteString(lipgloss.NewStyle().Foreground(c).Render("█"))
					continue
				}
			}

			// Fire pixel
			fr := row - fStart
			if fr >= 0 && fr < fH && col >= 0 && col < fW {
				heat := m.fire[fr][col]
				if heat > 0.04 {
					idx := int(heat * 10)
					if idx > 10 {
						idx = 10
					}
					if fCh[idx] != ' ' {
						b.WriteString(lipgloss.NewStyle().Foreground(fPal[idx]).Render(string(fCh[idx])))
						continue
					}
				}
			}
			b.WriteString(" ")
		}
		lines = append(lines, b.String())
	}

	art := strings.Join(lines, "\n")
	tS := lipgloss.NewStyle().Foreground(lipgloss.Color("#0AC18E")).Bold(true)
	sS := lipgloss.NewStyle().Foreground(lipgloss.Color("#555555"))

	content := lipgloss.JoinVertical(lipgloss.Center,
		art,
		"",
		tS.Render(splashTitle),
		"",
		sS.Render("atomic swap client  ·  press any key"),
	)

	return lipgloss.Place(m.width, m.height,
		lipgloss.Center, lipgloss.Center,
		content,
	)
}
