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
| HEAT commitment burn | The on-chain XFG burn mechanism. Burning XFG creates a `HEAT_commitment` in `tx_extra` — permanently unspendable, so it pads the commitment decoy pool forever. This is a v10 feature. |
| CommitmentI/O | Protocol-level docs only. Not in campaign copy. |
| Dynamigo | Internal protocol version name (block 999,999 upgrade) |
| AzorAhai | Public release name (block 1,000,000) |
| COLDAO Note | v11: deposit XFG, earn COLDAO governance tokens (PIK structure) |
| HEAT token (ERC-20) | v11. Minted on Ethereum only after ZK proof of on-chain `HEAT_commitment` burn is verified by prover contract. Each token forever collateralized by permanently destroyed XFG. Requires commitment Merkle proof generation. |
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
- HEAT token (ERC-20); HEAT commitment burns are v10 (different thing)

---

## What to Avoid

- Direct Monero comparison (different product category — banking + swap infra on CryptoNote)
- Overclaiming privacy (v10 amounts are plaintext)
- Marketing inflation: "revolutionary," "game-changing," "excited to announce"
- Fire metaphor forced into technical documentation
- EFiers, Burn2Mint, COLD deposit in any user-facing copy
- HEAT token or COLDAO described as v10 features
- CommitmentI/O in user-facing copy (protocol-level only)
