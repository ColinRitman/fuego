// swapxfg/app/tui.go
package app

import (
	"fmt"
	"strings"
	"time"

	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
)

// refreshInterval controls the data polling rate.
const refreshInterval = 5 * time.Second

// tuiModel is the main bubbletea model for the trading terminal.
type tuiModel struct {
	cfg    Config
	client *FuegoClient
	wallet *WalletClient

	// Terminal dimensions
	width, height int

	// Market state
	activePair uint8
	data       *AllPairData
	connected  bool

	// Wallet state
	walletBalance uint64
	walletLocked  uint64
	afkMode       bool

	// Input
...
	cmdFocus  bool
	cursorOn  bool
	blinkTick int

	// Status
	lastErr   string
	statusMsg string
}

type refreshMsg struct {
	data *AllPairData
	err  error
}

type refreshTickMsg time.Time

func refreshTick() tea.Cmd {
	return tea.Tick(refreshInterval, func(t time.Time) tea.Msg {
		return refreshTickMsg(t)
	})
}

type cursorBlinkMsg time.Time

func cursorBlink() tea.Cmd {
	return tea.Tick(530*time.Millisecond, func(t time.Time) tea.Msg {
		return cursorBlinkMsg(t)
	})
}

func newTuiModel(cfg Config) tuiModel {
	m := tuiModel{
		cfg:        cfg,
		client:     NewFuegoClient(cfg.DaemonRPC),
		activePair: cfg.StartPair,
		data: &AllPairData{
			Offers: make(map[uint8][]SwapOffer),
			Prices: make(map[uint8]*SwapPriceResponse),
			Trades: make(map[uint8][]SwapTrade),
		},
		cursorOn: true,
	}
	if cfg.WalletRPC != "" {
		m.wallet = NewWalletClient(cfg.WalletRPC)
	}
	return m
}

type walletBalanceMsg struct {
	available uint64
	locked    uint64
	err       error
}

func (m tuiModel) Init() tea.Cmd {
	cmds := []tea.Cmd{
		m.fetchData(),
		refreshTick(),
		cursorBlink(),
	}
	if m.wallet != nil {
		cmds = append(cmds, m.fetchWallet())
	}
	return tea.Batch(cmds...)
}

func (m tuiModel) fetchData() tea.Cmd {
	client := m.client
	return func() tea.Msg {
		data, err := client.FetchAll(ActivePairs)
		return refreshMsg{data: data, err: err}
	}
}

func (m tuiModel) fetchWallet() tea.Cmd {
	if m.wallet == nil {
		return nil
	}
	w := m.wallet
	return func() tea.Msg {
		avail, locked, err := w.GetBalance()
		return walletBalanceMsg{available: avail, locked: locked, err: err}
	}
}

type commandResultMsg struct {
	msg string
	err error
}

func (m tuiModel) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	switch msg := msg.(type) {

	case tea.WindowSizeMsg:
		m.width = msg.Width
		m.height = msg.Height

	case tea.KeyMsg:
		return m.handleKey(msg)

	case refreshMsg:
		if msg.err != nil {
			m.connected = false
			m.lastErr = msg.err.Error()
		} else {
			m.connected = true
			m.lastErr = ""
			m.data = msg.data
		}

	case walletBalanceMsg:
		if msg.err == nil {
			m.walletBalance = msg.available
			m.walletLocked = msg.locked
		}

	case commandResultMsg:
		if msg.err != nil {
			m.statusMsg = "Error: " + msg.err.Error()
		} else {
			m.statusMsg = msg.msg
		}

	case refreshTickMsg:
		cmds := []tea.Cmd{m.fetchData(), refreshTick()}
		if m.wallet != nil {
			cmds = append(cmds, m.fetchWallet())
		}
		return m, tea.Batch(cmds...)

	case cursorBlinkMsg:
		m.blinkTick++
		m.cursorOn = m.blinkTick%2 == 0
		return m, cursorBlink()
	}

	return m, nil
}

func (m tuiModel) handleKey(msg tea.KeyMsg) (tea.Model, tea.Cmd) {
	k := msg.String()

	// Quit
	if k == "q" && !m.cmdFocus {
		return m, tea.Quit
	}
	if k == "esc" {
		if m.cmdFocus {
			m.cmdFocus = false
			m.cmdBuf = ""
			return m, nil
		}
		return m, tea.Quit
	}
	if k == "ctrl+c" {
		return m, tea.Quit
	}

	// Command input mode
	if m.cmdFocus {
		switch k {
		case "enter":
			cmd := m.handleCommand(m.cmdBuf)
			m.cmdBuf = ""
			m.cmdFocus = false
			return m, cmd
		case "backspace":
			if len(m.cmdBuf) > 0 {
				m.cmdBuf = m.cmdBuf[:len(m.cmdBuf)-1]
			}
		default:
			if len(k) == 1 {
				m.cmdBuf += k
			}
		}
		return m, nil
	}

	// Normal mode hotkeys
	switch k {
	case "/":
		m.cmdFocus = true
		return m, nil
	case "tab":
		m.activePair = nextPair(m.activePair)
	case "r":
		return m, m.fetchData()
	case "?":
		m.statusMsg = "0-3: pairs  /: cmd  r: refresh  q: quit"
	default:
		if len(k) == 1 {
			r := rune(k[0])
			if p := HotkeyPair(r); p != 255 {
				m.activePair = p
			}
		}
	}

	return m, nil
}

func (m *tuiModel) handleCommand(cmd string) tea.Cmd {
	cmd = strings.TrimSpace(cmd)
	if cmd == "" {
		return nil
	}
	parts := strings.Fields(cmd)
	switch parts[0] {
	case "pair":
		if len(parts) > 1 {
			p := PairFromString(parts[1])
			if p != 255 {
				m.activePair = p
			} else {
				m.statusMsg = "unknown pair: " + parts[1]
			}
		}
	case "pool":
		if m.data.FeePool != nil {
			fp := m.data.FeePool
			m.statusMsg = fmt.Sprintf("Pool: %s XFG | CD Locked: %s XFG | Epoch Fees: %s XFG | EFiers: %d",
				FormatXfg(fp.FeePoolBalance), FormatXfg(fp.TotalCdLocked),
				FormatXfg(fp.CurrentEpochSwapFees), fp.ActiveEfierCount)
		} else {
			m.statusMsg = "fee pool data unavailable"
		}
	case "offer":
		if m.wallet == nil {
			m.statusMsg = "connect wallet to create offers"
			return nil
		}
		if len(parts) < 3 {
			m.statusMsg = "usage: offer <amount> <rate> [buy|sell]"
			return nil
		}
		amt, _ := strconv.ParseFloat(parts[1], 64)
		rate, _ := strconv.ParseFloat(parts[2], 64)
		isSell := true
		if len(parts) > 3 && parts[3] == "buy" {
			isSell = false
		}
		
		xfgAmt := uint64(amt * 1e7)
		rateNum := uint64(rate * 1e7)
		
		w := m.wallet
		c := m.client
		pair := m.activePair
		
		return func() tea.Msg {
			sigResp, err := w.SignOffer(SignOfferRequest{
				XfgAmount: xfgAmt,
				RateNum:   rateNum,
				Pair:      pair,
				TTLBlocks: 180, // ~24h
			})
			if err != nil {
				return commandResultMsg{err: err}
			}
			
			err = c.SubmitSwapOffer(SubmitSwapOfferRequest{
				OfferID:     sigResp.OfferID,
				IsSell:      isSell,
				XfgAmount:   xfgAmt,
				RateNum:     rateNum,
				Pair:        pair,
				MakerPubKey: sigResp.MakerPubKey,
				Signature:   sigResp.Signature,
				TTLBlocks:   180,
			})
			if err != nil {
				return commandResultMsg{err: err}
			}
			return commandResultMsg{msg: "offer submitted successfully"}
		}

	case "accept":
		if m.wallet == nil {
			m.statusMsg = "connect wallet to accept swaps"
			return nil
		}
		if len(parts) < 2 {
			m.statusMsg = "usage: accept <offer_id>"
			return nil
		}
		offerID := parts[1]
		var found bool
		var offer SwapOffer
		for _, o := range m.data.Offers[m.activePair] {
			if strings.HasPrefix(o.OfferID, offerID) {
				offer = o
				found = true
				break
			}
		}
		if !found {
			m.statusMsg = "offer not found: " + offerID
			return nil
		}

		w := m.wallet
		return func() tea.Msg {
			_, err := w.InitiateSwap(InitiateSwapRequest{
				OfferID:     offer.OfferID,
				Pair:        offer.Pair,
				Amount:      offer.XfgAmount,
				MakerPubKey: offer.MakerPubKey,
			})
			if err != nil {
				return commandResultMsg{err: err}
			}
			return commandResultMsg{msg: "swap initiated"}
		}

	case "afk":
		if len(parts) > 1 {
			if parts[1] == "on" {
				m.afkMode = true
			} else if parts[1] == "off" {
				m.afkMode = false
			}
		} else {
			m.afkMode = !m.afkMode
		}
		m.statusMsg = fmt.Sprintf("AFK Mode: %v", m.afkMode)

	case "help":
		m.statusMsg = "pair <n> | offer <amt> <rate> [buy|sell] | accept <id> | afk [on|off] | pool | q: quit"
	default:
		m.statusMsg = "unknown: " + cmd + " (type help)"
	}
	return nil
}

func nextPair(cur uint8) uint8 {
	for i, p := range ActivePairs {
		if p == cur {
			return ActivePairs[(i+1)%len(ActivePairs)]
		}
	}
	return ActivePairs[0]
}

// ─── view ─────────────────────────────────────────────────────────────

func (m tuiModel) View() string {
	if m.width == 0 {
		return ""
	}

	w := m.width
	h := m.height

	// Layout rows: ticker(2) + main(h-4) + input(1) + status(1)
	tickerH := 1
	inputH := 1
	statusH := 1
	mainH := h - tickerH - inputH - statusH - 1 // -1 for borders
	if mainH < 5 {
		mainH = 5
	}

	// ── Ticker ──
	ticker := RenderTicker(m.activePair, m.data.Prices, m.data.Height, m.data.FeePool, w, m.connected, m.afkMode)

	// ── Main area: chart (left 60%) | orderbook+tape (right 40%) ──
	rightW := w * 38 / 100
	if rightW < 30 {
		rightW = 30
	}
	leftW := w - rightW - 3 // 3 for border
	if leftW < 20 {
		leftW = 20
	}

	// Chart
	chartH := mainH - 2 // leave room for price line
	if chartH < 3 {
		chartH = 3
	}
	trades := m.data.Trades[m.activePair]
	chart := RenderChart(trades, leftW, chartH)
	priceLine := RenderPriceLine(m.activePair, m.data.Prices)
	leftPanel := lipgloss.JoinVertical(lipgloss.Left, chart, priceLine)

	// Orderbook + tape
	obH := mainH * 55 / 100
	if obH < 5 {
		obH = 5
	}
	tapeH := mainH - obH
	if tapeH < 3 {
		tapeH = 3
	}

	offers := m.data.Offers[m.activePair]
	ob := RenderOrderbook(offers, rightW, obH)
	tape := RenderTape(trades, rightW, tapeH)
	rightPanel := lipgloss.JoinVertical(lipgloss.Left, ob, tape)

	// Join left | right with separator
	sep := lipgloss.NewStyle().Foreground(ColorMuted).Render(
		strings.Repeat("│\n", mainH))
	mainArea := lipgloss.JoinHorizontal(lipgloss.Top, leftPanel, sep, rightPanel)

	// ── Input bar ──
	balStr := ""
	if m.wallet != nil {
		balStr = FormatXfg(m.walletBalance)
	}
	inputBar := RenderInputBar(m.cmdBuf, m.cursorOn && m.cmdFocus, balStr, m.cfg.DaemonRPC, m.connected, w)

	// ── Status ──
	status := ""
	if m.lastErr != "" {
		status = StyleStatus.Render(m.lastErr)
	} else if m.statusMsg != "" {
		status = StyleMuted.Render(m.statusMsg)
	}

	// ── Border ──
	hline := lipgloss.NewStyle().Foreground(ColorMuted).Render(strings.Repeat("─", w))

	return lipgloss.JoinVertical(lipgloss.Left,
		ticker,
		hline,
		mainArea,
		hline,
		inputBar,
		status,
	)
}
