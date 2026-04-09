# AzorAhai Release — Brand Voice & Campaign Design Spec

**Date:** 2026-04-08  
**Branch:** xfgCdswaps  
**Status:** Awaiting implementation  

---

## 1. Context & Objectives

Fuego (XFG) is approaching block 1,000,000 — the AzorAhai Release. This is the first time the project ships a complete sovereign money stack: trustless atomic swaps (SwapXFG), real-yield deposits (XFG CDs), and meaningful privacy upgrades (OSPEAD + dynamic ring size 8–18). The project has never had a formal brand voice, and the upcoming features have real consumer appeal beyond the existing technical community.

**Primary objective:** Drive awareness and adoption of SwapXFG among privacy coin holders who lost access to TradeOgre and similar exchanges.  
**Secondary objective:** Onboard yield-seeking privacy users to XFG CDs.  
**Tertiary objective:** Establish a reusable brand foundation for v11 (COLDAO Notes, HEAT token).

### What is NOT in scope
- EFiers / Elderfiers (deprecated — replaced by ZK proofs of Merkle trees)
- COLD deposit (deprecated CLI command — superseded by XFG CDs)
- Burn2Mint (dropped)
- HEAT token as a v10 feature (v11 only — but HEAT burn deposits DO still function as permanent decoys in the v10 commitment pool)
- COLDAO as a v10 feature (v11 only)

---

## 2. Brand Voice Guidelines

### 2.1 Identity

| Element | Value |
|---------|-------|
| Project name | Fuego |
| Ticker | XFG |
| Address prefix | `fire` (all addresses begin with "fire") |
| Daemon binary | `fuegod` |
| Release name | AzorAhai |
| Release tagline | "The Fire Has Arrived." |
| Pre-launch tagline | "The Long Night Is Almost Over." |
| Mission statement | "A decentralized blockchain bank with ledger deposits paying interest, without involvement of legacy financial institutions, powered by 100% open source code." |
| Builder philosophy | "Working software is the primary measure of progress." |

### 2.2 The Fire Metaphor

The fire identity runs through every layer of the project: the `fire` address prefix, Fuego orange (`#FF5500`), the fire gradient in SwapXFG, upgrade names (Dracarys, Godflame), the diamond-with-fire splash, and the GoT mythology (AzorAhai = the hero who ends the Long Night with fire).

**Use the metaphor for:** Release naming, launch copy tone, visual identity anchoring.  
**Do not force it:** Technical documentation does not need fire metaphors. Let it appear naturally.

### 2.3 Visual Brand Direction

**Primary palette:**

| Role | Value | Usage |
|------|-------|-------|
| Background / base | `#000000` — pure black | All surfaces. The brand is dark-first. No light mode. |
| Near-black (depth) | `#0A0000` – `#180400` | Cards, panels, elevated surfaces — fire-tinted black |
| Highlight / accent | `#FF5500` — Fuego orange | CTAs, active states, key data, fire icon fills |
| Highlight mid | `#CC3300` | Hover states, secondary accents |
| Text primary | `#FFFFFF` | On black backgrounds |
| Text secondary | `#999999` | Supporting copy, metadata |
| Fire gradient (11-stop) | `#180400` → `#FF5500` → `#FFFADD` | Flame animation, splash screen, hero backgrounds |

**Design principle:** Black is the night. Orange is the fire. Every composition is dark canvas + fire accent. No grey-wash neutrals as primaries. No white backgrounds. High contrast at all times — the fire must be visible.

**Application rules:**
- Interactive elements (buttons, links, CTAs): black background + orange text or orange border
- Data highlights, amounts, metrics: Fuego orange
- Error / warning states: orange (not red — stays on-brand)
- The diamond logo: always rendered on black; fire gradient fill or white outline only
- Never use orange as a background fill behind body text — contrast fails

### 2.4 Voice Attributes

**1. Technically honest**  
State limitations plainly, in the same breath as capabilities. Never overclaim privacy. If amounts are plaintext in v10, say so. This is Fuego's trust differentiator.

> ✅ "What v10 does not hide: Amounts are plaintext."  
> ❌ "Fuego provides complete transaction privacy."

**2. Direct, zero inflation**  
Short declarative sentences. Present tense. Active voice. No hedging, no qualifiers, no marketing filler.

> ✅ "TradeOgre is dead. Atomic swaps let users trade trustlessly, no exchange needed."  
> ❌ "We are excited to announce a revolutionary new solution for decentralized trading."

**3. Builder ethos**  
Ship first, explain after. Working software over announcements. Hype is a liability.

> ✅ "SwapXFG is live. XMR ↔ XFG. No exchange. No KYC."  
> ❌ "We are working hard to bring you an exciting new product very soon."

**4. Sovereign money, no intermediaries**  
Frame every feature through the lens of individual sovereignty — not technology for its own sake. The audience already knows why surveillance finance is a threat.

> ✅ "Your XFG address begins with fire. Every address does. Only you control the keys."  
> ❌ "Fuego uses advanced cryptographic techniques to ensure user privacy."

### 2.5 Audience Personas

**Persona A — Technical Privacy User**  
- Runs a node, reads changelogs, understands ring signatures and CryptoNote
- Holds XMR, possibly BCH, knows what CLSAG is
- Lost their TradeOgre/SideShift access; wants peer-to-peer swap infrastructure
- Needs: Technical accuracy, protocol specs, honest privacy comparisons
- Tone: Peer-to-peer technical. Tables, specs, protocol version numbers are welcome.

**Persona B — Non-Technical Privacy Seeker**  
- Wants yield without a bank, wants to swap without an exchange
- Does not need to know what OSPEAD is, but should be able to understand "ring size 8–18 means more cover for your transaction"
- Drawn to sovereignty narrative and fire mythology
- Needs: Plain-language product descriptions, benefit-before-mechanism framing
- Tone: Direct, warm, no jargon unless immediately explained.

### 2.6 Messaging Pillars (ranked for AzorAhai launch)

1. **Trustless exchange** — SwapXFG leads. XMR ↔ XFG, ETH ↔ XFG, no exchange, no KYC. This is the headline.
2. **Real yield, no bank** — XFG CDs. Earn interest on private money, funded by swap fees (not inflation), withdrawal ring-signature protected.
3. **Honest privacy, proven roadmap** — OSPEAD + dynamic ring size 8–18. What changed, what it means, what v10 still does not hide.
4. **Sovereign money** — No central authority. Open-source. Community-governed. Your keys, your coins.
5. **Roadmap credibility** — v11 teaser: COLDAO Notes (deposit XFG, earn COLDAO governance tokens as PIK yield), HEAT token (each token forever collateralized by ZK-proven burned XFG). Coming after block 1M.

### 2.7 Tone by Channel

| Channel | Tone | Notes |
|---------|------|-------|
| Twitter/X | Punchy, ≤2 sentences per tweet. Blunt market commentary. | Lead with the fact, not the explanation. |
| Discord | Warmer, "we" voice, invites participation. | Use community framing; link to technical docs. |
| BitcoinTalk | Technical, structured. Tables, spec comparisons, protocol version numbers. | Technical audience expects full detail. |
| GitHub / docs | Declarative, present tense, never hedges. | No marketing language in code comments or docs. |
| Press / announcement | Lead with product story. Fire mythology as opener and closer. | AzorAhai narrative frame. |
| Reddit (r/xmrtrader, r/privacy) | Technical but accessible. Acknowledge Monero comparison honestly. | Do not position as "Monero killer." |

### 2.8 Terminology Bible

**Canonical — always use:**

| Term | Usage |
|------|-------|
| Fuego | Project name, always capitalized |
| XFG | Ticker, always uppercase |
| SwapXFG | Atomic swap application name |
| XFG CD | Consumer product name for commitment deposit (Certificate of Deposit) |
| commitment deposit | Technical/protocol term for the on-chain deposit structure |
| FuCIA | Internal code designation: "Fuego Untraceable Custom Interest Assets." Not required in public copy, but may be surfaced as a product concept. |
| COLDAO Note | v11 governance bond product. Deposit XFG, earn COLDAO governance tokens as yield (PIK structure). |
| COLDAO | v11 governance token. Inflation-funded. Not a v10 feature. |
| HEAT | v11 token. Each token permanently collateralized by ZK-proven burned XFG. Not a v10 consumer product. |
| HEAT burn deposit | v10 feature: burned XFG permanently seeded into the commitment decoy pool. Distinct from HEAT token. |
| OSPEAD | Adaptive decoy selection algorithm. Always capitalize as acronym. |
| ring size 8–18 | Dynamic ring size range for public copy. Use total ring size, not mixin count. |
| CommitmentI/O | Internal technical term for deposit transaction structure. Appears in protocol-level docs only — not used in any campaign asset or user-facing copy. |
| Dynamigo | Internal protocol version name for block 999,999 upgrade. |
| AzorAhai | Public release name for block 1,000,000 milestone. |
| fuegod | Daemon binary name. |
| fire | Address prefix. All Fuego addresses begin with "fire." |
| DMWDA | Difficulty algorithm (Dynamic Multi-Window Difficulty Algorithm). Internal/technical use. |

**Deprecated — never use in public copy:**

| Term | Replacement |
|------|-------------|
| Elderfier / EFier | ZK proofs of Merkle trees |
| COLD deposit | XFG CD / commitment deposit |
| Burn2Mint | (dropped entirely) |
| mixin 8 (as public description) | ring size 8–18 |

**v11 roadmap — mention as future only:**

| Term | Note |
|------|------|
| COLDAO Note | v11. Governance bond. PIK structure. |
| COLDAO token | v11. Inflationary governance token. |
| HEAT token | v11. ZK-proven burn collateral, inflation-minted. |

### 2.9 What to Avoid

- Direct Monero comparisons (Fuego is a different product category — banking primitives + swap infrastructure built on CryptoNote, not a Monero competitor)
- Overclaiming privacy: v10 amounts are plaintext; hidden amounts are a v11 goal
- Marketing inflation: "revolutionary," "game-changing," "excited to announce"
- Forcing the fire metaphor into technical documentation
- Any reference to EFiers, Burn2Mint, or COLD deposit in user-facing copy
- Describing HEAT token or COLDAO as v10 features

---

## 3. AzorAhai Campaign Plan

### 3.1 Campaign Brief

| Element | Value |
|---------|-------|
| Campaign name | AzorAhai — Block 1,000,000 |
| Release tagline | "The Fire Has Arrived." |
| Primary KPI | 50 completed atomic swaps within 30 days |
| Secondary KPI | 10% of circulating supply in XFG CDs within 60 days (circulating supply figure to be sourced from block explorer at time of launch: http://fuego.spaceportx.net or https://explore-xfg.loudmining.com) |
| Community KPI | +500 Discord members from XMR/privacy crossover within 60 days |
| SEO KPI | usexfg.org top 10 for "XMR atomic swap" within 90 days |

### 3.2 Audience Segments & Messaging

| Segment | Hook | Channel priority |
|---------|------|-----------------|
| XMR/privacy coin holders | "XMR ↔ XFG atomic swap. No exchange. No KYC. Both Ed25519." | r/xmrtrader, Twitter, BitcoinTalk |
| Existing XFG holders | "Your XFG now earns real yield and swaps trustlessly." | Discord, Twitter |
| Cypherpunk/sovereign money | "A decentralized bank that pays you to hold private money." | BitcoinTalk, Reddit r/privacy |

### 3.3 Channel Strategy

| Channel | Owner | Cadence | Format |
|---------|-------|---------|--------|
| Twitter/X (@useXFG) | Marketing | 3 pre-launch + 1 launch day | Threads |
| Discord | Community | Countdown pin + day-of @everyone | Announcement |
| BitcoinTalk | Core team | 1 full technical post on launch day | Forum post |
| GitHub | Engineering | README update + release tag | Markdown |
| usexfg.org | Web | AzorAhai landing section | SEO-optimized web copy |
| Reddit | Community | Launch day cross-post | r/xmrtrader, r/privacy |
| Medium/blog | Core team | Within 2 weeks post-launch | Long-form |

### 3.4 Content Calendar

**Phase 1 — Pre-launch (block 999,900 → 999,999)**

| Asset | Channel | Timing | Notes |
|-------|---------|--------|-------|
| "The long night is almost over. Block 999,900." | Twitter | Block 999,900 | Single tweet, no explanation needed |
| SwapXFG demo thread | Twitter | Block ~999,950 | XMR ↔ XFG walkthrough, step-by-step |
| XFG CD explainer thread | Twitter | Block ~999,975 | "Earn yield on private money. No bank." |
| Countdown pinned message | Discord | Block 999,900 | Feature list + block countdown |

**Phase 2 — Launch Day (block 1,000,000)**

| Asset | Channel | Notes |
|-------|---------|-------|
| "AzorAhai. Block 1,000,000. The fire has arrived." | Twitter | Launch tweet + changelog thread |
| Full technical announcement | BitcoinTalk | All specs, protocol changelog table, OSPEAD explainer |
| README.md update | GitHub | Remove stale refs, add AzorAhai release section |
| Release tag | GitHub | v10 / AzorAhai tag |
| @everyone announcement | Discord | Full feature summary + links |

**Phase 3 — Post-launch (blocks 1,000,001+)**

| Asset | Channel | Timing | Notes |
|-------|---------|--------|-------|
| "Why we built SwapXFG" | Medium | Week 1–2 | TradeOgre post-mortem, sovereignty narrative |
| r/xmrtrader cross-post | Reddit | Week 1 | "XMR ↔ XFG atomic swap is live. No exchange." |
| First-swap social proof | Twitter | Week 1–2 | Screenshot/testimonial format |
| SEO audit + optimization | usexfg.org | Week 2–4 | Target: "XMR atomic swap", "private crypto yield", "XFG CD" |
| v11 teaser | Twitter + Discord | Week 3–4 | Approved framing: "COLDAO Notes let you deposit XFG and earn COLDAO governance tokens. HEAT tokens are each forever backed by burned XFG, proven on-chain. Both coming after block 1M." |

---

## 4. Content Asset Deliverables

### Tier 1 — Must ship at launch

| Asset | ID | Owner | Description |
|-------|----|-------|-------------|
| Brand voice guidelines | A | Core team | Canonical file: `.claude/brand-voice-guidelines.md`. Copy (kept in sync): `docs/superpowers/brand-voice-guidelines.md`. The `.claude/` version is canonical; the `docs/` copy is for repo visibility and should be updated whenever the canonical changes. |
| Updated README.md | B | Engineering | Remove stale refs; add AzorAhai release section |
| BitcoinTalk announcement | C | Core team | Full technical post: specs, changelog table, OSPEAD plain-language, fire opener |
| Twitter thread series | D | Marketing | 4 pre-written threads (teaser, SwapXFG demo, XFG CD explainer, launch day) |
| Discord announcement | E | Community | Countdown pin + launch day @everyone |

### Tier 2 — Ship within 2 weeks post-launch

| Asset | ID | Owner | Description |
|-------|----|-------|-------------|
| "Why we built SwapXFG" blog post | F | Core team | Long-form: TradeOgre failure, privacy vacuum, Ed25519 advantage |
| XFG CD product description | G | Marketing | 150 words, plain-language: yield mechanism, secondary market, private withdrawal |
| OSPEAD plain-language paragraph | H | Engineering | 1 paragraph for README + website privacy section |
| v11 roadmap teaser | I | Core team | 1 paragraph, future tense only. Approved framing for HEAT: "each token forever collateralized by ZK-proven burned XFG." Approved framing for COLDAO Note: "deposit XFG, earn COLDAO governance tokens as yield (PIK structure)." |

### Tier 3 — Supporting infrastructure

| Asset | ID | Owner | Description |
|-------|----|-------|-------------|
| SEO audit | J | Marketing / Web | `/marketing:seo-audit` on usexfg.org; keywords: "XMR atomic swap", "private crypto yield", "XFG CD", "fuego cryptocurrency" |
| Visual brand spec | K | Engineering | Output: markdown swatches table saved to `docs/superpowers/brand-visual-spec.md`. Content: pure black `#000000` base, Fuego orange `#FF5500` accent, full 11-stop fire gradient (from styles.go), fire-tinted near-blacks for depth layers, diamond geometry notes (from splash.go), typography defaults. Design principle: dark-first, black canvas + orange fire. Consumer: web team + social media templates. |

---

## 5. Implementation Sequence

The writing-plans skill will sequence this. Suggested order:

1. Brand voice guidelines doc (Asset A) — foundation for everything
2. README.md cleanup (Asset B) — remove stale refs first, unblocks all other copy
3. Tier 1 content assets (C, D, E) — launch-critical
4. Tier 2 assets (F, G, H, I) — post-launch
5. Tier 3 infrastructure (J, K) — ongoing

Commit + push after each handoff as specified by user.

---

## 6. Open Questions (Resolved)

| Question | Answer |
|----------|--------|
| Release name | AzorAhai (public), Dynamigo (internal protocol v10 name at block 999,999) |
| Product naming hierarchy | XFG CD (consumer) → commitment deposit (protocol) → FuCIA (internal code) |
| COLDAO product name | COLDAO Note (PIK/governance bond structure — deposit XFG, earn COLDAO governance tokens) |
| HEAT status | HEAT burn deposits = v10 decoy pool feature (active). HEAT token = v11 (ZK-proven burn collateral). |
| Lead feature | SwapXFG leads. CDs and privacy upgrades support. |
| Competitive framing vs Monero | Different product category — banking primitives + swap infra on CryptoNote. Not a Monero competitor. |
| "The Long Night Is Coming" tagline | Flip to "The Fire Has Arrived." at block 1,000,000. |
| Fire address prefix | Intentional brand touch — highlight in user-facing copy. |
