// swapxfg/app/ticker.go
package app

import (
	"fmt"
	"strconv"
	"strings"

	"github.com/charmbracelet/lipgloss"
)

// RenderTicker draws the top market ticker bar showing all pairs + block height + fee pool.
func RenderTicker(activePair uint8, prices map[uint8]*SwapPriceResponse, height uint64, feePool *FeePoolInfo, width int, connected, afk bool) string {
	var parts []string

	// Logo
	logo := StyleAccent.Render("⚛️SWAPXFG")
	parts = append(parts, logo)

	if afk {
		parts = append(parts, StyleBull.Render("[AFK ON]"))
	} else {
		parts = append(parts, StyleMuted.Render("[AFK OFF]"))
	}

	for _, p := range ActivePairs {
		name := PairShort(p)
		pr := prices[p]
		rate := "—"
		if pr != nil && pr.CompositeRate != "" {
			rate = pr.CompositeRate
		}

		var styled string
		if p == activePair {
			styled = StyleActiveTab.Render(fmt.Sprintf(" %s %s ", name, rate))
		} else {
			styled = StyleInactiveTab.Render(fmt.Sprintf("%s %s", name, rate))
		}
		parts = append(parts, styled)
	}

	// Fee pool summary
	if feePool != nil {
		poolXfg := FormatXfg(feePool.FeePoolBalance)
		cdLocked := FormatXfg(feePool.TotalCdLocked)
		yieldPct := float64(feePool.CurrentEpochFeeRate) / 1e6 * 100.0
		fpStr := fmt.Sprintf("POOL %s  CD %s  APR %.2f%%", poolXfg, cdLocked, yieldPct)
		parts = append(parts, StyleMuted.Render(fpStr))
	}

	// Block height
	blk := StyleMuted.Render(fmt.Sprintf("BLK %d", height))
	parts = append(parts, blk)

	// Connection indicator
	if connected {
		parts = append(parts, lipgloss.NewStyle().Foreground(ColorConnOK).Render("●"))
	} else {
		parts = append(parts, lipgloss.NewStyle().Foreground(ColorConnLost).Render("●"))
	}

	row := strings.Join(parts, "  ")
	return lipgloss.NewStyle().Width(width).Render(row)
}

// FormatXfg converts atomic units to human-readable XFG (7 decimal places).
func FormatXfg(atomic uint64) string {
	whole := atomic / 10_000_000
	frac := atomic % 10_000_000
	if frac == 0 {
		return fmt.Sprintf("%d", whole)
	}
	s := fmt.Sprintf("%d.%07d", whole, frac)
	return strings.TrimRight(s, "0")
}

// RenderPriceLine shows TWAP + composite below the chart.
func RenderPriceLine(pair uint8, prices map[uint8]*SwapPriceResponse) string {
	pr := prices[pair]
	if pr == nil {
		return StyleMuted.Render("  TWAP: —  Composite: —")
	}
	twap := pr.Twap
	comp := pr.CompositeRate
	if twap == "" {
		twap = "—"
	}
	if comp == "" {
		comp = "—"
	}
	
	twapStyled := StyleAccent.Render(twap)
	
	xfgUsd := ""
	if pr.XfgUsdMid != "" {
		v, err := strconv.ParseFloat(pr.XfgUsdMid, 64)
		if err == nil && v > 0 {
			xfgUsd = fmt.Sprintf("  XFG $%.4f", v)
		}
	}
	return StyleMuted.Render(fmt.Sprintf("  TWAP: ")) + twapStyled + StyleMuted.Render(fmt.Sprintf("  Composite: %s%s", comp, xfgUsd))
}
