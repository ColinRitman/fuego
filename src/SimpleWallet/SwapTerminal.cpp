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

#include "SwapTerminal.h"
#include "CryptoNoteConfig.h"
#include <ncurses.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace CryptoNote {

// ============================================================================
// Color pairs
// ============================================================================
enum ColorPairs {
  CP_DEFAULT = 0,
  CP_GREEN   = 1,
  CP_RED     = 2,
  CP_YELLOW  = 3,
  CP_CYAN    = 4,
  CP_HEADER  = 5,
  CP_INPUT   = 6,
  CP_STATUS  = 7,
  CP_CANDLE_UP   = 8,
  CP_CANDLE_DOWN = 9,
  CP_BORDER  = 10,
};

// ============================================================================
// Constructor / Destructor
// ============================================================================

SwapTerminal::SwapTerminal()
  : m_activePair("ETH")
  , m_running(false)
  , m_twap(0.0)
  , m_lastPrice(0.0)
  , m_change24h(0.0)
  , m_balance(0)
  , m_winChart(nullptr)
  , m_winBook(nullptr)
  , m_winStatus(nullptr)
  , m_winInput(nullptr)
  , m_winTrades(nullptr)
  , m_termRows(0)
  , m_termCols(0)
  , m_statusTime(0) {
}

SwapTerminal::~SwapTerminal() {
  if (m_running) {
    destroyScreen();
  }
}

void SwapTerminal::setCallbacks(const SwapTerminalCallbacks& cb) {
  m_cb = cb;
}

// ============================================================================
// Screen init / destroy
// ============================================================================

void SwapTerminal::initScreen() {
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  nodelay(stdscr, TRUE);  // non-blocking getch
  curs_set(0);            // hide cursor

  if (has_colors()) {
    start_color();
    use_default_colors();
    init_pair(CP_GREEN,      COLOR_GREEN,   -1);
    init_pair(CP_RED,        COLOR_RED,     -1);
    init_pair(CP_YELLOW,     COLOR_YELLOW,  -1);
    init_pair(CP_CYAN,       COLOR_CYAN,    -1);
    init_pair(CP_HEADER,     COLOR_BLACK,   COLOR_CYAN);
    init_pair(CP_INPUT,      COLOR_WHITE,   COLOR_BLACK);
    init_pair(CP_STATUS,     COLOR_BLACK,   COLOR_GREEN);
    init_pair(CP_CANDLE_UP,  COLOR_GREEN,   -1);
    init_pair(CP_CANDLE_DOWN,COLOR_RED,     -1);
    init_pair(CP_BORDER,     COLOR_CYAN,    -1);
  }

  getmaxyx(stdscr, m_termRows, m_termCols);
  resizeWindows();
}

void SwapTerminal::destroyScreen() {
  if (m_winChart)  { delwin(static_cast<WINDOW*>(m_winChart));  m_winChart = nullptr; }
  if (m_winBook)   { delwin(static_cast<WINDOW*>(m_winBook));   m_winBook = nullptr; }
  if (m_winTrades) { delwin(static_cast<WINDOW*>(m_winTrades)); m_winTrades = nullptr; }
  if (m_winStatus) { delwin(static_cast<WINDOW*>(m_winStatus)); m_winStatus = nullptr; }
  if (m_winInput)  { delwin(static_cast<WINDOW*>(m_winInput));  m_winInput = nullptr; }
  endwin();
}

void SwapTerminal::resizeWindows() {
  getmaxyx(stdscr, m_termRows, m_termCols);

  // Destroy old windows
  if (m_winChart)  delwin(static_cast<WINDOW*>(m_winChart));
  if (m_winBook)   delwin(static_cast<WINDOW*>(m_winBook));
  if (m_winTrades) delwin(static_cast<WINDOW*>(m_winTrades));
  if (m_winStatus) delwin(static_cast<WINDOW*>(m_winStatus));
  if (m_winInput)  delwin(static_cast<WINDOW*>(m_winInput));

  // Layout:
  //  Row 0:            Header bar (1 line on stdscr)
  //  Rows 1..H-4:      Main area split into:
  //    Left 60%:        Chart (candlestick)
  //    Right 40% top:   Order book
  //    Right 40% bot:   Trade history
  //  Row H-3:          Status bar
  //  Rows H-2..H-1:    Input bar

  int mainH = m_termRows - 4;  // header(1) + status(1) + input(2)
  int chartW = (m_termCols * 3) / 5;
  int bookW = m_termCols - chartW;
  int bookH = mainH / 2;
  int tradeH = mainH - bookH;

  m_winChart  = static_cast<void*>(newwin(mainH, chartW, 1, 0));
  m_winBook   = static_cast<void*>(newwin(bookH, bookW, 1, chartW));
  m_winTrades = static_cast<void*>(newwin(tradeH, bookW, 1 + bookH, chartW));
  m_winStatus = static_cast<void*>(newwin(1, m_termCols, m_termRows - 3, 0));
  m_winInput  = static_cast<void*>(newwin(2, m_termCols, m_termRows - 2, 0));
}

// ============================================================================
// Main run loop
// ============================================================================

void SwapTerminal::run() {
  initScreen();
  m_running = true;

  // Initial data fetch
  refreshData();

  auto lastRefresh = std::chrono::steady_clock::now();

  while (m_running) {
    // Handle input (non-blocking)
    int ch = getch();
    if (ch != ERR) {
      handleInput(ch);
    }

    // Auto-refresh data every 10 seconds
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - lastRefresh).count() >= 10) {
      refreshData();
      lastRefresh = now;
    }

    // Handle terminal resize
    int newRows, newCols;
    getmaxyx(stdscr, newRows, newCols);
    if (newRows != m_termRows || newCols != m_termCols) {
      resizeWindows();
    }

    // Redraw
    refreshAll();

    // Don't spin CPU
    napms(50);
  }

  destroyScreen();
}

// ============================================================================
// Data refresh
// ============================================================================

void SwapTerminal::refreshData() {
  std::lock_guard<std::mutex> lock(m_dataMutex);

  if (m_cb.fetchOffers) {
    auto offers = m_cb.fetchOffers(m_activePair);
    m_asks.clear();
    m_bids.clear();
    for (auto& o : offers) {
      if (o.isSell) {
        m_asks.push_back(std::move(o));
      } else {
        m_bids.push_back(std::move(o));
      }
    }
    sortOrderBook();
  }

  if (m_cb.fetchTrades) {
    auto trades = m_cb.fetchTrades(m_activePair, 50);
    m_trades.clear();
    for (auto& t : trades) {
      m_trades.push_back(std::move(t));
    }
    buildCandles();

    if (!m_trades.empty()) {
      m_lastPrice = m_trades.front().rate;
    }
  }

  if (m_cb.fetchTwap) {
    m_twap = m_cb.fetchTwap(m_activePair);
  }

  if (m_cb.getBalance) {
    m_balance = m_cb.getBalance();
  }

  checkArbitrageOpportunities();
}

// ============================================================================
// Rendering
// ============================================================================

void SwapTerminal::refreshAll() {
  drawChrome();
  drawChart();
  drawOrderBook();
  drawTradeHistory();
  drawStatusBar();
  drawInputBar();

  wnoutrefresh(stdscr);
  wnoutrefresh(static_cast<WINDOW*>(m_winChart));
  wnoutrefresh(static_cast<WINDOW*>(m_winBook));
  wnoutrefresh(static_cast<WINDOW*>(m_winTrades));
  wnoutrefresh(static_cast<WINDOW*>(m_winStatus));
  wnoutrefresh(static_cast<WINDOW*>(m_winInput));
  doupdate();
}

void SwapTerminal::drawChrome() {
  // Header bar on row 0 of stdscr
  attron(COLOR_PAIR(CP_HEADER) | A_BOLD);
  mvhline(0, 0, ' ', m_termCols);

  std::string header = " XFG/" + m_activePair + " SWAP";
  mvprintw(0, 1, "%s", header.c_str());

  std::string bal = "Balance: " + formatXfg(m_balance) + " XFG";
  mvprintw(0, m_termCols - static_cast<int>(bal.size()) - 1, "%s", bal.c_str());
  attroff(COLOR_PAIR(CP_HEADER) | A_BOLD);
}

void SwapTerminal::drawChart() {
  WINDOW* w = static_cast<WINDOW*>(m_winChart);
  werase(w);

  int rows, cols;
  getmaxyx(w, rows, cols);

  // Border
  wattron(w, COLOR_PAIR(CP_BORDER));
  box(w, 0, 0);
  wattroff(w, COLOR_PAIR(CP_BORDER));

  // Title
  wattron(w, COLOR_PAIR(CP_CYAN) | A_BOLD);
  mvwprintw(w, 0, 2, " XFG/%s Chart ", m_activePair.c_str());
  wattroff(w, COLOR_PAIR(CP_CYAN) | A_BOLD);

  std::lock_guard<std::mutex> lock(m_dataMutex);

  if (m_candles.empty()) {
    wattron(w, COLOR_PAIR(CP_YELLOW));
    mvwprintw(w, rows / 2, (cols - 16) / 2, "No trade data...");
    wattroff(w, COLOR_PAIR(CP_YELLOW));
    return;
  }

  // Determine price range
  double priceMin = 1e18, priceMax = 0.0;
  for (const auto& c : m_candles) {
    priceMin = std::min(priceMin, c.low);
    priceMax = std::max(priceMax, c.high);
  }
  double margin = (priceMax - priceMin) * 0.1;
  if (margin < 1.0) margin = 1.0;
  priceMin -= margin;
  priceMax += margin;

  // Draw candles right-to-left (newest on the right)
  int chartTop = 2;
  int chartBot = rows - 2;
  int maxCandles = (cols - 4) / 2;  // 2 cols per candle (body + gap)
  int numCandles = std::min(static_cast<int>(m_candles.size()), maxCandles);

  for (int i = 0; i < numCandles; ++i) {
    int candleIdx = static_cast<int>(m_candles.size()) - numCandles + i;
    int col = 2 + i * 2;
    drawCandlestick(col, chartTop, chartBot, m_candles[candleIdx], priceMin, priceMax);
  }

  // Price scale on right edge
  int scaleSteps = std::min(chartBot - chartTop, 5);
  for (int i = 0; i <= scaleSteps; ++i) {
    double price = priceMax - (priceMax - priceMin) * i / scaleSteps;
    int row = chartTop + (chartBot - chartTop) * i / scaleSteps;
    wattron(w, COLOR_PAIR(CP_YELLOW) | A_DIM);
    mvwprintw(w, row, cols - 12, "%s", formatRate(price).c_str());
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_DIM);
  }
}

void SwapTerminal::drawCandlestick(int col, int topRow, int bottomRow,
                                     const SwapCandle& candle,
                                     double priceMin, double priceMax) {
  WINDOW* w = static_cast<WINDOW*>(m_winChart);
  int chartH = bottomRow - topRow;
  if (chartH <= 0 || priceMax <= priceMin) return;

  auto priceToRow = [&](double price) -> int {
    double frac = (price - priceMin) / (priceMax - priceMin);
    return bottomRow - static_cast<int>(frac * chartH);
  };

  int rowOpen  = priceToRow(candle.open);
  int rowClose = priceToRow(candle.close);
  int rowHigh  = priceToRow(candle.high);
  int rowLow   = priceToRow(candle.low);

  bool bullish = candle.close >= candle.open;
  int cp = bullish ? CP_CANDLE_UP : CP_CANDLE_DOWN;
  wattron(w, COLOR_PAIR(cp));

  // Wick (high to low)
  int wickTop = std::min(rowHigh, std::min(rowOpen, rowClose));
  int wickBot = std::max(rowLow, std::max(rowOpen, rowClose));
  for (int r = wickTop; r <= wickBot; ++r) {
    mvwaddch(w, r, col, ACS_VLINE);
  }

  // Body
  int bodyTop = std::min(rowOpen, rowClose);
  int bodyBot = std::max(rowOpen, rowClose);
  if (bodyTop == bodyBot) {
    mvwaddch(w, bodyTop, col, ACS_HLINE);
  } else {
    for (int r = bodyTop; r <= bodyBot; ++r) {
      mvwaddch(w, r, col, bullish ? ACS_CKBOARD : ACS_BLOCK);
    }
  }

  wattroff(w, COLOR_PAIR(cp));
}

void SwapTerminal::drawOrderBook() {
  WINDOW* w = static_cast<WINDOW*>(m_winBook);
  werase(w);

  int rows, cols;
  getmaxyx(w, rows, cols);

  wattron(w, COLOR_PAIR(CP_BORDER));
  box(w, 0, 0);
  wattroff(w, COLOR_PAIR(CP_BORDER));

  wattron(w, COLOR_PAIR(CP_CYAN) | A_BOLD);
  mvwprintw(w, 0, 2, " Order Book ");
  wattroff(w, COLOR_PAIR(CP_CYAN) | A_BOLD);

  // Column headers
  wattron(w, A_BOLD | A_DIM);
  mvwprintw(w, 1, 2, "%-10s %-12s %s", "Rate", "Amount", "Maker");
  wattroff(w, A_BOLD | A_DIM);

  std::lock_guard<std::mutex> lock(m_dataMutex);

  int maxLines = (rows - 4) / 2;

  // Asks (sells) — top half, red (cheapest at bottom, closest to spread)
  int askStart = 2;
  int numAsks = std::min(static_cast<int>(m_asks.size()), maxLines);
  for (int i = 0; i < numAsks; ++i) {
    // Show asks in reverse order so cheapest is at bottom (near spread)
    int idx = numAsks - 1 - i;
    wattron(w, COLOR_PAIR(CP_RED));
    mvwprintw(w, askStart + i, 2, "%-10s %-12s %.6s",
              formatRate(m_asks[idx].rate).c_str(),
              formatXfg(m_asks[idx].xfgAmount).c_str(),
              m_asks[idx].makerAlias.c_str());
    wattroff(w, COLOR_PAIR(CP_RED));
  }

  // Spread line
  int spreadRow = askStart + maxLines;
  wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
  double spread = 0.0;
  if (!m_asks.empty() && !m_bids.empty()) {
    spread = m_asks.front().rate - m_bids.front().rate;
  }
  mvwprintw(w, spreadRow, 2, "--- spread: %s ---", formatRate(spread).c_str());
  wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);

  // Bids (buys) — bottom half, green (best bid at top, closest to spread)
  int bidStart = spreadRow + 1;
  int numBids = std::min(static_cast<int>(m_bids.size()), maxLines);
  for (int i = 0; i < numBids; ++i) {
    wattron(w, COLOR_PAIR(CP_GREEN));
    mvwprintw(w, bidStart + i, 2, "%-10s %-12s %.6s",
              formatRate(m_bids[i].rate).c_str(),
              formatXfg(m_bids[i].xfgAmount).c_str(),
              m_bids[i].makerAlias.c_str());
    wattroff(w, COLOR_PAIR(CP_GREEN));
  }
}

void SwapTerminal::drawTradeHistory() {
  WINDOW* w = static_cast<WINDOW*>(m_winTrades);
  werase(w);

  int rows, cols;
  getmaxyx(w, rows, cols);

  wattron(w, COLOR_PAIR(CP_BORDER));
  box(w, 0, 0);
  wattroff(w, COLOR_PAIR(CP_BORDER));

  wattron(w, COLOR_PAIR(CP_CYAN) | A_BOLD);
  mvwprintw(w, 0, 2, " Recent Trades ");
  wattroff(w, COLOR_PAIR(CP_CYAN) | A_BOLD);

  wattron(w, A_BOLD | A_DIM);
  mvwprintw(w, 1, 2, "%-10s %-10s %-5s %s", "Rate", "Vol", "Side", "Age");
  wattroff(w, A_BOLD | A_DIM);

  std::lock_guard<std::mutex> lock(m_dataMutex);

  int maxLines = rows - 3;
  int numTrades = std::min(static_cast<int>(m_trades.size()), maxLines);
  for (int i = 0; i < numTrades; ++i) {
    const auto& t = m_trades[i];
    int cp = t.isBuy ? CP_GREEN : CP_RED;
    wattron(w, COLOR_PAIR(cp));
    mvwprintw(w, 2 + i, 2, "%-10s %-10.4f %-5s %s",
              formatRate(t.rate).c_str(),
              t.volume,
              t.isBuy ? "BUY" : "SELL",
              formatAge(t.timestamp).c_str());
    wattroff(w, COLOR_PAIR(cp));
  }
}

void SwapTerminal::drawStatusBar() {
  WINDOW* w = static_cast<WINDOW*>(m_winStatus);
  werase(w);

  wattron(w, COLOR_PAIR(CP_STATUS));
  mvwhline(w, 0, 0, ' ', m_termCols);

  std::lock_guard<std::mutex> lock(m_dataMutex);

  // Left: TWAP + last price
  mvwprintw(w, 0, 1, "TWAP: %s | Last: %s | Pair: XFG/%s",
            formatRate(m_twap).c_str(),
            formatRate(m_lastPrice).c_str(),
            m_activePair.c_str());

  // Right: arb alert or status message
  if (!m_arbAlert.empty()) {
    wattron(w, A_BOLD | A_BLINK);
    mvwprintw(w, 0, m_termCols - static_cast<int>(m_arbAlert.size()) - 1,
              "%s", m_arbAlert.c_str());
    wattroff(w, A_BOLD | A_BLINK);
  } else if (!m_statusMsg.empty()) {
    time_t now = std::time(nullptr);
    if (now - m_statusTime < 5) {
      mvwprintw(w, 0, m_termCols - static_cast<int>(m_statusMsg.size()) - 1,
                "%s", m_statusMsg.c_str());
    } else {
      m_statusMsg.clear();
    }
  }

  wattroff(w, COLOR_PAIR(CP_STATUS));
}

void SwapTerminal::drawInputBar() {
  WINDOW* w = static_cast<WINDOW*>(m_winInput);
  werase(w);

  int rows, cols;
  getmaxyx(w, rows, cols);
  (void)rows;

  wattron(w, COLOR_PAIR(CP_INPUT));

  // Help line — tiers: 0.8|8|80|800, ratio: 1=best 2=mkt 3=fast
  wattron(w, A_DIM);
  if (m_twap > 0.0) {
    std::string hint = "swap [0.8|8|80|800] [pair] [1=" + formatRate(m_twap * 0.97) +
                       " 2=" + formatRate(m_twap) +
                       " 3=" + formatRate(m_twap * 1.03) + "]";
    hint += " | F1-F3 | q";
    mvwprintw(w, 0, 1, "%.*s", cols - 2, hint.c_str());
  } else {
    mvwprintw(w, 0, 1, "swap [0.8|8|80|800] [pair] [rate] | accept <id> | F1-F3=pair | q");
  }
  wattroff(w, A_DIM);

  // Input prompt
  wattron(w, A_BOLD);
  mvwprintw(w, 1, 1, "> %s_", m_inputBuf.c_str());
  wattroff(w, A_BOLD);

  wattroff(w, COLOR_PAIR(CP_INPUT));
}

// ============================================================================
// Chart helpers
// ============================================================================

void SwapTerminal::buildCandles() {
  // Build 5-minute candles from trade history
  m_candles.clear();
  if (m_trades.empty()) return;

  const int CANDLE_INTERVAL = 300;  // 5 minutes

  // Trades are newest-first, reverse for processing
  std::vector<SwapTrade> sorted(m_trades.begin(), m_trades.end());
  std::sort(sorted.begin(), sorted.end(), [](const SwapTrade& a, const SwapTrade& b) {
    return a.timestamp < b.timestamp;
  });

  time_t bucketStart = (sorted.front().timestamp / CANDLE_INTERVAL) * CANDLE_INTERVAL;

  SwapCandle current;
  current.timestamp = bucketStart;
  current.open = sorted.front().rate;
  current.high = sorted.front().rate;
  current.low = sorted.front().rate;
  current.close = sorted.front().rate;
  current.volume = sorted.front().volume;

  for (size_t i = 1; i < sorted.size(); ++i) {
    time_t ts = sorted[i].timestamp;
    time_t bucket = (ts / CANDLE_INTERVAL) * CANDLE_INTERVAL;

    if (bucket != bucketStart) {
      m_candles.push_back(current);
      bucketStart = bucket;
      current.timestamp = bucket;
      current.open = sorted[i].rate;
      current.high = sorted[i].rate;
      current.low = sorted[i].rate;
      current.close = sorted[i].rate;
      current.volume = sorted[i].volume;
    } else {
      current.high = std::max(current.high, sorted[i].rate);
      current.low = std::min(current.low, sorted[i].rate);
      current.close = sorted[i].rate;
      current.volume += sorted[i].volume;
    }
  }
  m_candles.push_back(current);
}

// ============================================================================
// Order book helpers
// ============================================================================

void SwapTerminal::sortOrderBook() {
  // Asks: ascending by rate (cheapest first)
  std::sort(m_asks.begin(), m_asks.end(), [](const SwapOffer& a, const SwapOffer& b) {
    return a.rate < b.rate;
  });
  // Bids: descending by rate (best bid first)
  std::sort(m_bids.begin(), m_bids.end(), [](const SwapOffer& a, const SwapOffer& b) {
    return a.rate > b.rate;
  });
}

// ============================================================================
// Input handling
// ============================================================================

void SwapTerminal::handleInput(int ch) {
  switch (ch) {
    case 27:  // ESC
    case 'q':
    case 'Q':
      if (m_inputBuf.empty()) {
        m_running = false;
      } else {
        m_inputBuf.clear();
      }
      break;

    case '\n':
    case '\r':
    case KEY_ENTER:
      if (!m_inputBuf.empty()) {
        processCommand(m_inputBuf);
        m_inputBuf.clear();
      }
      break;

    case KEY_BACKSPACE:
    case 127:
    case 8:
      if (!m_inputBuf.empty()) {
        m_inputBuf.pop_back();
      }
      break;

    case KEY_RESIZE:
      resizeWindows();
      break;

    case KEY_F(1):
      switchPair("ETH");
      break;
    case KEY_F(2):
      switchPair("BCH");
      break;
    case KEY_F(3):
      switchPair("XMR");
      break;

    default:
      if (ch >= 32 && ch < 127) {
        m_inputBuf += static_cast<char>(ch);
      }
      break;
  }
}

void SwapTerminal::processCommand(const std::string& cmd) {
  std::istringstream ss(cmd);
  std::string action;
  ss >> action;

  // Normalize to lowercase
  std::transform(action.begin(), action.end(), action.begin(), ::tolower);

  if (action == "pair") {
    std::string pair;
    ss >> pair;
    std::transform(pair.begin(), pair.end(), pair.begin(), ::toupper);
    switchPair(pair);
  }
  else if (action == "swap") {
    // swap <tier_amount> [pair] [1|2|3]
    //   tier_amount: 0.8, 8, 80, or 800 XFG
    //   1 = best (3% in your favor, default)
    //   2 = market (TWAP)
    //   3 = fast fill (3% generous to counterparty)
    double amount = 0.0;
    ss >> amount;

    if (amount <= 0.0) {
      showRecommendation(true);
      return;
    }

    // Validate tier amount
    uint64_t xfgAmount = static_cast<uint64_t>(amount * 1e7 + 0.5);
    static const uint64_t validTiers[] = {
      CryptoNote::parameters::AMOUNT_TIER_0,  // 0.8 XFG
      CryptoNote::parameters::AMOUNT_TIER_1,  // 8 XFG
      CryptoNote::parameters::AMOUNT_TIER_2,  // 80 XFG
      CryptoNote::parameters::AMOUNT_TIER_3   // 800 XFG
    };
    bool validAmount = false;
    for (auto t : validTiers) {
      if (xfgAmount == t) { validAmount = true; break; }
    }
    if (!validAmount) {
      m_statusMsg = "Invalid tier. Use: 0.8, 8, 80, or 800 XFG";
      m_statusTime = std::time(nullptr);
      return;
    }

    // Optional pair override
    std::string token;
    ss >> token;
    if (!token.empty()) {
      std::string upper = token;
      std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
      if (upper == "ETH" || upper == "BCH" || upper == "XMR") {
        switchPair(upper);
        token.clear();
        ss >> token;  // next token is ratio choice
      }
    }

    // Determine rate from ratio choice
    double rate = 0.0;
    double twap = m_twap;

    if (token.empty() || token == "1") {
      rate = (twap > 0.0) ? twap * 0.97 : 0.0;
    } else if (token == "2") {
      rate = twap;
    } else if (token == "3") {
      rate = (twap > 0.0) ? twap * 1.03 : 0.0;
    } else {
      try { rate = std::stod(token); } catch (...) { rate = 0.0; }
    }

    if (rate <= 0.0) {
      m_statusMsg = "No TWAP data. swap <tier> [pair] <custom_rate>";
      m_statusTime = std::time(nullptr);
      return;
    }

    if (m_cb.submitOffer) {
      bool ok = m_cb.submitOffer(xfgAmount, rate, m_activePair);
      m_statusMsg = ok ? "Swap offer posted at " + formatRate(rate) : "Failed to post";
      m_statusTime = std::time(nullptr);
      if (ok) refreshData();
    }
  }
  else if (action == "cancel") {
    std::string offerId;
    ss >> offerId;
    if (offerId.empty()) {
      m_statusMsg = "Usage: cancel <offer_id>";
      m_statusTime = std::time(nullptr);
      return;
    }
    if (m_cb.cancelOffer) {
      bool ok = m_cb.cancelOffer(offerId);
      m_statusMsg = ok ? "Offer cancelled" : "Failed to cancel";
      m_statusTime = std::time(nullptr);
      if (ok) refreshData();
    }
  }
  else if (action == "accept") {
    std::string offerId;
    ss >> offerId;
    if (offerId.empty()) {
      m_statusMsg = "Usage: accept <offer_id>";
      m_statusTime = std::time(nullptr);
      return;
    }
    if (m_cb.acceptOffer) {
      bool ok = m_cb.acceptOffer(offerId);
      m_statusMsg = ok ? "Offer accepted — swap initiated" : "Failed to accept";
      m_statusTime = std::time(nullptr);
      if (ok) refreshData();
    }
  }
  else if (action == "refresh" || action == "r") {
    refreshData();
    m_statusMsg = "Data refreshed";
    m_statusTime = std::time(nullptr);
  }
  else {
    m_statusMsg = "Unknown command: " + action;
    m_statusTime = std::time(nullptr);
  }
}

void SwapTerminal::switchPair(const std::string& pair) {
  if (pair == "ETH" || pair == "BCH" || pair == "XMR") {
    m_activePair = pair;
    refreshData();
    m_statusMsg = "Switched to XFG/" + pair;
    m_statusTime = std::time(nullptr);
  } else {
    m_statusMsg = "Invalid pair: " + pair + " (use ETH, BCH, XMR)";
    m_statusTime = std::time(nullptr);
  }
}

// ============================================================================
// Formatting
// ============================================================================

std::string SwapTerminal::formatRate(double rate) {
  if (rate <= 0.0) return "---";
  std::ostringstream ss;
  if (rate >= 10000.0) {
    ss << std::fixed << std::setprecision(0) << rate;
  } else if (rate >= 100.0) {
    ss << std::fixed << std::setprecision(1) << rate;
  } else if (rate >= 1.0) {
    ss << std::fixed << std::setprecision(2) << rate;
  } else {
    ss << std::fixed << std::setprecision(6) << rate;
  }
  return ss.str();
}

std::string SwapTerminal::formatXfg(uint64_t atomic) {
  uint64_t whole = atomic / 10000000;
  uint64_t frac = atomic % 10000000;
  std::ostringstream ss;
  ss << whole << "." << std::setfill('0') << std::setw(7) << frac;
  // Trim trailing zeros but keep at least 2 decimals
  std::string s = ss.str();
  size_t dot = s.find('.');
  size_t last = s.find_last_not_of('0');
  if (last != std::string::npos && last > dot + 1) {
    s = s.substr(0, last + 1);
  } else if (dot != std::string::npos) {
    s = s.substr(0, dot + 3);
  }
  return s;
}

std::string SwapTerminal::formatAge(time_t ts) {
  time_t now = std::time(nullptr);
  int64_t diff = now - ts;
  if (diff < 0) diff = 0;

  if (diff < 60) {
    return std::to_string(diff) + "s";
  } else if (diff < 3600) {
    return std::to_string(diff / 60) + "m";
  } else if (diff < 86400) {
    return std::to_string(diff / 3600) + "h";
  } else {
    return std::to_string(diff / 86400) + "d";
  }
}

// ============================================================================
// Smart order recommendation — always favors XFG holders
// ============================================================================

double SwapTerminal::getRecommendedRate(bool /*isSell*/) const {
  // Rate = XFG per 1 CTR. Lower rate = XFG more expensive = better for XFG holder.
  // Recommend 3% below TWAP — user gets more value per XFG swapped out.
  double twap = m_twap;
  if (twap <= 0.0) return 0.0;
  return twap * 0.97;
}

void SwapTerminal::showRecommendation(bool /*isSell*/) {
  double twap = m_twap;
  if (twap <= 0.0) {
    m_statusMsg = "No TWAP data yet";
  } else {
    m_statusMsg = "1=" + formatRate(twap * 0.97) + "(best) "
                + "2=" + formatRate(twap) + "(market) "
                + "3=" + formatRate(twap * 1.03) + "(fast)";
  }
  m_statusTime = std::time(nullptr);
}

// ============================================================================
// Cross-pair arbitrage detection
// ============================================================================

void SwapTerminal::checkArbitrageOpportunities() {
  // Simple triangular arbitrage check:
  // If XFG/ETH and XFG/BCH imply a different ETH/BCH rate than market,
  // there's an arb opportunity.
  //
  // We can only check pairs we track. Compare implied cross-rate with
  // the best ask/bid across pairs.

  m_arbAlert.clear();

  // Need at least 2 pairs with active TWAP
  // For now, compare current pair's best ask vs recommended rate
  std::lock_guard<std::mutex> lock(m_dataMutex);

  if (m_asks.empty() || m_twap <= 0.0) return;

  double bestAsk = m_asks.front().rate;

  // If someone is selling XFG significantly below TWAP, that's a buy opportunity
  if (bestAsk < m_twap * 0.95) {
    double discount = (1.0 - bestAsk / m_twap) * 100.0;
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);
    ss << "BUY SIGNAL: XFG/" << m_activePair << " "
       << discount << "% below TWAP!";
    m_arbAlert = ss.str();
  }

  // If best bid is significantly above TWAP, that's a sell opportunity
  if (!m_bids.empty()) {
    double bestBid = m_bids.front().rate;
    if (bestBid > m_twap * 1.05) {
      double premium = (bestBid / m_twap - 1.0) * 100.0;
      std::ostringstream ss;
      ss << std::fixed << std::setprecision(1);
      ss << "SELL SIGNAL: XFG/" << m_activePair << " "
         << premium << "% above TWAP!";
      m_arbAlert = ss.str();
    }
  }
}

} // namespace CryptoNote
