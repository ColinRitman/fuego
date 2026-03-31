// swapxfg/app/orderbook.go
package app

import (
	"fmt"
	"sort"
	"strconv"
	"strings"

	"github.com/charmbracelet/lipgloss"
)

// RenderOrderbook draws the ask/bid display for a given pair.
// width and height constrain the panel size.
func RenderOrderbook(offers []SwapOffer, width, height int) string {
	title := lipgloss.NewStyle().Bold(true).Foreground(ColorActiveTab).
		Width(width).Align(lipgloss.Center).Render("ORDER BOOK")

	sep := lipgloss.NewStyle().Foreground(ColorMuted).
		Width(width).Align(lipgloss.Center).Render(strings.Repeat("─", width-2))

	if len(offers) == 0 {
		empty := StyleMuted.Render("  no offers")
		return lipgloss.JoinVertical(lipgloss.Left, title, sep, empty)
	}

	type entry struct {
		rate   float64
		amount float64
		raw    SwapOffer
	}

	var asks, bids []entry
	for _, o := range offers {
		r, err := strconv.ParseFloat(fmt.Sprintf("%d.%07d", o.RateNum/10000000, o.RateNum%10000000), 64)
		if err != nil || r <= 0 {
			r = float64(o.RateNum)
		}
		amt := float64(o.XfgAmount) / 1e7
		e := entry{rate: r, amount: amt, raw: o}
		if o.IsSell {
			asks = append(asks, e)
		} else {
			bids = append(bids, e)
		}
	}

	// Sort asks: lowest rate first (cheapest)
	sort.Slice(asks, func(i, j int) bool {
		return asks[i].rate < asks[j].rate
	})
	// Sort bids: highest rate first (best bid)
	sort.Slice(bids, func(i, j int) bool {
		return bids[i].rate > bids[j].rate
	})

	maxRows := (height - 4) / 2
	if maxRows < 1 {
		maxRows = 1
	}

	var lines []string
	lines = append(lines, title, sep)

	// Asks: displayed bottom-to-top, cheapest nearest the spread line
	// So we show them in reverse order: highest at top of ask section
	askCount := len(asks)
	if askCount > maxRows {
		askCount = maxRows
	}
	for i := askCount - 1; i >= 0; i-- {
		a := asks[i]
		line := fmt.Sprintf("  ASK %10.5f  %8.1f XFG", a.rate, a.amount)
		lines = append(lines, StyleBear.Render(truncPad(line, width)))
	}

	// Spread line
	spread := "—"
	if len(asks) > 0 && len(bids) > 0 {
		lowestAsk := asks[0].rate
		highestBid := bids[0].rate
		spreadValue := lowestAsk - highestBid
		if spreadValue < 0 {
			spread = "CROSS"
		} else {
			spread = fmt.Sprintf("%.5f", spreadValue)
		}
	}
	spreadLine := StyleSpread.Render(
		lipgloss.NewStyle().Width(width).Align(lipgloss.Center).
			Render(fmt.Sprintf("━━━ spread %s ━━━", spread)))
	lines = append(lines, spreadLine)

	// Bids: best bid at top of bid section
	for i := 0; i < len(bids) && i < maxRows; i++ {
		b := bids[i]
		line := fmt.Sprintf("  BID %10.5f  %8.1f XFG", b.rate, b.amount)
		lines = append(lines, StyleBull.Render(truncPad(line, width)))
	}

	return lipgloss.JoinVertical(lipgloss.Left, lines...)
}

func truncPad(s string, w int) string {
	if len(s) >= w {
		return s[:w]
	}
	return s + strings.Repeat(" ", w-len(s))
}
