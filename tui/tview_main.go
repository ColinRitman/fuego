package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"github.com/gdamore/tcell/v2"
	"github.com/rivo/tview"
)

// RPCResponse represents a JSON-RPC response
type RPCResponse struct {
	ID      interface{} `json:"id"`
	JSONRPC string      `json:"jsonrpc"`
	Result  interface{} `json:"result,omitempty"`
	Error   *RPCError   `json:"error,omitempty"`
}

// RPCError represents a JSON-RPC error
type RPCError struct {
	Code    int    `json:"code"`
	Message string `json:"message"`
}

// NodeInfo represents node information
type NodeInfo struct {
	Height int `json:"height"`
	Peers  int `json:"peers"`
}

// AppState represents the application state
type AppState struct {
	app             *tview.Application
	pages           *tview.Pages
	network         string
	nodeCmd         *exec.Cmd
	walletCmd       *exec.Cmd
	logs            []string
	isNodeRunning   bool
	isWalletRunning bool
}

var appState AppState

const fuegoLogo = `
    ███████╗██╗   ██╗███████╗ ██████╗  ██████╗
    ██╔════╝██║   ██║██╔════╝██╔════╝ ██╔═══██╗
    █████╗  ██║   ██║█████╗  ██║  ███╗██║   ██║
    ██╔══╝  ██║   ██║██╔══╝  ██║   ██║██║   ██║
    ██║     ╚██████╔╝███████╗╚██████╔╝╚██████╔╝
    ╚═╝      ╚═════╝ ╚══════╝ ╚═════╝  ╚═════╝ `

func main() {
	appState = AppState{
		app:     tview.NewApplication(),
		pages:   tview.NewPages(),
		network: "mainnet",
		logs:    make([]string, 0),
	}
	CurrentConfig = MainnetConfig

	appState.app.EnableMouse(true)

	tview.Styles.PrimaryTextColor = tcell.ColorOrange
	tview.Styles.SecondaryTextColor = tcell.ColorYellow
	tview.Styles.TertiaryTextColor = tcell.ColorRed
	tview.Styles.BorderColor = tcell.ColorOrange
	tview.Styles.TitleColor = tcell.ColorOrange

	// Show the splash then go to main menu
	showSplashScreen()

	if err := appState.app.SetRoot(appState.pages, true).SetFocus(appState.pages).Run(); err != nil {
		panic(err)
	}
}

// showSplashScreen shows flashing FUEGO
func showSplashScreen() {
	splash := tview.NewTextView().
		SetTextAlign(tview.AlignCenter).
		SetDynamicColors(true)
	splash.SetBackgroundColor(tcell.ColorBlack)

	appState.pages.AddPage("splash", splash, true, true)

	// Fuego marquee: cycle orange/yellow/white rapidly with bulb frames
	go func() {
		type frame struct {
			color tcell.Color
			border string
		}
		colors := []frame{
			{tcell.ColorOrange, " *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  * "},
			{tcell.ColorYellow, "  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *"},
			{tcell.ColorWhite, " *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  * "},
			{tcell.ColorOrange, "*  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  "},
			{tcell.ColorYellow, "  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *"},
			{tcell.ColorWhite, " *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  * "},
		}

		// Flash for ~3 seconds (18 frames * 170ms)
		for i := 0; i < 18; i++ {
			f := colors[i%len(colors)]
			appState.app.QueueUpdateDraw(func() {
				splash.SetTextColor(f.color)
				splash.SetText(fmt.Sprintf("\n%s\n%s\n%s\n\n         The Fire Blockchain\n",
					f.border, fuegoLogo, f.border))
			})
			time.Sleep(170 * time.Millisecond)
		}

		// Hold steady orange for a beat
		appState.app.QueueUpdateDraw(func() {
			splash.SetTextColor(tcell.ColorOrange)
			splash.SetText(fmt.Sprintf("\n%s\n\n  Money To Burn + COLD Return \n",
				fuegoLogo))
		})
		time.Sleep(800 * time.Millisecond)

		// Transition to main menu
		appState.app.QueueUpdateDraw(func() {
			createMainMenu()
			appState.pages.SwitchToPage("main")
			appState.pages.RemovePage("splash")
		})
	}()
}

// createMainMenu creates the main menu screen
func createMainMenu() {
	createMainMenuWithNetwork(appState.network)
}

func createMainMenuWithNetwork(network string) {
	// Header: network info
	headerText := fmt.Sprintf(" [orange]FUEGO[white] | Network: [yellow]%s[white] | Node: [yellow]%s[white] / Wallet: [yellow]%s[white] | Press [orange]'n'[white] to toggle network",
		CurrentConfig.NetworkName, CurrentConfig.NodeBinary, CurrentConfig.WalletBinary)
	header := tview.NewTextView().
		SetDynamicColors(true).
		SetText(headerText).
		SetBackgroundColor(tcell.ColorBlack)

	// Build grouped menu list
	list := tview.NewList().
		SetMainTextColor(tcell.ColorOrange).
		SetSecondaryTextColor(tcell.ColorDarkGray).
		SetSelectedTextColor(tcell.ColorBlack).
		SetSelectedBackgroundColor(tcell.ColorOrange)

	// --- Node ---
	list.AddItem("[::b]--- Node ---", "", 0, nil)
	list.AddItem("  Start Node", fmt.Sprintf("Launch %s daemon", CurrentConfig.NodeBinary), '1', startNode)
	list.AddItem("  Stop Node", "Shut down running daemon", '2', stopNode)
	list.AddItem("  Node Status", "Show height and peer count", '3', showNodeStatus)

	// --- Wallet ---
	list.AddItem("[::b]--- Wallet ---", "", 0, nil)
	list.AddItem("  Start Wallet RPC", fmt.Sprintf("Launch %s in RPC mode", CurrentConfig.WalletBinary), '4', startWalletRPC)
	list.AddItem("  Create New Wallet", "Generate a new wallet file", '5', createWallet)

	// --- Transfer ---
	list.AddItem("[::b]--- Transfer ---", "", 0, nil)
	list.AddItem("  Get Balance", "Query wallet balance", 'b', getBalance)
	list.AddItem("  Send Transaction", fmt.Sprintf("Send %s to an address", CurrentConfig.CoinName), 's', showSendTransactionForm)

	// --- Burns (HEAT) ---
	list.AddItem("[::b]--- XFG Burns (HEAT) ---", "", 0, nil)
	list.AddItem("  The Ethereal Mint", "Create an XFG burn for HEAT minting rights", 'h', showBurn2MintMenu)

	// --- COLD Deposits ---
	list.AddItem("[::b]--- XFG COLD Interest Banking ---", "", 0, nil)
	list.AddItem("  (coming soon)", "Certificates of Ledger Deposit", 0, nil)

	// --- Elderfier ---
	list.AddItem("[::b]--- Ξlderfiers ---", "", 0, nil)
	list.AddItem("  Ælder Kings Council", "Staking, consensus, ENindex", 'e', showElderfierMenu)

	// --- System ---
	list.AddItem("[::b]--- System ---", "", 0, nil)
	list.AddItem("  Show Logs", "View application log output", 'l', showLogs)
	list.AddItem("  Quit", "Exit the TUI", 'q', func() { appState.app.Stop() })

	// Status bar at bottom
	statusText := "[green]Ready[white]"
	if appState.isNodeRunning {
		statusText = "[green]Node: Running[white]"
	}
	if appState.isWalletRunning {
		statusText += " | [green]Wallet RPC: Running[white]"
	}
	statusBar := tview.NewTextView().
		SetDynamicColors(true).
		SetText(" " + statusText).
		SetBackgroundColor(tcell.ColorDarkSlateGray)

	// Layout
	mainLayout := tview.NewFlex().
		SetDirection(tview.FlexRow).
		AddItem(header, 1, 0, false).
		AddItem(list, 0, 1, true).
		AddItem(statusBar, 1, 0, false)

	// 'n' to toggle network
	mainLayout.SetInputCapture(func(event *tcell.EventKey) *tcell.EventKey {
		if event.Rune() == 'n' || event.Rune() == 'N' {
			if appState.network == "mainnet" {
				appState.network = "testnet"
				CurrentConfig = TestnetConfig
			} else {
				appState.network = "mainnet"
				CurrentConfig = MainnetConfig
			}
			appState.pages.RemovePage("main")
			createMainMenu()
			appState.pages.SwitchToPage("main")
			return nil
		}
		return event
	})

	appState.pages.AddPage("main", mainLayout, true, true)
}

// ============================================================================
// Node
// ============================================================================

func startNode() {
	if appState.isNodeRunning {
		showMessage("Node is already running")
		return
	}

	bp := findBinary(CurrentConfig.NodeBinary)
	if bp == "" {
		showMessage(CurrentConfig.NodeBinary + " not found")
		return
	}

	dataDir := filepath.Join(os.Getenv("HOME"), CurrentConfig.DataDir)
	os.MkdirAll(dataDir, 0755)

	args := []string{
		fmt.Sprintf("--p2p-bind-port=%d", CurrentConfig.NodeP2PPort),
		fmt.Sprintf("--rpc-bind-port=%d", CurrentConfig.NodeRPCPort),
		fmt.Sprintf("--data-dir=%s", dataDir),
	}
	if CurrentConfig.IsTestnet {
		args = append(args, "--testnet")
	}
	cmd := exec.Command(bp, args...)

	stdout, _ := cmd.StdoutPipe()
	stderr, _ := cmd.StderrPipe()

	if err := cmd.Start(); err != nil {
		appendLog("[ERROR] Failed to start node: " + err.Error())
		showMessage("Failed to start node: " + err.Error())
		return
	}

	appState.nodeCmd = cmd
	appState.isNodeRunning = true
	appendLog("[INFO] Started " + CurrentConfig.NodeBinary)
	showMessage("Node starting...")

	go streamPipe(stdout, "NODE")
	go streamPipe(stderr, "NODE-ERR")

	go func() {
		time.Sleep(3 * time.Second)
		for appState.isNodeRunning && appState.nodeCmd != nil {
			info, err := getNodeInfo()
			if err == nil {
				appState.app.QueueUpdateDraw(func() {
					appendLog(fmt.Sprintf("[NODE] Height: %d, Peers: %d", info.Height, info.Peers))
				})
			}
			time.Sleep(5 * time.Second)
		}
	}()
}

func stopNode() {
	if !appState.isNodeRunning {
		showMessage("Node is not running")
		return
	}
	if appState.nodeCmd != nil && appState.nodeCmd.Process != nil {
		appState.nodeCmd.Process.Kill()
	}
	appState.isNodeRunning = false
	showMessage("Node stopped")
}

func showNodeStatus() {
	if !appState.isNodeRunning {
		showMessage("Node is not running")
		return
	}
	info, err := getNodeInfo()
	if err != nil {
		showMessage("Error: " + err.Error())
		return
	}
	showMessage(fmt.Sprintf("Node Status\n\nHeight: %d\nPeers: %d", info.Height, info.Peers))
}

// ============================================================================
// Wallet
// ============================================================================

func startWalletRPC() {
	if appState.isWalletRunning {
		showMessage("Wallet RPC is already running")
		return
	}

	bp := findBinary(CurrentConfig.WalletBinary)
	if bp == "" {
		showMessage(CurrentConfig.WalletBinary + " not found")
		return
	}

	dataDir := filepath.Join(os.Getenv("HOME"), CurrentConfig.DataDir)
	os.MkdirAll(dataDir, 0755)
	walletFile := filepath.Join(dataDir, "wallet.wallet")

	if _, err := os.Stat(walletFile); os.IsNotExist(err) {
		showMessage("No wallet file found. Create a wallet first.")
		return
	}

	form := tview.NewForm()
	pw := tview.NewInputField().SetLabel("Wallet Password").SetFieldWidth(40).SetMaskCharacter('*')

	form.AddFormItem(pw).
		AddButton("Start", func() {
			password := pw.GetText()
			if password == "" {
				showMessage("Password required")
				return
			}

			args := []string{
				fmt.Sprintf("--rpc-bind-port=%d", CurrentConfig.WalletRPCPort),
				fmt.Sprintf("--wallet-file=%s", walletFile),
				fmt.Sprintf("--daemon-address=127.0.0.1:%d", CurrentConfig.NodeRPCPort),
				fmt.Sprintf("--password=%s", password),
			}
			if CurrentConfig.IsTestnet {
				args = append(args, "--testnet")
			}

			cmd := exec.Command(bp, args...)
			stdout, _ := cmd.StdoutPipe()
			stderr, _ := cmd.StderrPipe()

			if err := cmd.Start(); err != nil {
				showMessage("Failed: " + err.Error())
				return
			}

			appState.walletCmd = cmd
			appState.isWalletRunning = true
			appendLog("[INFO] Started " + CurrentConfig.WalletBinary + " RPC")
			showMessage("Wallet RPC starting...")

			go streamPipe(stdout, "WALLET")
			go streamPipe(stderr, "WALLET-ERR")
		}).
		AddButton("Cancel", func() { appState.pages.SwitchToPage("main") })

	form.SetBorder(true).SetTitle(" Start Wallet RPC ").SetTitleAlign(tview.AlignLeft)
	layout := tview.NewFlex().SetDirection(tview.FlexRow).AddItem(form, 0, 1, true)
	appState.pages.AddPage("walletPassword", layout, true, true)
	appState.pages.SwitchToPage("walletPassword")
}

func createWallet() {
	bp := findBinary(CurrentConfig.WalletBinary)
	if bp == "" {
		showMessage(CurrentConfig.WalletBinary + " not found")
		return
	}

	dataDir := filepath.Join(os.Getenv("HOME"), CurrentConfig.DataDir)
	os.MkdirAll(dataDir, 0755)
	walletFile := filepath.Join(dataDir, "wallet.wallet")

	if _, err := os.Stat(walletFile); err == nil {
		showMessage("Wallet already exists at:\n" + walletFile)
		return
	}

	form := tview.NewForm()
	pw := tview.NewInputField().SetLabel("New Password").SetFieldWidth(40).SetMaskCharacter('*')
	confirm := tview.NewInputField().SetLabel("Confirm Password").SetFieldWidth(40).SetMaskCharacter('*')

	form.AddFormItem(pw).AddFormItem(confirm).
		AddButton("Create", func() {
			if pw.GetText() == "" {
				showMessage("Password required")
				return
			}
			if pw.GetText() != confirm.GetText() {
				showMessage("Passwords do not match")
				return
			}

			showMessage("Creating wallet...")
			go func() {
				args := []string{
					fmt.Sprintf("--generate-new-wallet=%s", walletFile),
					fmt.Sprintf("--password=%s", pw.GetText()),
				}
				if CurrentConfig.IsTestnet {
					args = append(args, "--testnet")
				}
				cmd := exec.Command(bp, args...)
				output, err := cmd.CombinedOutput()
				appState.app.QueueUpdateDraw(func() {
					if err != nil {
						appendLog("[WALLET] Error: " + err.Error() + "\n" + string(output))
						showMessage("Error creating wallet. Check logs.")
					} else {
						appendLog("[WALLET] Created successfully")
						showMessage("Wallet created!\n" + walletFile)
					}
				})
			}()
		}).
		AddButton("Cancel", func() { appState.pages.SwitchToPage("main") })

	form.SetBorder(true).SetTitle(" Create New Wallet ").SetTitleAlign(tview.AlignLeft)
	layout := tview.NewFlex().SetDirection(tview.FlexRow).AddItem(form, 0, 1, true)
	appState.pages.AddPage("createWallet", layout, true, true)
	appState.pages.SwitchToPage("createWallet")
}

// ============================================================================
// Transfer
// ============================================================================

func getBalance() {
	if !appState.isWalletRunning {
		showMessage("Wallet RPC not running")
		return
	}
	result, err := walletRpcCall(CurrentConfig.WalletRPCPort, CurrentConfig.GetBalanceRPC, nil)
	if err != nil {
		showMessage("Error: " + err.Error())
		return
	}
	balance, ok := result["balance"]
	if !ok {
		showMessage("Unexpected response")
		return
	}
	balanceInt, err := strconv.ParseInt(fmt.Sprintf("%.0f", balance), 10, 64)
	if err != nil {
		showMessage("Parse error: " + err.Error())
		return
	}
	balVal := float64(balanceInt) / float64(CurrentConfig.CoinUnits)
	showMessage(fmt.Sprintf("Balance: %.7f %s", balVal, CurrentConfig.CoinName))
}

func showSendTransactionForm() {
	if !appState.isWalletRunning {
		showMessage("Wallet RPC not running")
		return
	}

	form := tview.NewForm()
	addr := tview.NewInputField().SetLabel("Recipient Address").SetFieldWidth(100)
	amt := tview.NewInputField().SetLabel("Amount (" + CurrentConfig.CoinName + ")").SetFieldWidth(20)

	form.AddFormItem(addr).AddFormItem(amt).
		AddButton("Send", func() {
			if addr.GetText() == "" || amt.GetText() == "" {
				showMessage("Fill all fields")
				return
			}
			amount, err := strconv.ParseFloat(amt.GetText(), 64)
			if err != nil {
				showMessage("Invalid amount")
				return
			}
			amountAtomic := int64(amount * float64(CurrentConfig.CoinUnits))
			params := map[string]interface{}{
				"transfers": []map[string]interface{}{
					{"address": addr.GetText(), "amount": amountAtomic},
				},
			}
			result, err := walletRpcCall(CurrentConfig.WalletRPCPort, CurrentConfig.SendTransactionRPC, params)
			if err != nil {
				showMessage("Error: " + err.Error())
				return
			}
			txHash, _ := result["tx_hash"]
			showMessage(fmt.Sprintf("Sent!\nTX: %v", txHash))
		}).
		AddButton("Cancel", func() { appState.pages.SwitchToPage("main") })

	form.SetBorder(true).SetTitle(" Send " + CurrentConfig.CoinName + " ").SetTitleAlign(tview.AlignLeft)
	layout := tview.NewFlex().SetDirection(tview.FlexRow).AddItem(form, 0, 1, true)
	appState.pages.AddPage("sendTx", layout, true, true)
	appState.pages.SwitchToPage("sendTx")
}

// ============================================================================
// Burns (HEAT) / Ethereal Mint
// ============================================================================

func showBurn2MintMenu() {
	if !appState.isWalletRunning {
		showMessage("Wallet RPC not running")
		return
	}

	list := tview.NewList().
		SetMainTextColor(tcell.ColorOrange).
		SetSelectedTextColor(tcell.ColorBlack).
		SetSelectedBackgroundColor(tcell.ColorOrange)

	labels := []string{"0.8", "8", "80", "800"}
	for i, tier := range CurrentConfig.BurnTiers {
		t := tier // capture
		label := labels[i]
		list.AddItem(
			fmt.Sprintf("  Burn %s %s", label, CurrentConfig.CoinName),
			"HEAT deposit (permanent)", 0,
			func() { startBurnProcess(t) })
	}
	list.AddItem("  Back", "", 0, func() { appState.pages.SwitchToPage("main") })

	title := tview.NewTextView().SetText("The Ethereal Mint - HEAT Burns").
		SetTextColor(tcell.ColorOrange).SetTextAlign(tview.AlignCenter)

	layout := tview.NewFlex().SetDirection(tview.FlexRow).
		AddItem(title, 1, 0, false).
		AddItem(list, 0, 1, true)
	appState.pages.AddPage("burn2mint", layout, true, true)
	appState.pages.SwitchToPage("burn2mint")
}

func startBurnProcess(amount int64) {
	amtStr := fmt.Sprintf("%.7f", float64(amount)/float64(CurrentConfig.CoinUnits))
	msg := fmt.Sprintf("Burn %s %s permanently?\nThis cannot be undone.", amtStr, CurrentConfig.CoinName)

	modal := tview.NewModal().SetText(msg).
		AddButtons([]string{"Confirm", "Cancel"}).
		SetDoneFunc(func(_ int, label string) {
			if label == "Confirm" {
				performBurn(amount)
			} else {
				appState.pages.SwitchToPage("burn2mint")
			}
		})
	appState.pages.AddPage("burnConfirm", modal, true, true)
	appState.pages.SwitchToPage("burnConfirm")
}

func performBurn(amount int64) {
	params := map[string]interface{}{"amount": amount}
	result, err := walletRpcCall(CurrentConfig.WalletRPCPort, CurrentConfig.CreateBurnRPC, params)
	if err != nil {
		showMessage("Burn error: " + err.Error())
		return
	}
	txHash, _ := result["tx_hash"]
	showMessage(fmt.Sprintf("Burn TX Created\nHash: %v", txHash))
}

// ============================================================================
// Elderfier
// ============================================================================

func showElderfierMenu() {
	if !appState.isWalletRunning {
		showMessage("Wallet RPC not running")
		return
	}

	result, err := walletRpcCall(CurrentConfig.WalletRPCPort, CurrentConfig.GetStakeStatusRPC, nil)
	hasStake := err == nil && result != nil

	list := tview.NewList().
		SetMainTextColor(tcell.ColorOrange).
		SetSelectedTextColor(tcell.ColorBlack).
		SetSelectedBackgroundColor(tcell.ColorOrange)

	if hasStake {
		list.AddItem("  View Consensus Requests", "", 0, viewConsensusRequests)
		list.AddItem("  Vote on Pending Items", "", 0, voteOnPendingItems)
		list.AddItem("  Manage Stake", "", 0, func() { showMessage("Managing stake...") })
		list.AddItem("  Update ENindex Keys", "", 0, func() { showMessage("Updating ENindex keys...") })
	} else {
		list.AddItem("  Start Elderfyre Stayking", "", 0, startElderfyreStayking)
		list.AddItem("  Check Stake Status", "", 0, checkStakeStatus)
	}
	list.AddItem("  Back", "", 0, func() { appState.pages.SwitchToPage("main") })

	title := tview.NewTextView().SetText("Ælder Kings Council").
		SetTextColor(tcell.ColorOrange).SetTextAlign(tview.AlignCenter)
	layout := tview.NewFlex().SetDirection(tview.FlexRow).
		AddItem(title, 1, 0, false).
		AddItem(list, 0, 1, true)
	appState.pages.AddPage("elderfier", layout, true, true)
	appState.pages.SwitchToPage("elderfier")
}

func startElderfyreStayking() {
	form := tview.NewForm()
	stakeAmt := tview.NewInputField().SetLabel("Stake Amount (" + CurrentConfig.CoinName + ")").SetFieldWidth(20).SetText("800")
	efID := tview.NewInputField().SetLabel("Ξlderfier ID (8 chars)").SetFieldWidth(20)

	form.AddFormItem(stakeAmt).AddFormItem(efID).
		AddButton("Create Stake", func() {
			amount, err := strconv.ParseFloat(stakeAmt.GetText(), 64)
			if err != nil {
				showMessage("Invalid amount")
				return
			}
			if len(efID.GetText()) != 8 {
				showMessage("ID must be exactly 8 characters")
				return
			}
			amountAtomic := int64(amount * float64(CurrentConfig.CoinUnits))
			params := map[string]interface{}{"amount": amountAtomic}
			result, err := walletRpcCall(CurrentConfig.WalletRPCPort, CurrentConfig.CreateStakeRPC, params)
			if err != nil {
				showMessage("Error: " + err.Error())
				return
			}
			txHash, _ := result["tx_hash"]
			showMessage(fmt.Sprintf("Stake Created!\nTX: %v\n\n1. Wait 10 confirmations\n2. Register Elderfier ID", txHash))
		}).
		AddButton("Cancel", func() { appState.pages.SwitchToPage("elderfier") })

	form.SetBorder(true).SetTitle(" Elderfyre Stayking ").SetTitleAlign(tview.AlignLeft)
	layout := tview.NewFlex().SetDirection(tview.FlexRow).AddItem(form, 0, 1, true)
	appState.pages.AddPage("stayking", layout, true, true)
	appState.pages.SwitchToPage("stayking")
}

func checkStakeStatus() {
	result, err := walletRpcCall(CurrentConfig.WalletRPCPort, CurrentConfig.GetStakeStatusRPC, nil)
	if err != nil {
		showMessage("Error: " + err.Error())
		return
	}
	showMessage(fmt.Sprintf("Stake Status:\n%v", result))
}

func viewConsensusRequests() {
	result, err := walletRpcCall(CurrentConfig.WalletRPCPort, CurrentConfig.GetConsensusRPC, nil)
	if err != nil {
		showMessage("Error: " + err.Error())
		return
	}
	showMessage(fmt.Sprintf("Consensus Requests:\n%v", result))
}

func voteOnPendingItems() {
	result, err := walletRpcCall(CurrentConfig.WalletRPCPort, CurrentConfig.GetPendingVotesRPC, nil)
	if err != nil {
		showMessage("Error: " + err.Error())
		return
	}
	showMessage(fmt.Sprintf("Pending Votes:\n%v", result))
}

// ============================================================================
// Logs
// ============================================================================

func showLogs() {
	logText := strings.Join(appState.logs, "\n")
	if logText == "" {
		logText = "No logs yet."
	}

	tv := tview.NewTextView().SetText(logText).SetScrollable(true).SetWrap(true)
	tv.SetBorder(true).SetTitle(" Logs ").SetTitleAlign(tview.AlignLeft)

	back := tview.NewButton("Back").SetSelectedFunc(func() {
		appState.pages.SwitchToPage("main")
	})

	layout := tview.NewFlex().SetDirection(tview.FlexRow).
		AddItem(tv, 0, 1, true).
		AddItem(back, 1, 0, false)

	layout.SetInputCapture(func(event *tcell.EventKey) *tcell.EventKey {
		if event.Key() == tcell.KeyEsc {
			appState.pages.SwitchToPage("main")
			return nil
		}
		return event
	})

	appState.pages.AddPage("logs", layout, true, true)
	appState.pages.SwitchToPage("logs")
}

// ============================================================================
// UI Helpers
// ============================================================================

func showMessage(message string) {
	modal := tview.NewModal().SetText(message).
		AddButtons([]string{"OK"}).
		SetDoneFunc(func(_ int, _ string) {
			appState.pages.SwitchToPage("main")
		})
	appState.pages.AddPage("message", modal, true, true)
	appState.pages.SwitchToPage("message")
}

func appendLog(msg string) {
	appState.logs = append(appState.logs, msg)
	if len(appState.logs) > 1000 {
		appState.logs = appState.logs[len(appState.logs)-1000:]
	}
}

// ============================================================================
// I/O Helpers
// ============================================================================

func streamPipe(r io.Reader, prefix string) {
	scanner := bufio.NewScanner(r)
	for scanner.Scan() {
		line := scanner.Text()
		if line != "" {
			appState.app.QueueUpdateDraw(func() {
				appendLog(fmt.Sprintf("[%s] %s", prefix, line))
			})
		}
	}
}

func findBinary(name string) string {
	paths := []string{
		filepath.Join("..", "build", "src", name),
		filepath.Join("..", "build", "release", "src", name),
		filepath.Join("..", "build-test", "src", name),
		filepath.Join("..", "bulid3", "release", "src", name),
		filepath.Join("/home/ar/fuego", "build", "release", "src", name),
		filepath.Join("/home/ar/fuego", "build", "src", name),
		filepath.Join("/home/ar/fuego", "build-test", "src", name),
		filepath.Join("/home/ar/fuego", "bulid3", "release", "src", name),
	}
	for _, p := range paths {
		if _, err := os.Stat(p); err == nil {
			appendLog(fmt.Sprintf("[DEBUG] Found %s at %s", name, p))
			return p
		}
	}
	if p, err := exec.LookPath(name); err == nil {
		appendLog(fmt.Sprintf("[DEBUG] Found %s in PATH: %s", name, p))
		return p
	}
	appendLog(fmt.Sprintf("[ERROR] Binary not found: %s", name))
	return ""
}

// ============================================================================
// RPC
// ============================================================================

func getNodeInfo() (*NodeInfo, error) {
	url := fmt.Sprintf("http://127.0.0.1:%d/get_info", CurrentConfig.NodeRPCPort)
	client := &http.Client{Timeout: 5 * time.Second}
	resp, err := client.Get(url)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("HTTP %d", resp.StatusCode)
	}

	var data map[string]interface{}
	if err := json.NewDecoder(resp.Body).Decode(&data); err != nil {
		return nil, err
	}

	info := &NodeInfo{}
	if h, ok := data["height"]; ok {
		if v, ok := h.(float64); ok {
			info.Height = int(v)
		}
	}
	peers := 0
	if ic, ok := data["incoming_connections_count"]; ok {
		if v, ok := ic.(float64); ok {
			peers += int(v)
		}
	}
	if oc, ok := data["outgoing_connections_count"]; ok {
		if v, ok := oc.(float64); ok {
			peers += int(v)
		}
	}
	info.Peers = peers
	return info, nil
}

func walletRpcCall(port int, method string, params interface{}) (map[string]interface{}, error) {
	url := fmt.Sprintf("http://127.0.0.1:%d/json_rpc", port)
	client := &http.Client{Timeout: 5 * time.Second}

	request := map[string]interface{}{
		"jsonrpc": "2.0",
		"id":      "tui",
		"method":  method,
	}
	if params != nil {
		request["params"] = params
	}

	jsonData, err := json.Marshal(request)
	if err != nil {
		return nil, err
	}

	resp, err := client.Post(url, "application/json", strings.NewReader(string(jsonData)))
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("HTTP %d", resp.StatusCode)
	}

	var rpcResp RPCResponse
	if err := json.NewDecoder(resp.Body).Decode(&rpcResp); err != nil {
		return nil, err
	}
	if rpcResp.Error != nil {
		return nil, fmt.Errorf("RPC error: %s", rpcResp.Error.Message)
	}
	result, ok := rpcResp.Result.(map[string]interface{})
	if !ok {
		return nil, fmt.Errorf("unexpected result format")
	}
	return result, nil
}
