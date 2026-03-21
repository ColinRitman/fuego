// Copyright (c) 2017-2026 Fuego Developers
//
// This file is part of Fuego.
//
// Fuego is free software distributed in the hope that it
// will be useful, but WITHOUT ANY WARRANTY; without even the
// implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. You can redistribute it and/or modify it under the terms
// of the GNU General Public License v3 or later versions as published
// by the Free Software Foundation. Fuego includes elements written
// by third parties. See file labeled LICENSE for more details.
// You should have received a copy of the GNU General Public License
// along with Fuego. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <ctime>
#include <functional>

namespace CryptoNote {

// A swap offer on the order book
struct SwapOffer {
  std::string offerId;
  bool        isSell;       // true = selling XFG for CTR, false = buying XFG
  uint64_t    xfgAmount;    // atomic units
  double      rate;          // XFG per 1 whole CTR coin
  std::string pair;          // "ETH", "BCH", "XMR"
  std::string makerAlias;    // @ alias or truncated pubkey
  time_t      timestamp;
  bool        isMine;        // true if this wallet posted it
};

// OHLCV candle for chart rendering
struct SwapCandle {
  time_t   timestamp;
  double   open;
  double   high;
  double   low;
  double   close;
  double   volume;   // XFG volume
};

// Completed trade for the tape
struct SwapTrade {
  double   rate;
  double   volume;
  bool     isBuy;    // taker was buying XFG
  time_t   timestamp;
};

// Callback to fetch data from the RPC node
struct SwapTerminalCallbacks {
  // Fetch current offers from fuegod /getoffers
  std::function<std::vector<SwapOffer>(const std::string& pair)> fetchOffers;
  // Fetch completed trades from fuegod /getswaptrades
  std::function<std::vector<SwapTrade>(const std::string& pair, size_t limit)> fetchTrades;
  // Fetch TWAP from fuegod /getswapprice
  std::function<double(const std::string& pair)> fetchTwap;
  // Submit a swap offer (XFG → CTR)
  std::function<bool(uint64_t xfgAmount, double rate, const std::string& pair)> submitOffer;
  // Accept an offer
  std::function<bool(const std::string& offerId)> acceptOffer;
  // Cancel own offer
  std::function<bool(const std::string& offerId)> cancelOffer;
  // Get wallet balance
  std::function<uint64_t()> getBalance;
};

class SwapTerminal {
public:
  SwapTerminal();
  ~SwapTerminal();

  // Set callbacks for RPC interaction
  void setCallbacks(const SwapTerminalCallbacks& cb);

  // Run the TUI (blocks until user exits with 'q' or ESC)
  // Returns when user exits the swap terminal
  void run();

private:
  SwapTerminalCallbacks m_cb;
  std::string m_activePair;   // "ETH", "BCH", "XMR"
  std::atomic<bool> m_running;

  // Cached data
  mutable std::mutex m_dataMutex;
  std::vector<SwapOffer> m_asks;   // sorted by rate ascending (cheapest first)
  std::vector<SwapOffer> m_bids;   // sorted by rate descending (best bid first)
  std::deque<SwapCandle> m_candles;
  std::deque<SwapTrade>  m_trades;
  double m_twap;
  double m_lastPrice;
  double m_change24h;
  uint64_t m_balance;

  // ncurses windows
  void* m_winChart;      // WINDOW* — use void* to avoid ncurses.h in header
  void* m_winBook;
  void* m_winStatus;
  void* m_winInput;
  void* m_winTrades;
  int m_termRows;
  int m_termCols;

  // Layout + rendering
  void initScreen();
  void destroyScreen();
  void resizeWindows();
  void drawChrome();
  void drawChart();
  void drawOrderBook();
  void drawTradeHistory();
  void drawStatusBar();
  void drawInputBar();
  void refreshAll();

  // Chart helpers
  void buildCandles();
  void drawCandlestick(int col, int topRow, int bottomRow, const SwapCandle& candle,
                       double priceMin, double priceMax);

  // Order book helpers
  void sortOrderBook();

  // Input handling
  void handleInput(int ch);
  void processCommand(const std::string& cmd);
  void switchPair(const std::string& pair);

  // Data refresh
  void refreshData();

  // Smart order recommendation: favors XFG holders
  // Sell XFG: recommends slightly above TWAP (get more CTR per XFG)
  // Buy XFG: recommends slightly below TWAP (pay less CTR per XFG)
  double getRecommendedRate(bool isSell) const;
  void showRecommendation(bool isSell);

  // Cross-pair arbitrage detection
  void checkArbitrageOpportunities();
  std::string m_arbAlert;   // non-empty if arb opportunity exists

  // Formatting
  static std::string formatRate(double rate);
  static std::string formatXfg(uint64_t atomic);
  static std::string formatAge(time_t ts);

  // Input buffer
  std::string m_inputBuf;
  std::string m_statusMsg;
  time_t m_statusTime;
};

} // namespace CryptoNote
