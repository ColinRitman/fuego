// swapxfg/app/rpc.go
package app

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"sync"
	"time"
)

// FuegoClient talks to a fuegod node via JSON-RPC.
type FuegoClient struct {
	endpoint string
	client   *http.Client
}

func NewFuegoClient(endpoint string) *FuegoClient {
	return &FuegoClient{
		endpoint: endpoint,
		client:   &http.Client{Timeout: 10 * time.Second},
	}
}

// --- RPC response types (mirror CoreRpcServerCommandsDefinitions.h) ---

type SwapOffer struct {
	OfferID      string `json:"offerId"`
	IsSell       bool   `json:"isSell"`
	XfgAmount    uint64 `json:"xfgAmount"`
	RateNum      uint64 `json:"rateNum"`
	Pair         uint8  `json:"pair"`
	MakerPubKey  string `json:"makerPubKey"`
	Timestamp    uint64 `json:"timestamp"`
	TTLBlocks    uint32 `json:"ttlBlocks"`
	PostedHeight uint32 `json:"postedHeight"`
}

type SubmitSwapOfferRequest struct {
	OfferID     string `json:"offerId"`
	IsSell      bool   `json:"isSell"`
	XfgAmount   uint64 `json:"xfgAmount"`
	RateNum     uint64 `json:"rateNum"`
	Pair        uint8  `json:"pair"`
	MakerPubKey string `json:"makerPubKey"`
	Signature   string `json:"signature"`
	TTLBlocks   uint32 `json:"ttlBlocks"`
}

type SwapTrade struct {
	Pair      uint8  `json:"pair"`
	XfgAmount uint64 `json:"xfgAmount"`
	CtrAmount uint64 `json:"ctrAmount"`
	Rate      string `json:"rate"`
	Height    uint32 `json:"blockHeight"`
	Timestamp uint64 `json:"timestamp"`
}

type PriceSourceEntry struct {
	Name      string `json:"name"`
	Pair      uint8  `json:"pair"`
	Weight    string `json:"weight"`
	Rate      string `json:"rate"`
	UpdatedAt uint64 `json:"updatedAt"`
	Stale     bool   `json:"stale"`
}

type PairImplied struct {
	Pair       uint8  `json:"pair"`
	ImpliedUsd string `json:"impliedUsd"`
}

type SwapPriceResponse struct {
	Twap          string             `json:"twap"`
	SeedRate      string             `json:"seedRate"`
	CompositeRate string             `json:"compositeRate"`
	SourceCount   uint32             `json:"sourceCount"`
	Sources       []PriceSourceEntry `json:"sources"`
	XfgUsdLow     string            `json:"xfgUsdLow"`
	XfgUsdHigh    string            `json:"xfgUsdHigh"`
	XfgUsdMid     string            `json:"xfgUsdMid"`
	PairImplied   []PairImplied     `json:"pairImplied"`
	Status        string            `json:"status"`
}

// SwapStatus represents the state of an active adaptor-signature swap.
type SwapStatus struct {
	SwapID    string `json:"swapId"`
	Pair      uint8  `json:"pair"`
	Role      string `json:"role"`      // "ALICE" or "BOB"
	State     string `json:"state"`     // INITIATED, KEYS_EXCHANGED, ESCROW_FUNDED, PRESIGS_READY, CTR_LOCKED, SECRET_REVEALED, XFG_SPENT, REFUNDED, FAILED
	XfgAmount uint64 `json:"xfgAmount"`
	CtrAmount uint64 `json:"ctrAmount"`
	Rate      string `json:"rate"`

	// Adaptor sig fields
	EscrowKey    string `json:"escrowKey"`    // Musig2 aggregated public key
	AdaptorPoint string `json:"adaptorPoint"` // T = t*G
	OurSwapKey   string `json:"ourSwapKey"`
	PeerSwapKey  string `json:"peerSwapKey"`

	// Chain state
	EscrowTxHash string `json:"escrowTxHash"`
	CtrLockTxId  string `json:"ctrLockTxId"`
	PeerEndpoint string `json:"peerEndpoint"`

	// Timestamps
	CreatedAt uint64 `json:"createdAt"`
	UpdatedAt uint64 `json:"updatedAt"`
}

type NodeInfo struct {
	Height        uint64 `json:"height"`
	Difficulty    uint64 `json:"difficulty"`
	TxCount       uint64 `json:"tx_count"`
	TxPoolSize    uint64 `json:"tx_pool_size"`
	IncomingConns uint64 `json:"incoming_connections_count"`
	OutgoingConns uint64 `json:"outgoing_connections_count"`
	Connections   uint64 `json:"-"` // computed: IncomingConns + OutgoingConns
	Version       string `json:"version"`
	Status        string `json:"status"`
}

type FeePoolInfo struct {
	FeePoolBalance       uint64 `json:"fee_pool_balance"`
	CurrentEpochSwapFees uint64 `json:"current_epoch_swap_fees"`
	TotalCdLocked        uint64 `json:"total_cd_locked"`
	CurrentEpochFeeRate  uint64 `json:"current_epoch_fee_rate"`
	ActiveEfierCount     uint32 `json:"active_efier_count"`
	BankingFeeRateBps    uint64 `json:"banking_fee_rate_bps"`
	EfierSwapRewardPerBlock uint64 `json:"efier_swap_reward_per_block"`
	TreasuryBalance      uint64 `json:"treasury_balance"`
	TotalSwapFeesCollected uint64 `json:"total_swap_fees_collected"`
	TotalCdInterestPaid  uint64 `json:"total_cd_interest_paid"`
	Status               string `json:"status"`
}

// --- Per-pair fetch methods ---

func (c *FuegoClient) GetOffers(pair uint8) ([]SwapOffer, error) {
	req := map[string]interface{}{"pair": pair}
	var resp struct {
		Offers []SwapOffer `json:"offers"`
		Status string      `json:"status"`
	}
	if err := c.post("/getswapoffers", req, &resp); err != nil {
		return nil, err
	}
	return resp.Offers, nil
}

func (c *FuegoClient) GetPrice(pair uint8) (*SwapPriceResponse, error) {
	req := map[string]interface{}{"pair": pair}
	var resp SwapPriceResponse
	if err := c.post("/getswapprice", req, &resp); err != nil {
		return nil, err
	}
	return &resp, nil
}

func (c *FuegoClient) GetTrades(pair uint8, limit int) ([]SwapTrade, error) {
	req := map[string]interface{}{"pair": pair, "limit": limit}
	var resp struct {
		Trades []SwapTrade `json:"trades"`
		Status string      `json:"status"`
	}
	if err := c.post("/getswaptrades", req, &resp); err != nil {
		return nil, err
	}
	return resp.Trades, nil
}

func (c *FuegoClient) GetSwapStatus(swapId string) (*SwapStatus, error) {
	req := map[string]interface{}{"swapId": swapId}
	var resp SwapStatus
	if err := c.post("/getswapstatus", req, &resp); err != nil {
		return nil, err
	}
	return &resp, nil
}

func (c *FuegoClient) GetActiveSwaps() ([]SwapStatus, error) {
	var resp struct {
		Swaps  []SwapStatus `json:"swaps"`
		Status string       `json:"status"`
	}
	if err := c.post("/getactiveswaps", nil, &resp); err != nil {
		return nil, err
	}
	return resp.Swaps, nil
}

func (c *FuegoClient) GetInfo() (*NodeInfo, error) {
	var resp NodeInfo
	if err := c.post("/getinfo", nil, &resp); err != nil {
		return nil, err
	}
	resp.Connections = resp.IncomingConns + resp.OutgoingConns
	return &resp, nil
}

func (c *FuegoClient) GetFeePool() (*FeePoolInfo, error) {
	var resp FeePoolInfo
	if err := c.post("/getfeepool", nil, &resp); err != nil {
		return nil, err
	}
	return &resp, nil
}

func (c *FuegoClient) SubmitSwapOffer(req SubmitSwapOfferRequest) error {
	var resp struct {
		Status string `json:"status"`
	}
	if err := c.post("/submitswap", req, &resp); err != nil {
		return err
	}
	if resp.Status != "OK" {
		return fmt.Errorf("daemon: %s", resp.Status)
	}
	return nil
}

func (c *FuegoClient) CancelSwapOffer(offerID, makerPubKey, signature string) error {
	req := map[string]interface{}{
		"offerId":     offerID,
		"makerPubKey": makerPubKey,
		"signature":   signature,
	}
	var resp struct {
		Status string `json:"status"`
	}
	if err := c.post("/cancelswap", req, &resp); err != nil {
		return err
	}
	if resp.Status != "OK" {
		return fmt.Errorf("daemon: %s", resp.Status)
	}
	return nil
}

// --- Multi-pair parallel fetch ---

// AllPairData holds fetched data for all active pairs.
type AllPairData struct {
	Offers  map[uint8][]SwapOffer
	Prices  map[uint8]*SwapPriceResponse
	Trades  map[uint8][]SwapTrade
	Height  uint64
	FeePool *FeePoolInfo
}

// FetchAll fetches offers, prices, and trades for the given pairs in parallel.
func (c *FuegoClient) FetchAll(pairs []uint8) (*AllPairData, error) {
	data := &AllPairData{
		Offers: make(map[uint8][]SwapOffer),
		Prices: make(map[uint8]*SwapPriceResponse),
		Trades: make(map[uint8][]SwapTrade),
	}

	var mu sync.Mutex
	var wg sync.WaitGroup
	var firstErr error

	// Fetch info (block height) + fee pool in parallel
	wg.Add(2)
	go func() {
		defer wg.Done()
		info, err := c.GetInfo()
		if err != nil {
			mu.Lock()
			if firstErr == nil {
				firstErr = err
			}
			mu.Unlock()
			return
		}
		mu.Lock()
		data.Height = info.Height
		mu.Unlock()
	}()
	go func() {
		defer wg.Done()
		fp, err := c.GetFeePool()
		if err != nil {
			// Non-fatal: fee pool display is optional
			return
		}
		mu.Lock()
		data.FeePool = fp
		mu.Unlock()
	}()

	// Fetch offers + prices + trades per pair
	for _, pair := range pairs {
		p := pair
		wg.Add(3)
		go func() {
			defer wg.Done()
			offers, err := c.GetOffers(p)
			mu.Lock()
			if err == nil {
				data.Offers[p] = offers
			} else if firstErr == nil {
				firstErr = err
			}
			mu.Unlock()
		}()
		go func() {
			defer wg.Done()
			price, err := c.GetPrice(p)
			mu.Lock()
			if err == nil {
				data.Prices[p] = price
			} else if firstErr == nil {
				firstErr = err
			}
			mu.Unlock()
		}()
		go func() {
			defer wg.Done()
			trades, err := c.GetTrades(p, 50)
			mu.Lock()
			if err == nil {
				data.Trades[p] = trades
			} else if firstErr == nil {
				firstErr = err
			}
			mu.Unlock()
		}()
	}

	wg.Wait()
	return data, firstErr
}

// --- HTTP helper ---

func (c *FuegoClient) post(path string, reqBody interface{}, result interface{}) error {
	var body io.Reader
	if reqBody != nil {
		data, err := json.Marshal(reqBody)
		if err != nil {
			return fmt.Errorf("marshal: %w", err)
		}
		body = bytes.NewReader(data)
	}

	resp, err := c.client.Post(c.endpoint+path, "application/json", body)
	if err != nil {
		return fmt.Errorf("request %s: %w", path, err)
	}
	defer resp.Body.Close()

	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return fmt.Errorf("read %s: %w", path, err)
	}

	if err := json.Unmarshal(raw, result); err != nil {
		return fmt.Errorf("decode %s: %w", path, err)
	}
	return nil
}
