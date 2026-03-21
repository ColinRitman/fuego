package app

import (
	"fmt"
	"strconv"
	"strings"
	"time"

	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
)

// Styles
var (
	headerStyle = lipgloss.NewStyle().
			Foreground(lipgloss.Color("#627EEA")).
			Bold(true)

	priceStyle = lipgloss.NewStyle().
			Foreground(lipgloss.Color("#FFD700")).
			Bold(true)

	greenStyle = lipgloss.NewStyle().
			Foreground(lipgloss.Color("#00FF7F"))

	redStyle = lipgloss.NewStyle().
			Foreground(lipgloss.Color("#FF4500"))

	dimStyle = lipgloss.NewStyle().
			Foreground(lipgloss.Color("#666666"))

	inputStyle = lipgloss.NewStyle().
			Foreground(lipgloss.Color("#FFFFFF")).
			Bold(true)

	statusStyle = lipgloss.NewStyle().
			Foreground(lipgloss.Color("#FF6347"))

	borderStyle = lipgloss.NewStyle().
			Border(lipgloss.RoundedBorder()).
			BorderForeground(lipgloss.Color("#333333")).
			Padding(0, 1)

	activeTabStyle = lipgloss.NewStyle().
			Foreground(lipgloss.Color("#627EEA")).
			Bold(true).
			Underline(true)

	inactiveTabStyle = lipgloss.NewStyle().
			Foreground(lipgloss.Color("#555555"))
)

type tuiModel struct {
	client    *FuegoClient
	width     int
	height    int
	offers    []SwapOffer
	trades    []SwapTrade
	price     *SwapPriceResponse
	input     string
	status    string
	statusAt  time.Time
	connected bool
	lastFetch time.Time
	tab       int // 0=book, 1=trades, 2=info
}

type refreshMsg struct{}
type tickMsg time.Time

func newTuiModel(client *FuegoClient) tuiModel {
	return tuiModel{
		client: client,
		tab:    0,
	}
}

func (m tuiModel) Init() tea.Cmd {
	return tea.Batch(
		func() tea.Msg { return refreshMsg{} },
		tea.Tick(5*time.Second, func(t time.Time) tea.Msg { return tickMsg(t) }),
	)
}

func (m tuiModel) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	switch msg := msg.(type) {
	case tea.KeyMsg:
		switch msg.String() {
		case "ctrl+c", "q":
			return m, tea.Quit
		case "tab":
			m.tab = (m.tab + 1) % 3
			return m, nil
		case "1":
			m.tab = 0
			return m, nil
		case "2":
			m.tab = 1
			return m, nil
		case "3":
			m.tab = 2
			return m, nil
		case "r":
			return m, func() tea.Msg { return refreshMsg{} }
		case "backspace":
			if len(m.input) > 0 {
				m.input = m.input[:len(m.input)-1]
			}
			return m, nil
		case "enter":
			m.processCommand()
			m.input = ""
			return m, nil
		default:
			if len(msg.String()) == 1 && msg.String()[0] >= 32 {
				m.input += msg.String()
			}
		}
		return m, nil

	case tea.WindowSizeMsg:
		m.width = msg.Width
		m.height = msg.Height
		return m, nil

	case refreshMsg:
		m.refresh()
		return m, nil

	case tickMsg:
		if time.Since(m.lastFetch) > 10*time.Second {
			m.refresh()
		}
		return m, tea.Tick(5*time.Second, func(t time.Time) tea.Msg { return tickMsg(t) })
	}
	return m, nil
}

func (m *tuiModel) refresh() {
	offers, err := m.client.GetOffers()
	if err != nil {
		m.connected = false
		m.status = "EFier offline: " + err.Error()
		m.statusAt = time.Now()
		return
	}
	m.offers = offers
	m.connected = true

	price, err := m.client.GetPrice()
	if err == nil {
		m.price = price
	}

	trades, err := m.client.GetTrades(20)
	if err == nil {
		m.trades = trades
	}

	m.lastFetch = time.Now()
}

func (m *tuiModel) processCommand() {
	parts := strings.Fields(m.input)
	if len(parts) == 0 {
		return
	}

	cmd := strings.ToLower(parts[0])
	switch cmd {
	case "accept":
		if len(parts) < 2 {
			m.status = "Usage: accept <offer_id>"
		} else {
			m.status = "Accept flow: lock ETH in HTLC for offer " + parts[1][:8] + "..."
		}
	case "help":
		m.status = "Commands: accept <id> | r=refresh | 1/2/3=tabs | q=quit"
	default:
		m.status = "Unknown: " + cmd + " (type 'help')"
	}
	m.statusAt = time.Now()
}

func (m tuiModel) View() string {
	if m.width == 0 {
		return ""
	}

	// Header bar
	header := m.renderHeader()

	// Tab content
	var content string
	switch m.tab {
	case 0:
		content = m.renderOrderBook()
	case 1:
		content = m.renderTrades()
	case 2:
		content = m.renderInfo()
	}

	// Input bar
	inputBar := m.renderInput()

	// Compose
	return lipgloss.JoinVertical(lipgloss.Left,
		header,
		"",
		content,
		"",
		inputBar,
	)
}

func (m tuiModel) renderHeader() string {
	// Connection indicator
	var connStr string
	if m.connected {
		connStr = greenStyle.Render("●") + dimStyle.Render(" EFier")
	} else {
		connStr = redStyle.Render("●") + dimStyle.Render(" offline")
	}

	// Price
	var priceStr string
	if m.price != nil {
		rate, _ := strconv.ParseFloat(m.price.CompositeRate, 64)
		if rate <= 0 {
			rate, _ = strconv.ParseFloat(m.price.SeedRate, 64)
		}
		if rate > 0 {
			ethPerXfg := 1.0 / rate
			priceStr = priceStyle.Render(fmt.Sprintf("1 XFG = %.8f ETH", ethPerXfg))

			// USD range
			usdMid, _ := strconv.ParseFloat(m.price.XfgUsdMid, 64)
			if usdMid > 0 {
				priceStr += dimStyle.Render(fmt.Sprintf("  ($%.4f)", usdMid))
			}
		}
	}

	title := headerStyle.Render("◆ XFG⇄ETH")

	// Tabs
	tabs := []string{"[1]Book", "[2]Trades", "[3]Info"}
	var tabStr string
	for i, t := range tabs {
		if i == m.tab {
			tabStr += activeTabStyle.Render(t) + " "
		} else {
			tabStr += inactiveTabStyle.Render(t) + " "
		}
	}

	left := title + "  " + priceStr
	right := tabStr + "  " + connStr

	gap := m.width - lipgloss.Width(left) - lipgloss.Width(right)
	if gap < 1 {
		gap = 1
	}

	return left + strings.Repeat(" ", gap) + right
}

func (m tuiModel) renderOrderBook() string {
	if len(m.offers) == 0 {
		return dimStyle.Render("  No offers available. Waiting for XFG holders to post swap offers...")
	}

	// Header
	hdr := fmt.Sprintf("  %-10s %16s %16s %14s %s",
		"OFFER", "XFG AMOUNT", "RATE (XFG/ETH)", "ETH VALUE", "AGE")
	lines := []string{dimStyle.Render(hdr)}

	maxLines := m.height - 8
	if maxLines < 3 {
		maxLines = 3
	}

	for i, o := range m.offers {
		if i >= maxLines {
			lines = append(lines, dimStyle.Render(fmt.Sprintf("  ... and %d more", len(m.offers)-i)))
			break
		}

		xfg := float64(o.XfgAmount) / 1e7
		rate := float64(o.RateNum) / 1e7
		ethVal := xfg / rate

		age := formatAge(o.Timestamp)
		id := o.OfferID
		if len(id) > 8 {
			id = id[:8]
		}

		line := fmt.Sprintf("  %-10s %13.2f XFG %13.1f %11.6f ETH %s",
			id, xfg, rate, ethVal, age)

		lines = append(lines, greenStyle.Render(line))
	}

	return strings.Join(lines, "\n")
}

func (m tuiModel) renderTrades() string {
	if len(m.trades) == 0 {
		return dimStyle.Render("  No recent trades.")
	}

	hdr := fmt.Sprintf("  %16s %16s %14s %s",
		"XFG VOLUME", "RATE (XFG/ETH)", "ETH VALUE", "AGE")
	lines := []string{dimStyle.Render(hdr)}

	maxLines := m.height - 8
	if maxLines < 3 {
		maxLines = 3
	}

	for i, t := range m.trades {
		if i >= maxLines {
			break
		}
		xfg := float64(t.XfgAmount) / 1e7
		rate, _ := strconv.ParseFloat(t.Rate, 64)
		ethVal := 0.0
		if rate > 0 {
			ethVal = xfg / rate
		}
		age := formatAge(t.Timestamp)

		line := fmt.Sprintf("  %13.2f XFG %13.1f %11.6f ETH %s",
			xfg, rate, ethVal, age)
		lines = append(lines, line)
	}

	return strings.Join(lines, "\n")
}

func (m tuiModel) renderInfo() string {
	var lines []string

	lines = append(lines, headerStyle.Render("  Price Sources"))
	lines = append(lines, "")

	if m.price != nil && len(m.price.Sources) > 0 {
		for _, src := range m.price.Sources {
			rate, _ := strconv.ParseFloat(src.Rate, 64)
			weight, _ := strconv.ParseFloat(src.Weight, 64)
			staleStr := ""
			if src.Stale {
				staleStr = redStyle.Render(" [stale]")
			}
			lines = append(lines, fmt.Sprintf("  %-20s rate=%.1f  weight=%.1f%s",
				src.Name, rate, weight, staleStr))
		}
	} else {
		lines = append(lines, dimStyle.Render("  No price sources available"))
	}

	lines = append(lines, "")
	lines = append(lines, headerStyle.Render("  Cross-Pair XFG Price"))
	if m.price != nil {
		lo, _ := strconv.ParseFloat(m.price.XfgUsdLow, 64)
		hi, _ := strconv.ParseFloat(m.price.XfgUsdHigh, 64)
		mid, _ := strconv.ParseFloat(m.price.XfgUsdMid, 64)
		if mid > 0 {
			lines = append(lines, fmt.Sprintf("  USD range: $%.4f — $%.4f  (mid: $%.4f)", lo, hi, mid))
		}
		for _, pi := range m.price.PairImplied {
			usd, _ := strconv.ParseFloat(pi.ImpliedUsd, 64)
			name := pairName(pi.Pair)
			lines = append(lines, fmt.Sprintf("    via %s: $%.4f", name, usd))
		}
	}

	lines = append(lines, "")
	lines = append(lines, headerStyle.Render("  Swap Flow"))
	lines = append(lines, dimStyle.Render("  1. Browse offers from XFG holders (Book tab)"))
	lines = append(lines, dimStyle.Render("  2. accept <offer_id> → locks your ETH in HTLC"))
	lines = append(lines, dimStyle.Render("  3. XFG maker sees hashlock, locks XFG"))
	lines = append(lines, dimStyle.Render("  4. You claim XFG (reveals preimage)"))
	lines = append(lines, dimStyle.Render("  5. Maker claims your ETH with revealed preimage"))

	lines = append(lines, "")
	lines = append(lines, dimStyle.Render("  HEAT/ETH pool:  1 XFG = 10,000,000 HEAT (ERC-20)"))

	return strings.Join(lines, "\n")
}

func (m tuiModel) renderInput() string {
	// Status message (fades after 5s)
	var statusLine string
	if m.status != "" && time.Since(m.statusAt) < 5*time.Second {
		statusLine = statusStyle.Render("  " + m.status)
	}

	prompt := inputStyle.Render("  > " + m.input + "█")
	hint := dimStyle.Render("  accept <id> | r=refresh | tab=switch | q=quit")

	if statusLine != "" {
		return lipgloss.JoinVertical(lipgloss.Left, statusLine, prompt, hint)
	}
	return lipgloss.JoinVertical(lipgloss.Left, prompt, hint)
}

func formatAge(ts uint64) string {
	if ts == 0 {
		return "-"
	}
	d := time.Since(time.Unix(int64(ts), 0))
	switch {
	case d < time.Minute:
		return fmt.Sprintf("%ds", int(d.Seconds()))
	case d < time.Hour:
		return fmt.Sprintf("%dm", int(d.Minutes()))
	case d < 24*time.Hour:
		return fmt.Sprintf("%dh", int(d.Hours()))
	default:
		return fmt.Sprintf("%dd", int(d.Hours()/24))
	}
}

func pairName(pair uint8) string {
	switch pair {
	case 0:
		return "XMR"
	case 1:
		return "ETH"
	case 2:
		return "BCH"
	default:
		return "?"
	}
}
