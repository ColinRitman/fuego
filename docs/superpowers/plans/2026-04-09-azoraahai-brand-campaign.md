# AzorAhai Brand Voice & Campaign Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce all 11 brand and campaign assets for the AzorAhai Release (Block 1,000,000), starting with a canonical brand voice guidelines doc and ending with Tier 1 launch-ready content.

**Architecture:** Foundation-first — brand guidelines and visual spec are written before any copy. All content is verified against the terminology bible and dark-brand direction before commit. README cleanup happens immediately after foundation to unblock all other copy. Tier 1 launch content (BitcoinTalk, Twitter, Discord) is written last, drawing on all prior work.

**Tech Stack:** Markdown, git, local repo at `/Users/aejt/Documents/GitHub/fire`. No build system. Verification is brand-compliance checklist review, not automated tests. Commit + push after each task.

**Spec:** `docs/superpowers/specs/2026-04-08-azoraahai-brand-campaign-design.md`

---

## File Map

| File | Action | Asset | Responsibility |
|------|--------|-------|----------------|
| `.claude/brand-voice-guidelines.md` | Create | A (canonical) | Brand voice reference — tone, pillars, palette, terminology |
| `docs/superpowers/brand-voice-guidelines.md` | Create | A (copy) | Repo-visible copy of canonical, kept in sync |
| `docs/superpowers/brand-visual-spec.md` | Create | K | Color swatches, fire palette, typography, application rules |
| `README.md` | Modify | B | Remove stale refs; add AzorAhai release section |
| `docs/superpowers/content/azoraahai-bitcointalk.md` | Create | C | Full technical announcement post |
| `docs/superpowers/content/azoraahai-twitter-threads.md` | Create | D | 4 pre-written Twitter/X threads |
| `docs/superpowers/content/azoraahai-discord.md` | Create | E | Countdown pin + launch day @everyone |
| `docs/superpowers/content/why-we-built-swapxfg.md` | Create | F | Long-form blog post |
| `docs/superpowers/content/xfg-cd-product-description.md` | Create | G | 150-word XFG CD product description |
| `docs/superpowers/content/ospead-explainer.md` | Create | H | OSPEAD plain-language paragraph |
| `docs/superpowers/content/v11-roadmap-teaser.md` | Create | I | v11 teaser paragraph |

Asset J (SEO audit) is a tool invocation (`/marketing:seo-audit`), not a file — triggered in Chunk 3.

---

## Brand Compliance Checklist

Every content asset is verified against this checklist before commit. This is the "test" for content work.

```
[ ] Uses canonical terms (XFG CD, SwapXFG, OSPEAD, ring size 8–18, commitment deposit)
[ ] No deprecated terms (EFiers, Elderfiers, COLD deposit, Burn2Mint, mixin 8 in public copy)
[ ] v11 features (COLDAO Note, HEAT token) described as future only, never present tense
[ ] HEAT commitment burns described correctly as v10: burning XFG creates `HEAT_commitment` tx_extra — permanently pads decoy pool. HEAT ERC-20 token minting is v11 (needs ZK proof + prover contract).
[ ] Privacy limitations stated honestly (v10 amounts are plaintext)
[ ] No marketing inflation ("revolutionary", "game-changing", "excited to announce")
[ ] Dark-first visual language — black canvas, orange accent, no "bright" or "light" framing
[ ] Fuego orange (#FF5500) referenced as the fire/accent color
[ ] SwapXFG is the headline feature for AzorAhai messaging
[ ] Monero not positioned as competitor (different product category framing)
```

---

## Chunk 1: Foundation — Brand Guidelines + Visual Spec

### Task 1: Brand Voice Guidelines (Asset A — Canonical)

**Files:**
- Create: `.claude/brand-voice-guidelines.md`

- [ ] **Step 1: Create the canonical brand guidelines file**

Path: `.claude/brand-voice-guidelines.md`

```markdown
# Fuego (XFG) Brand Voice Guidelines
# AzorAhai Release — Canonical Reference

> Canonical location: .claude/brand-voice-guidelines.md
> Repo copy: docs/superpowers/brand-voice-guidelines.md
> Source spec: docs/superpowers/specs/2026-04-08-azoraahai-brand-campaign-design.md

---

## Identity

| Element | Value |
|---------|-------|
| Project name | Fuego |
| Ticker | XFG |
| Address prefix | `fire` — every Fuego address begins with "fire" |
| Daemon binary | `fuegod` |
| Release name | AzorAhai |
| Release tagline | "The Fire Has Arrived." |
| Pre-launch tagline | "The Long Night Is Almost Over." |
| Mission statement | "A decentralized blockchain bank with ledger deposits paying interest, without involvement of legacy financial institutions, powered by 100% open source code." |
| Builder philosophy | "Working software is the primary measure of progress." |

---

## Visual Brand

**Principle:** Black is the night. Orange is the fire. Dark-first, always.

| Role | Hex | Usage |
|------|-----|-------|
| Base / background | `#000000` | All surfaces. No light mode. |
| Near-black (depth) | `#180400` | Fire-tinted black for cards, panels |
| Accent | `#FF5500` | CTAs, active states, key data, fire fills |
| Accent mid | `#CC3300` | Hover, secondary accents |
| Text primary | `#FFFFFF` | On black |
| Text secondary | `#999999` | Supporting copy, metadata |
| Muted | `#555555` | Disabled labels, inactive elements |

**Fire gradient (11 stops, from styles.go):**
`#180400` → `#3D0A00` → `#6D1400` → `#9E2000` → `#D43800` → `#FF5500` → `#FF7F11` → `#FFA940` → `#FFD066` → `#FFE899` → `#FFFADD`

**Rules:**
- Never use orange as background behind body text
- Fire symbol (triangle): always on black; fire gradient fill or white outline only
- Error/warning states: orange (not red — stays on-brand)

---

## Voice Attributes

### 1. Technically Honest
State limitations plainly, in the same breath as capabilities. Never overclaim privacy.

✅ "What v10 does not hide: Amounts are plaintext."
❌ "Fuego provides complete transaction privacy."

### 2. Direct, Zero Inflation
Short declarative sentences. Present tense. Active voice. No hedging or qualifiers.

✅ "TradeOgre is dead. Atomic swaps let users trade trustlessly, no exchange needed."
❌ "We are excited to announce a revolutionary new solution for decentralized trading."

### 3. Builder Ethos
Ship first, explain after. Hype is a liability.

✅ "SwapXFG is live. XMR ↔ XFG. No exchange. No KYC."
❌ "We are working hard to bring you an exciting new product very soon."

### 4. Sovereign Money, No Intermediaries
Frame every feature through individual sovereignty. The audience already knows why surveillance finance is a threat.

✅ "Your XFG address begins with fire. Every address does. Only you control the keys."
❌ "Fuego uses advanced cryptographic techniques to ensure user privacy."

---

## Audience Personas

**Persona A — Technical Privacy User**
- Runs a node, reads changelogs, knows ring signatures and CryptoNote
- Holds XMR, possibly BCH; lost TradeOgre/SideShift access
- Needs: Technical accuracy, protocol specs, honest privacy comparisons
- Tone: Peer-to-peer technical. Tables, specs, version numbers welcome.

**Persona B — Non-Technical Privacy Seeker**
- Wants yield without a bank, swap without an exchange
- Drawn to sovereignty narrative and fire mythology
- Needs: Plain-language product descriptions, benefit-before-mechanism framing
- Tone: Direct, warm, no unexplained jargon.

---

## Messaging Pillars (AzorAhai ranked order)

1. **Trustless exchange** — SwapXFG leads. XMR ↔ XFG, ETH ↔ XFG. No exchange, no KYC.
2. **Real yield, no bank** — XFG CDs. Swap-fee funded, ring-sig protected withdrawal.
3. **Honest privacy, proven roadmap** — OSPEAD + ring size 8–18. What v10 does and does not hide.
4. **Sovereign money** — No central authority. Your keys. Open-source.
5. **Roadmap credibility** — v11: COLDAO Notes + HEAT token. Future tense only.

---

## Tone by Channel

| Channel | Tone |
|---------|------|
| Twitter/X | Punchy, ≤2 sentences. Blunt market commentary. Fact first. |
| Discord | Warmer, "we" voice, invites participation. Link to docs. |
| BitcoinTalk | Technical, structured. Tables, specs, version numbers. Full detail expected. |
| GitHub/docs | Declarative, present tense, never hedges. No marketing language. |
| Press/announcement | Lead with product story. Fire mythology as opener and closer. |
| Reddit (r/xmrtrader, r/privacy) | Technical but accessible. Honest about Monero comparison. |

---

## Terminology Bible

### Canonical — always use

| Term | Usage |
|------|-------|
| Fuego | Project name, always capitalized |
| XFG | Ticker, always uppercase |
| SwapXFG | Atomic swap application name |
| XFG CD | Consumer product name (Certificate of Deposit) |
| commitment deposit | Technical/protocol term |
| FuCIA | Internal: "Fuego Untraceable Custom Interest Assets." Optional in public copy. |
| OSPEAD | Adaptive decoy selection algorithm. Always capitalized as acronym. |
| ring size 8–18 | Dynamic ring size range for public copy (total ring, not mixin count) |
| HEAT commitment burn | v10: burning XFG creates `HEAT_commitment` in tx_extra — permanently unspendable, pads commitment decoy pool. The HEAT ERC-20 (minted on Ethereum via ZK proof) is v11. |
| CommitmentI/O | Protocol-level docs only. Not in campaign copy. |
| Dynamigo | Internal protocol version name (block 999,999 upgrade) |
| AzorAhai | Public release name (block 1,000,000) |
| COLDAO Note | v11: deposit XFG, earn COLDAO governance tokens (PIK structure) |
| HEAT token | v11: ZK-proven burn collateral, inflation-minted |
| fuegod | Daemon binary |
| fire | Address prefix |

### Deprecated — never use in public copy

| Term | Replace with |
|------|-------------|
| Elderfier / EFier | ZK proofs of Merkle trees |
| COLD deposit | XFG CD / commitment deposit |
| Burn2Mint | (dropped) |
| mixin 8 (public) | ring size 8–18 |

### v11 roadmap — future tense only

- COLDAO Note, COLDAO token — v11, not shipping with AzorAhai
- HEAT ERC-20 token — v11; HEAT commitment burns (the on-chain `HEAT_commitment` tx_extra + permanent decoy mechanism) are v10

---

## What to Avoid

- Direct Monero comparison (different product category — banking + swap infra on CryptoNote)
- Overclaiming privacy (v10 amounts are plaintext)
- Marketing inflation: "revolutionary," "game-changing," "excited to announce"
- Fire metaphor forced into technical documentation
- EFiers, Burn2Mint, COLD deposit in any user-facing copy
- HEAT token or COLDAO described as v10 features
```

- [ ] **Step 2: Verify against brand compliance checklist**

Read the file. Check every item in the Brand Compliance Checklist at the top of this plan. Fix any failures before proceeding.

- [ ] **Step 3: Copy to repo location**

Copy `.claude/brand-voice-guidelines.md` verbatim to `docs/superpowers/brand-voice-guidelines.md`. Both files must be byte-for-byte identical at this point.

- [ ] **Step 4: Commit and push**

```bash
git add .claude/brand-voice-guidelines.md docs/superpowers/brand-voice-guidelines.md
git commit -m "feat(brand): add canonical brand voice guidelines

AzorAhai release brand voice: voice attributes, audience personas,
messaging pillars, terminology bible, dark-first visual palette.
Canonical: .claude/brand-voice-guidelines.md

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
git push origin xfgCdswaps
```

---

### Task 2: Visual Brand Spec (Asset K)

**Files:**
- Create: `docs/superpowers/brand-visual-spec.md`
- Read: `swapxfg/app/styles.go` (exact color values)
- Read: `swapxfg/app/splash.go` (fire symbol / triangle geometry)

- [ ] **Step 1: Read source files for exact values**

Read `swapxfg/app/styles.go` — extract all color constants.
Read `swapxfg/app/splash.go` — extract fire symbol (triangle) geometry and animation notes.

- [ ] **Step 2: Write the visual brand spec**

Path: `docs/superpowers/brand-visual-spec.md`

```markdown
# Fuego (XFG) Visual Brand Spec
# AzorAhai Release

> Extracted from: swapxfg/app/styles.go, swapxfg/app/splash.go
> Consumer: web team, social media templates
> Output format: This document is the design token reference.

---

## Core Palette

| Role | Hex | Variable (styles.go) | Usage |
|------|-----|----------------------|-------|
| Base background | `#000000` | — | All surfaces. Dark-first. No light mode. |
| Fire base (near-black) | `#180400` | `FirePalette[0]` | Fire-tinted black for cards, panels, depth |
| Accent (Fuego orange) | `#FF5500` | `ColorAccent` | CTAs, active states, key data, fire fills |
| Accent mid | `#CC3300` | — | Hover states, secondary accents |
| Bullish | `#00CC66` | `ColorBullish` | Positive price / balance changes |
| Bearish / Error | `#FF3344` | `ColorBearish` | Negative changes, error states |
| Spread | `#FFAA00` | `ColorSpread` | Bid/ask spread, yield display |
| Muted | `#555555` | `ColorMuted` | Supporting copy, metadata, inactive elements |
| Own position | `#00CCCC` | `ColorOwn` | User's own orders in orderbook |
| Escrow (pulsing) | `#FFDD00` | `ColorEscrow` | Escrow/locked state indicator |
| Conn OK | `#00FF00` | `ColorConnOK` | Network connected indicator |
| Conn lost | `#FF0000` | `ColorConnLost` | Network disconnected indicator |
| Text primary | `#FFFFFF` | `ColorActiveTab` | Body text on black |
| Text inactive | `#777777` | `ColorInactive` | Disabled / inactive labels |

---

## Fire Gradient (11 stops)

Source: `FirePalette` in `swapxfg/app/styles.go`

| Stop | Hex | FireChars glyph |
|------|-----|-----------------|
| 0 (darkest) | `#180400` | ` ` (space) |
| 1 | `#3D0A00` | `.` |
| 2 | `#6D1400` | `:` |
| 3 | `#9E2000` | `*` |
| 4 | `#D43800` | `░` (U+2591) |
| 5 | `#FF5500` | `▒` (U+2592) |
| 6 | `#FF7F11` | `▓` (U+2593) |
| 7 | `#FFA940` | `█` (U+2588) |
| 8 | `#FFD066` | `█` (U+2588) |
| 9 | `#FFE899` | `█` (U+2588) |
| 10 (lightest) | `#FFFADD` | `█` (U+2588) |

**CSS gradient (left to right, dark to light):**
```css
background: linear-gradient(
  to right,
  #180400, #3D0A00, #6D1400, #9E2000, #D43800,
  #FF5500, #FF7F11, #FFA940, #FFD066, #FFE899, #FFFADD
);
```

---

## Tab / Navigation Styles

| State | Foreground | Background | Bold |
|-------|-----------|------------|------|
| Active tab | `#FFFFFF` | `#FF5500` | Yes |
| Inactive tab | `#777777` | transparent | No |
| Input text | `#FFFFFF` | — | Yes |

---

## Fire Symbol (Triangle)

Source: `swapxfg/app/splash.go`

- Geometry: ASCII triangle (🜂 alchemical fire symbol) rendered in terminal with fire simulation
- Always rendered on black background
- Fill: fire gradient (bottom dark, top light) or white outline on black
- Never render on non-black background
- Never use orange fill behind body text

---

## Design Principles

1. **Dark-first.** `#000000` is the only valid background color. No light mode.
2. **Black canvas, orange fire.** Every composition is dark surface + Fuego orange accent.
3. **High contrast required.** The fire must be visible against the night.
4. **No grey-wash neutrals as primaries.** Muted (`#555555`) is for supporting copy only.
5. **Consistent data semantics.** Bullish = `#00CC66`, Bearish = `#FF3344`, Spread = `#FFAA00`. Never repurpose these.

---

## Typography

SwapXFG is a terminal application — all type is monospace. For web/social:

| Role | Recommendation |
|------|---------------|
| Display / headline | Bold monospace or geometric sans-serif |
| Body | System monospace or geometric sans |
| Code / addresses | Monospace, Fuego orange or white on black |
| Never | Serif fonts, light weights on dark background |
```

- [ ] **Step 3: Verify against brand compliance checklist**

Check the visual spec file confirms dark-first direction and correct color values against `styles.go`.

- [ ] **Step 4: Commit and push**

```bash
git add docs/superpowers/brand-visual-spec.md
git commit -m "feat(brand): add visual brand spec

11-stop fire palette, full color token table, tab styles, fire symbol (triangle)
rules. Dark-first: black base (#000000), Fuego orange (#FF5500) accent.
Extracted from swapxfg/app/styles.go + splash.go.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
git push origin xfgCdswaps
```

---

### Task 3: README Cleanup (Asset B)

**Files:**
- Modify: `README.md`

Before touching any content, read the full README to understand what currently exists.

- [ ] **Step 1: Read README.md in full**

Read `README.md`. Identify all instances of:
- "Elderfier" / "EFier" / "elderfier"
- "Burn2Mint" / "burn2mint"
- "COLD deposit" / "COLD command" (as user-facing feature description)
- "HEAT" described as a current v10 consumer product
- Any stale TUI feature descriptions referencing the above

- [ ] **Step 2: Remove stale references**

Make targeted edits only — do not restructure or rewrite sections not touching stale content.

For each stale reference found:
- "Elderfier Staking" → remove the sentence entirely
- "Burn2Mint flows" → remove the sentence entirely
- "COLD deposit" as user-facing feature → replace with "XFG CDs (commitment deposits)"
- HEAT ERC-20 token described as v10 → clarify: HEAT commitment burns (burning XFG creates `HEAT_commitment` tx_extra → permanent unspendable decoy) are v10. The HEAT ERC-20 minting contract on Ethereum (requires ZK proof via prover contract) is v11.

- [ ] **Step 3: Add AzorAhai release section**

After the existing feature list / capabilities section, add:

```markdown
### AzorAhai Release — Block 1,000,000

The long night is over. At block 1,000,000, Fuego ships its most significant
upgrade: a complete sovereign money stack.

**SwapXFG** — Trustless atomic swaps with no exchange and no KYC.
Supported pairs: XMR ↔ XFG, ETH ↔ XFG, SOL ↔ XFG, wXFG ↔ XFG.
Both XMR and XFG are Ed25519/CryptoNote — adaptor signatures on both sides.

**XFG CDs** — Commitment deposits earning real yield in XFG, funded by swap
fees (not inflation). Withdrawal is ring-signature protected. Transfer before
maturity via the SwapXFG secondary market.

**Dynamic ring size 8–18 with OSPEAD** — Ring size scales adaptively from 8
to 18 based on available outputs. OSPEAD (adaptive decoy selection) chooses
ring members that match real spending patterns, hardening against analysis.

**New deposit transaction structure** — Commitment-scheme privacy for
deposited amounts at the protocol level (v10 feature; hidden amounts are a
v11 goal — see roadmap).

**v11 roadmap:** COLDAO Notes (deposit XFG, earn COLDAO governance tokens)
and HEAT token (each token forever collateralized by ZK-proven burned XFG)
are in development for after block 1M.
```

- [ ] **Step 4: Verify against brand compliance checklist**

Re-read the modified README sections. Run through every item on the Brand Compliance Checklist.

- [ ] **Step 5: Commit and push**

```bash
git add README.md
git commit -m "docs(readme): remove stale refs, add AzorAhai release section

Remove Elderfier staking, Burn2Mint, COLD deposit references.
Add AzorAhai release section: SwapXFG, XFG CDs, OSPEAD/ring 8-18,
new commitment deposit structure, v11 roadmap teaser.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
git push origin xfgCdswaps
```

---

## Chunk 2: Tier 1 Launch Content

### Task 4: Supporting Copy Pieces (Assets G, H, I)

Write these three short-form pieces first — they feed directly into the longer Tier 1 content.

**Files:**
- Create: `docs/superpowers/content/xfg-cd-product-description.md`
- Create: `docs/superpowers/content/ospead-explainer.md`
- Create: `docs/superpowers/content/v11-roadmap-teaser.md`

- [ ] **Step 1: Write XFG CD product description (Asset G)**

Path: `docs/superpowers/content/xfg-cd-product-description.md`

Target: ~150 words. Plain language. Persona B audience. Benefit-before-mechanism.

```markdown
# XFG CD — Product Description

**XFG CDs (commitment deposits)** are Fuego's on-chain savings instrument.
Lock XFG for a fixed term. Earn real yield — paid in XFG, funded by swap
fees from the SwapXFG network, not by inflation.

Unlike a bank deposit, there is no custodian. Your XFG is locked on-chain
by cryptographic commitment. Only you can withdraw. Withdrawal is protected
by ring signatures — an observer cannot tell which commitment you spent.

Terms range from short to long. Longer terms earn higher yield. Need
liquidity before maturity? XFG CDs are transferable: sell your position on
the SwapXFG secondary market before the term expires.

There are no accounts, no KYC, no counterparty risk. Your XFG address begins
with `fire`. Your deposit does too.

Yield is real. Privacy is real. The bank is you.
```

- [ ] **Step 2: Write OSPEAD plain-language paragraph (Asset H)**

Path: `docs/superpowers/content/ospead-explainer.md`

Target: 1 paragraph (~80 words). Works for both README and website privacy section.

```markdown
# OSPEAD — Plain-Language Explainer

Fuego uses OSPEAD — an Optimal Spent-Output Probability Estimation Adaptive
Decoy selection algorithm — to choose ring members that match real spending
patterns rather than selecting them uniformly at random. Uniform random
selection makes real outputs statistically detectable over time. OSPEAD
closes this gap: the ring members chosen for your transaction look like
plausible real spends, making chain analysis significantly harder. Combined
with a dynamic ring size of 8–18, this is the most meaningful privacy
improvement in Fuego's v10 protocol.
```

- [ ] **Step 3: Write v11 roadmap teaser (Asset I)**

Path: `docs/superpowers/content/v11-roadmap-teaser.md`

Target: 1 paragraph. Future tense only. Use approved framing from spec.

```markdown
# v11 Roadmap Teaser

After block 1,000,000, Fuego's next milestone is v11: **COLDAO Notes** let
you deposit XFG and earn COLDAO governance tokens as yield — a
payment-in-kind structure where interest is paid in protocol governance
rather than currency. **HEAT tokens** take a different angle: each token is
forever collateralized by permanently destroyed XFG, proven on-chain by
zero-knowledge proof. Every HEAT token in existence corresponds to XFG that
can never return to circulation. Both features require Fuego's commitment
Merkle proof generation, now in development.
```

- [ ] **Step 4: Verify all three against brand compliance checklist**

Read each file. Check every item on the Brand Compliance Checklist. Fix any failures.

- [ ] **Step 5: Commit and push**

```bash
git add docs/superpowers/content/
git commit -m "feat(content): add XFG CD description, OSPEAD explainer, v11 teaser

Asset G: 150-word XFG CD product description (Persona B, plain-language).
Asset H: OSPEAD plain-language paragraph for README + web.
Asset I: v11 roadmap teaser with approved COLDAO Note + HEAT token framing.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
git push origin xfgCdswaps
```

---

### Task 5: BitcoinTalk Announcement (Asset C)

**Files:**
- Create: `docs/superpowers/content/azoraahai-bitcointalk.md`
- Read: `docs/ATOMIC_SWAP_PLAN.md` (pair priority, protocol details)
- Read: `docs/DYNAMIC_RING_SIZE.md` (ring size details)
- Read: `docs/PRIVACY_ROADMAP.md` (v10 capabilities and limitations)
- Reference: `docs/superpowers/content/ospead-explainer.md` (already written)
- Reference: `docs/superpowers/content/v11-roadmap-teaser.md` (already written)

- [ ] **Step 1: Read source technical docs**

Read `docs/ATOMIC_SWAP_PLAN.md`, `docs/DYNAMIC_RING_SIZE.md`, and `docs/PRIVACY_ROADMAP.md`. Extract:
- Confirmed swap pairs and protocol details
- Exact ring size behavior
- v10 privacy capabilities vs. limitations table

- [ ] **Step 2: Write the BitcoinTalk announcement**

Path: `docs/superpowers/content/azoraahai-bitcointalk.md`

The post structure:
1. Fire mythology opener (2-3 sentences max)
2. What is shipping (bullet list, technical precision)
3. SwapXFG — pairs, protocol, adaptor sigs
4. XFG CDs — mechanics, yield source, secondary market
5. Privacy upgrades — OSPEAD + ring 8–18 (use Asset H paragraph)
6. Protocol changelog table (v9 → v10)
7. What v10 does NOT hide (honest privacy section — required)
8. v11 roadmap (use Asset I paragraph)
9. Community links, fire closer

```markdown
# [AzorAhai] Fuego v10 — Block 1,000,000 Release: SwapXFG Atomic Swaps,
  XFG CDs, OSPEAD + Dynamic Ring 8–18

---

The long night is over.

At block 1,000,000, Fuego (XFG) ships its most significant upgrade since
launch. Not a roadmap. Not a testnet. Working software.
---

## What's Shipping

- **SwapXFG** — Trustless cross-chain atomic swaps. No exchange. No KYC.
- **XFG CDs** — On-chain commitment deposits earning real yield in XFG.
- **OSPEAD + Dynamic Ring 8–18** — Adaptive decoy selection with ring sizes
  scaling from 8 to 18.
- **New deposit transaction structure** — Commitment-scheme privacy at protocol level.

---

## SwapXFG — Atomic Swaps

TradeOgre is dead. The privacy coin ecosystem lacks a trading
venue. Atomic swaps let users trade trustlessly, no exchange needed.

**Supported pairs (priority order):**

| Pair | Protocol | Both Ed25519? |
|------|----------|---------------|
| XMR ↔ XFG | COMIT adaptor sigs on both sides | Yes - privacy pair |
| ETH ↔ XFG | HashedTimelock.sol + adaptor sig on XFG | No |
| SOL ↔ XFG | Solana HTLC + adaptor sig on XFG | Yes |
| HEAT ↔ XFG | ERC-20 HTLC + adaptor sig on XFG | — |
| LUSD ↔ XFG | ERC-20 HTLC + adaptor sig on XFG | — |

XFG swap outputs are normal `KeyOutput` to Musig2 joint addresses —
indistinguishable from regular transactions on-chain. The XMR ↔ XFG pair
uses COMIT adaptor signatures on both sides: both Ed25519/CryptoNote
chains, no HTLC required on either side.

---

## XFG CDs — Commitment Deposits

**XFG CDs (commitment deposits)** are Fuego's on-chain savings instrument.
Lock XFG for a fixed term. Earn real yield — paid in XFG, funded by swap
fees from the SwapXFG network, not by inflation.

Unlike a bank deposit, there is no custodian. Your XFG is locked on-chain
by cryptographic commitment. Only you can withdraw. Withdrawal is protected
by ring signatures — an observer cannot tell which commitment you spent.

Terms range from short to long. Longer terms earn higher yield. Need
liquidity before maturity? XFG CDs are transferable: sell your position on
the SwapXFG secondary market even before the term expires.

There are no accounts, no KYC, no counterparty risk. 
Yield is real. Privacy is real. The bank is you.

---

## Privacy Upgrades: OSPEAD + Dynamic Ring 8–18

Fuego uses OSPEAD — an Optimal Spent-Output Probability Estimation Adaptive
Decoy selection algorithm — to choose ring members that match real spending
patterns rather than selecting them uniformly at random. Uniform random
selection makes real outputs statistically detectable over time. OSPEAD
closes this gap: the ring members chosen for your transaction look like
plausible real spends, making chain analysis significantly harder. Combined
with a dynamic ring size of 8–18, this is the most meaningful privacy
improvement in Fuego's v10 protocol.

**v10 ring size behavior:**

| Condition | Ring size |
|-----------|-----------|
| Sufficient outputs available | 18 (maximum) |
| Limited outputs | Adaptive (8–18) |
| Absolute minimum | 8 |

---

## Protocol Changelog: v9 → v10 (AzorAhai)

| Feature | v9 | v10 (AzorAhai) |
|---------|----|-----------------|
| Ring size | Fixed 8 | Dynamic 8–18 |
| Decoy selection | Uniform random | OSPEAD adaptive |
| Atomic swaps | None | SOL, ETH, XMR, wXFG, LUSD |
| Commitment deposits | None | XFG CDs (real-yield, ring-sig withdrawal) |
| Deposit tx structure | — | CommitmentI/O |
| HEAT commitment burns | Active (permanent decoys) | Active (unchanged) — burn XFG → `HEAT_commitment` tx_extra → permanent unspendable decoy. HEAT ERC-20 minting is v11. |

---

## What v10 Does NOT Hide

Fuego doesn't overclaim. Here is what v10 still exposes:

- **Amounts are plaintext.** `TransactionOutput.amount` is visible on-chain.
  Hidden amounts are a v11 goal.
- **Deposit terms and amounts** in commitment deposit outputs are observable
  (the commitment scheme hides which output is spent on withdrawal, not the
  deposited amount).
- **Swap counterparty chains** (ETH, SOL) have transparent on-chain state.
  XFG-side outputs are private; counterparty-side are not.

The honest answer is that v10 is meaningfully more private than v9. It is
not fully private. The roadmap is public. We ship what works.

---

## v11 Roadmap

After block 1,000,000, Fuego's next milestone is v11: **COLDAO Notes** let
you deposit XFG and earn COLDAO governance tokens as yield — a
payment-in-kind structure where interest is paid in protocol governance
rather than currency. **HEAT tokens** take a different angle: each token is
forever collateralized by permanently destroyed XFG, proven on-chain by
zero-knowledge proof. Every HEAT token in existence corresponds to XFG that
can never return to circulation. Both features require Fuego's commitment
Merkle proof generation, now in development.

---

## Links

- Website: https://usexfg.org
- GitHub: https://github.com/usexfg/fuego
- Discord: https://discord.gg/5UJcJJg
- Twitter: https://twitter.com/useXFG
- Explorer: http://fuego.spaceportx.net

Your XFG address begins with fire. The fire has arrived.
```

- [ ] **Step 3: Verify against brand compliance checklist**

- [ ] **Step 4: Commit and push**

```bash
git add docs/superpowers/content/azoraahai-bitcointalk.md
git commit -m "feat(content): add AzorAhai BitcoinTalk announcement draft

Full technical announcement: SwapXFG pairs/protocol, XFG CDs,
OSPEAD+ring 8-18, v10 changelog table, honest privacy, 
v11 teaser. Fire mythology opener and closer.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
git push origin xfgCdswaps
```

---

### Task 6: Twitter/X Thread Series (Asset D)

**Files:**
- Create: `docs/superpowers/content/azoraahai-twitter-threads.md`

- [ ] **Step 1: Write all 4 threads**

Path: `docs/superpowers/content/azoraahai-twitter-threads.md`

Each thread tweet ≤280 chars. Threads are 3–6 tweets each. Label each tweet.

```markdown
# AzorAhai Twitter/X Thread Series

Post timing per spec content calendar:
- Thread 1: Block 999,900
- Thread 2: Block ~999,950
- Thread 3: Block ~999,975
- Thread 4: Block 1,000,000

---

## Thread 1 — Pre-launch Teaser (Block 999,900)

**Tweet 1 (standalone):**
The long night is over.

Block 999,900. 🔥 #XFG

---

## Thread 2 — SwapXFG Demo (Block ~999,950)

**Tweet 1 (opener):**
SwapXFG is almost live.

Trustless atomic swaps. No exchange. No KYC.
XMR ↔ XFG. ETH ↔ XFG. SOL ↔ XFG.

Here's how it works 🧵

**Tweet 2:**
TradeOgre is dead. SideShift has limits.

Privacy coin holders have been stuck. Atomic swaps change that.
You hold your keys on both sides for the entire swap.

**Tweet 3:**
XMR ↔ XFG uses COMIT adaptor signatures on both chains.

Both Ed25519. Both CryptoNote. No HTLC required on either side.
The XFG output is a normal KeyOutput — indistinguishable on-chain.

**Tweet 4:**
ETH ↔ XFG uses a HashTimeLock contract on Ethereum.
Adaptor signature privacy on the XFG side.

Swap ETH for private XFG. No KYC. No wrapped tokens on the XFG side.

**Tweet 5:**
SwapXFG also has a CD secondary market.

Sell your XFG commitment deposit before maturity.
Yield, privacy, and liquidity without an exchange.

**Tweet 6 (closer):**
Block 1,000,000. The fire arrives.

github.com/usexfg/fuego
usexfg.org

---

## Thread 3 — XFG CD Explainer (Block ~999,975)

**Tweet 1 (opener):**
XFG CDs: earn yield on private money.

No bank. No custodian. No KYC.
Real yield, paid in XFG.

🧵

**Tweet 2:**
Lock XFG on-chain for a fixed term.

Yield is funded by SwapXFG swap fees — not inflation.
The network pays you to hold private money.

**Tweet 3:**
Withdrawal is ring-signature protected.

An observer can't tell which commitment you spent.
The bank vault is a ring signature set.

**Tweet 4:**
Need liquidity before your CD matures?

Transfer it. XFG CDs are tradeable on the SwapXFG secondary market.
Yield with an exit.

**Tweet 5 (closer):**
Your XFG address begins with `fire`.

Every deposit. Every swap. Your keys.

Block 1,000,000 → azoraahai.

---

## Thread 4 — Launch Day (Block 1,000,000)

**Tweet 1 (standalone launch tweet):**
AzorAhai.

Block 1,000,000.

The fire has arrived. 🔥

**Tweet 2:**
What shipped:

→ SwapXFG: XMR/ETH/SOL ↔ XFG atomic swaps
→ XFG CDs: real yield, ring-sig withdrawal
→ OSPEAD + dynamic ring size 8–18
→ New commitment deposit transaction structure

**Tweet 3:**
Privacy upgrades: OSPEAD selects ring members that match real
spending patterns. Ring size scales 8–18 based on outputs available.

More cover. Harder to analyze. Ships now.

**Tweet 4:**
What v10 does NOT hide: XFG amounts are plaintext.

We value honesty over hype, and feel like leaving amounts public
while we adjust to new features is a trade we can handle- while community
can gain confidence in network supply numbers.
Hidden amounts are in development for v11. 

**Tweet 5:**
v11 coming after block 1M:

→ COLDAO Notes: deposit XFG, earn governance tokens
→ HEAT token: each token forever backed by burned XFG (ZK proven)

Not yet. But the foundation is here.

**Tweet 6 (closer):**
Fuego is open-source. No central authority.
Community governed. Your keys.

github.com/usexfg/fuego
discord.gg/5UJcJJg

The long night is over.
```

- [ ] **Step 2: Verify against brand compliance checklist**

Pay special attention: no deprecated terms, no overclaiming privacy, HEAT token framed correctly (v11 not v10), v10 honest limitations present in Thread 4.

- [ ] **Step 3: Commit and push**

```bash
git add docs/superpowers/content/azoraahai-twitter-threads.md
git commit -m "feat(content): add AzorAhai Twitter/X thread series

4 pre-written threads: teaser (block 999,900), SwapXFG demo
(~999,950), XFG CD explainer (~999,975), launch day (1,000,000).
Honest privacy limitations in launch thread.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
git push origin xfgCdswaps
```

---

### Task 7: Discord Announcements (Asset E)

**Files:**
- Create: `docs/superpowers/content/azoraahai-discord.md`

- [ ] **Step 1: Write both Discord messages**

Path: `docs/superpowers/content/azoraahai-discord.md`

```markdown
# AzorAhai Discord Announcements

---

## Message 1 — Countdown Pin (Block 999,900)

📌 **PIN THIS — AzorAhai is coming. Block 1,000,000.**

We're at block 999,900. Less than 100 blocks to go.

Here's what's shipping:

🔁 **SwapXFG** — atomic swaps with XMR, ETH, SOL. No exchange. No KYC.
💰 **XFG CDs** — commitment deposits earning real yield in XFG.
🔒 **OSPEAD + dynamic ring size 8–18** — better decoys, adaptive ring size.
📦 **New deposit structure** — upgraded commitment deposit transactions.

Block countdown: [link to explorer http://fuego.spaceportx.net]

Your address begins with fire. The fire arrives at block 1,000,000.

---

## Message 2 — Launch Day @everyone (Block 1,000,000)

@everyone

**AzorAhai. Block 1,000,000. The fire has arrived.**

What shipped:

→ **SwapXFG** is live — trustless atomic swaps, no exchange, no KYC.
  Pairs: XMR ↔ XFG · ETH ↔ XFG · SOL ↔ XFG · wXFG ↔ XFG
→ **XFG CDs** — lock XFG, earn yield funded by swap fees. Ring-sig withdrawal.
→ **OSPEAD + ring size 8–18** — adaptive decoy selection, max ring 18.
→ **New deposit transaction structure** — upgraded commitment deposit protocol.

What v10 does not hide: amounts are plaintext. We don't overclaim.
Hidden amounts are a v11 goal — roadmap is public.

📢 Full technical announcement: [BitcoinTalk link]
🐙 Release: github.com/usexfg/fuego
🌐 Website: usexfg.org

The long night is over. Start swapping. 🔥
```

- [ ] **Step 2: Verify against brand compliance checklist**

- [ ] **Step 3: Commit and push**

```bash
git add docs/superpowers/content/azoraahai-discord.md
git commit -m "feat(content): add AzorAhai Discord countdown + launch announcements

Countdown pin for block 999,900 and launch-day @everyone for block
1,000,000. Honest privacy limitations included in launch message.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
git push origin xfgCdswaps
```

---

## Chunk 3: Tier 2 + Infrastructure

### Task 8: "Why We Built SwapXFG" Blog Post (Asset F)

**Files:**
- Create: `docs/superpowers/content/why-we-built-swapxfg.md`
- Read: `docs/ATOMIC_SWAP_PLAN.md` (for technical background)

- [ ] **Step 1: Read ATOMIC_SWAP_PLAN.md**

Re-read `docs/ATOMIC_SWAP_PLAN.md` to ground the narrative in accurate technical claims.

- [ ] **Step 2: Write the long-form post**

Path: `docs/superpowers/content/why-we-built-swapxfg.md`

```markdown
# Why We Built SwapXFG

TradeOgre is gone. SideShift has limits. LocalMonero shut down.

Privacy coin holders are stranded. You can hold XFG. You can mine it.
You cannot easily trade it without routing through a KYC exchange, a
centralized aggregator, or a service that can freeze your funds. That's
not sovereignty. That's surveillance with extra steps.

We built SwapXFG because working software solves this. Not promises.
Not roadmaps. Software.

---

## Why Exchanges Fail Privacy Coins

Centralized exchanges require identity. Even the small ones eventually
do — through regulatory pressure, exit scams, or both. TradeOgre was
the best option privacy coin holders had. It's dead.

Decentralized exchanges on EVM chains work for ERC-20 tokens. They
don't work for native CryptoNote coins. You can't put XFG on Uniswap.
You can wrap it — wXFG on Ethereum — but wrapping adds a custody step
and breaks the privacy model.

For privacy coins to work as money, they need peer-to-peer exchange
infrastructure that matches their trust model. An atomic swap requires
no custodian, no KYC, no approval. Either the swap completes or it
doesn't. There is no middle state where someone else holds your funds.

---

## Atomic Swaps: How They Work

An atomic swap uses cryptographic time-locks and adaptor signatures to
ensure both parties receive what they agreed to — or neither party
loses anything. The key primitive is the adaptor signature: a
cryptographic commitment that makes two transactions atomically
dependent on a shared secret.

On the XFG side, all swap outputs are normal `KeyOutput` to Musig2
joint addresses. They are indistinguishable from regular XFG
transactions on-chain. Fuego's swap outputs do not look like swaps.

The counterparty chain uses its native mechanism: HashTimeLock contracts
on Ethereum and Solana, COMIT adaptor signatures on Monero.

---

## The Ed25519 Advantage: XMR ↔ XFG

The XMR ↔ XFG pair is special.

Both Monero and Fuego use Ed25519 and the CryptoNote protocol. This
means both sides can use COMIT adaptor signatures — no HTLC required
on either chain. The swap is cryptographically equivalent on both sides.
No contract. No timelock hash visible in a contract log. Both sides
look like normal transactions.

This is the most private swap pair available anywhere. XMR holders who
want to hold XFG — or earn yield on XFG via XFG CDs — can do so without
leaving a HTLC fingerprint on either chain.

---

## What SwapXFG Ships at AzorAhai

At block 1,000,000, SwapXFG is live with the following pairs:

- XMR ↔ XFG (COMIT adaptor sigs, both Ed25519)
- ETH ↔ XFG (HashedTimelock.sol + adaptor sig on XFG)
- SOL ↔ XFG (Solana HTLC + adaptor sig on XFG)
- wXFG ↔ XFG (ERC-20 HTLC + adaptor sig on XFG)
- LUSD ↔ XFG (ERC-20 HTLC + adaptor sig on XFG)

SwapXFG also hosts the XFG CD secondary market. Commitment deposit
holders can sell their position before maturity. Yield with an exit.

---

## The Broader Picture

Fuego is a decentralized blockchain bank with ledger deposits paying
interest, without involvement of legacy financial institutions, powered
by 100% open source code.

SwapXFG is the trading infrastructure that makes the bank useful.
Without a way to enter and exit XFG privately, yield doesn't matter.
Without yield, holding XFG is purely speculative.

The complete picture: private deposits earning real yield, exchangeable
with XMR and ETH without a custodian, governed by no central authority.

Working software is the primary measure of progress.

The fire has arrived.
```

- [ ] **Step 3: Verify against brand compliance checklist**

- [ ] **Step 4: Commit and push**

```bash
git add docs/superpowers/content/why-we-built-swapxfg.md
git commit -m "feat(content): add 'Why we built SwapXFG' long-form blog post

TradeOgre post-mortem, privacy coin trading vacuum, Ed25519 advantage,
atomic swap sovereignty narrative. Asset F (Tier 2, post-launch).

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
git push origin xfgCdswaps
```

---

### Task 9: Visual Brand Spec Verification + SEO Audit Prep (Assets J, K verification)

**Files:**
- Read: `docs/superpowers/brand-visual-spec.md` (verify completeness)
- Create: `docs/superpowers/content/seo-audit-brief.md` (brief for SEO audit skill)

- [ ] **Step 1: Verify visual brand spec is complete**

Read `docs/superpowers/brand-visual-spec.md`. Confirm:
- All 11 fire gradient stops match `swapxfg/app/styles.go` exactly
- All color constants present with hex values
- CSS gradient output is correct
- Design principles section present

If any values are missing or incorrect, fix them now.

- [ ] **Step 2: Write SEO audit brief**

Path: `docs/superpowers/content/seo-audit-brief.md`

```markdown
# SEO Audit Brief — AzorAhai Release

**Site:** usexfg.org
**Skill to invoke:** /marketing:seo-audit

## Target Keywords (priority order)

| Keyword | Intent | Why |
|---------|--------|-----|
| XMR atomic swap | Transactional | XMR holders with no exchange |
| privacy coin atomic swap | Informational | Broader privacy audience |
| private crypto yield | Informational | XFG CD audience |
| XFG CD | Brand | Direct product search |
| fuego cryptocurrency | Brand | Direct project search |
| CryptoNote atomic swap | Informational | Technical audience |
| no KYC crypto swap | Transactional | Privacy-first traders |
| decentralized crypto exchange privacy | Informational | Long-tail |

## Audit Scope

- On-page analysis: title tags, meta descriptions, H1/H2 structure
- Content gaps: pages that should exist for target keywords
- Technical: canonical URLs, sitemap, page speed basics
- Competitor comparison: Monero's atomic swap messaging (framing, not traffic)

## Notes for Auditor

- Do NOT position Fuego as Monero competitor. Different product category.
- SwapXFG is the headline. XFG CDs are secondary.
- Dark-first visual brand. Any screenshot analysis should note dark theme.
- AzorAhai release landing section needs to be SEO-optimized first.
```

- [ ] **Step 3: Commit and push**

```bash
git add docs/superpowers/content/seo-audit-brief.md
git commit -m "docs(brand): add SEO audit brief for AzorAhai release

Target keywords, audit scope, and brand positioning notes for
/marketing:seo-audit invocation post-launch.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
git push origin xfgCdswaps
```

---

## Plan Complete

**All 11 assets accounted for:**

| Asset | ID | Task | Tier |
|-------|----|------|------|
| Brand voice guidelines | A | Task 1 | 1 |
| README cleanup | B | Task 3 | 1 |
| BitcoinTalk announcement | C | Task 5 | 1 |
| Twitter threads | D | Task 6 | 1 |
| Discord announcements | E | Task 7 | 1 |
| Blog post | F | Task 8 | 2 |
| XFG CD product description | G | Task 4 | 2 |
| OSPEAD explainer | H | Task 4 | 2 |
| v11 roadmap teaser | I | Task 4 | 2 |
| SEO audit brief | J | Task 9 | 3 |
| Visual brand spec | K | Task 2 | 3 |

**Execution order:** Task 1 → Task 2 → Task 3 → Task 4 → Task 5 → Task 6 → Task 7 → Task 8 → Task 9

Commit + push after each task as specified.
