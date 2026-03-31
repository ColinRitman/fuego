// swapxfg/app/wallet_rpc.go
package app

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"time"
)

type WalletClient struct {
	endpoint string
	client   *http.Client
}

func NewWalletClient(endpoint string) *WalletClient {
	return &WalletClient{
		endpoint: endpoint,
		client:   &http.Client{Timeout: 15 * time.Second},
	}
}

type InitiateSwapRequest struct {
	OfferID     string `json:"offer_id"`
	Pair        uint8  `json:"pair"`
	Amount      uint64 `json:"amount"`
	MakerPubKey string `json:"maker_pubkey"`
}

type InitiateSwapResponse struct {
	JointAddress string `json:"joint_address"`
	AdaptorPoint string `json:"adaptor_point"`
	DleqProof    string `json:"dleq_proof"`
	Status       string `json:"status"`
}

type SignOfferRequest struct {
	XfgAmount uint64 `json:"xfg_amount"`
	RateNum   uint64 `json:"rate_num"`
	Pair      uint8  `json:"pair"`
	TTLBlocks uint32 `json:"ttl_blocks"`
}

type SignOfferResponse struct {
	OfferID     string `json:"offer_id"`
	MakerPubKey string `json:"maker_pubkey"`
	Signature   string `json:"signature"`
	Timestamp   uint64 `json:"timestamp"`
	Status      string `json:"status"`
}

func (c *WalletClient) GetAddress() (string, error) {
	var resp struct {
		Address string `json:"address"`
	}
	if err := c.post("/getaddress", nil, &resp); err != nil {
		return "", err
	}
	return resp.Address, nil
}

func (c *WalletClient) GetBalance() (uint64, uint64, error) {
	var resp struct {
		Available uint64 `json:"available_balance"`
		Locked    uint64 `json:"locked_amount"`
	}
	if err := c.post("/getbalance", nil, &resp); err != nil {
		return 0, 0, err
	}
	return resp.Available, resp.Locked, nil
}

func (c *WalletClient) SignOffer(req SignOfferRequest) (*SignOfferResponse, error) {
	var resp SignOfferResponse
	if err := c.post("/sign_offer", req, &resp); err != nil {
		return nil, err
	}
	if resp.Status != "OK" {
		return nil, fmt.Errorf("wallet: %s", resp.Status)
	}
	return &resp, nil
}

func (c *WalletClient) InitiateSwap(req InitiateSwapRequest) (*InitiateSwapResponse, error) {
	var resp InitiateSwapResponse
	if err := c.post("/initiate_swap", req, &resp); err != nil {
		return nil, err
	}
	if resp.Status != "OK" {
		return nil, fmt.Errorf("wallet: %s", resp.Status)
	}
	return &resp, nil
}

func (c *WalletClient) post(path string, reqBody interface{}, result interface{}) error {
	var body io.Reader
	if reqBody != nil {
		data, err := json.Marshal(reqBody)
		if err != nil {
			return err
		}
		body = bytes.NewReader(data)
	}

	resp, err := c.client.Post(c.endpoint+path, "application/json", body)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	return json.NewDecoder(resp.Body).Decode(result)
}
