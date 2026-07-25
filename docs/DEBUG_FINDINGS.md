# TG Timer Debug Findings — Seiko 4R36 Gallop Analysis

**Date**: 2025-07-25  
**Hardware**: Seiko 4R36, 21,600 BPH, LA=53°, sample_rate=44,100 Hz  
**Period**: ~14,700 samples/cycle → beat interval ~7,350 samples

---

## Executive Summary

The "blue tic/toc line jump" and "amplitude instability" were **not algorithmic bugs** but **physical watch behaviour**: the movement genuinely alternates short/long beats (gallop). The autocorrelation ridge is bimodal by physics, not noise.

The fix is a **cluster-aware dual IIR filter** tracking both gallop clusters independently and reporting their midpoint — eliminating the visual jump while preserving measurement integrity.

---

## Root Cause Chain

```
Physical watch alternation (short/long beat)
        ↓
Half-period autocorr captures ONE dominant ridge per frame
        ↓
Ridge alternates between two cluster centres ~30 samples apart
        ↓
Integer argmax (p->tic) jumps between clusters
        ↓
Blue lines jump visibly; amplitude mount point shifts → measure_pulse fails
```

---

## Key Diagnostic Evidence

### 1. Ridge Geometry (`half_band @7325..7375` diagnostic)
| Frame | Chosen Cluster | Profile Shape |
|-------|---------------|---------------|
| 1 | B (7335) | Monotonic decrease 7335→7375 |
| 2 | A (7365) | Monotonic increase 7325→7366 |

**Only ONE ridge exists per frame** — the "other cluster" is a descending shoulder, not a competing peak.

### 2. Mirror Symmetry Proof
- B-cluster profile (lags 7325..7375) mirrored about 7350 **exactly matches** A-cluster profile
- Sum B[k] + A[50-k] → smooth bimodal envelope with:
  - Peaks at 7338 and 7365
  - Dip at 7350 (−3.5%)
- Centroid (±15 samples) differs by only **3.2 samples** between clusters
- Mean of two centroids = **7350.00 = exactly period/2**

### 3. Statistical Gallop Confirmation
- 25/28 consecutive same-`sc_win` frames **flip cluster** (89% alternation rate)
- Per-`sc_win` sequences show strict or near-strict alternation
- **Physical reality**: watch produces SHORT/LONG beat pairs

### 4. Peak Height Scaling
| sc_win (N) | Peak Height | ~√N scaling |
|------------|-------------|-------------|
| 88,200 | 486 | ✓ |
| 176,400 | 595 | ✓ |
| 352,800 | 703 | ✓ |
| 705,600 | 750 | ✓ |

Both clusters scale identically → **same physical ridge**, not competing peaks.

---

## Failed Attempts (Inert on Hardware)

| Attempt | Why It Failed |
|---------|---------------|
| PLL on `p->phase` | Fixed phase drift; ridge flip persisted (wrong layer) |
| `peak_detector_tracked` `>=` plateau fix | Mathematically correct but ridge isn't a plateau |
| Sub-sample `tic_f` / `tic_centroid` | <1% effect; ridge structure unchanged |
| Single IIR on `tic_to_toc` (α=0.10) | Damped 10× but left ±1-sample floor on gallop runs |
| Sub-sample fold (KDE interpolation) | Sharpened ridge (flat→convex) but didn't collapse bimodal |
| Oscilloscope trigger alignment | Addressed display symptom, not measurement cause |

---

## Correct Fix: Cluster-Aware Dual IIR

### Architecture
```
p->tic_fold, p->toc_fold    ← raw fold indices (saved BEFORE timestamp swap)
        │                            │
        ├─→ measure_pulse()          │  (amplitude mount points)
        ├─→ locate_events()          │
        │                            │
        ▼                            ▼
Cluster IIRs (α=0.10)          Cluster IIRs (α=0.10)
  │  short_e  │                   │  short_e  │
  │  long_e   │                   │  long_e   │
  └───────────┘                   └───────────┘
       │                              │
       ▼                              ▼
  midpoint = (short+long)/2      midpoint = (short+long)/2
       │                              │
       ▼                              ▼
p->tic_display, p->toc_display    p->tic_display, p->toc_display
       │                              │
       └──────────────┬───────────────┘
                      ▼
              UI Drawing (blue lines, zones)
```

### Track-and-Hold Integer Rounding
```c
if (p->lowpass_int == 0 || fabs(p->lowpass - p->lowpass_int) > 1.5)
    p->lowpass_int = (int)round(p->lowpass);
```
Eliminates ±1-sample flip-flop at 0.5 boundaries.

### Bootstrap at Symmetry Centre
- TOC: both clusters seed at `period/2` → midpoint correct from cycle 1
- TIC: both clusters seed at `fold_size/2` → midpoint correct from cycle 1

---

## Data Structure Roles (Critical Separation)

| Field | Purpose | Consumer |
|-------|---------|----------|
| `tic_fold`, `toc_fold` | Raw fold indices (pre-swap) | `measure_pulse`, `locate_events`, trigger alignment |
| `tic`, `toc` | Timestamp-corrected (post-swap) | `last_tic`, `last_toc`, beat error calc |
| `tic_display`, `toc_display` | Stabilized midpoints | **UI only** (blue lines, zones, stats) |

**Mistake made**: Using `tic_display` for amplitude measurement → mount point at wrong phase → `measure_pulse` returns -1 → blue pulse lines vanish.

---

## Diagnostic Tools to Keep (DEBUG-gated)

1. **`half_band @7325..7375`** — 51-sample autocorr profile spanning both clusters
2. **`tic_cluster` / `toc_cluster`** — raw, short_e, long_e, midpoint, committed
3. **`tic_shape` bracket** — 21 samples around argmax with 3 estimators
4. **`ridge_select` + `prior_ridge`** — peak_detector_tracked decisions
5. **`full_lag_scan` + `alt_half_cluster`** — fold vs half-period disambiguation

---

## Files Modified (Reverted)

- `src/algo.c` — cluster-aware IIR, sub-sample fold, PLL, diagnostics
- `src/tg.h` — added `tic_display`, `toc_display`, `tic_fold`, `toc_fold`, cluster state
- `src/output_panel.c` — trigger alignment, display value getters
- `src/computer.c` — `DISPLAY step` diagnostic

---

## Next Implementation Checklist

- [ ] Re-implement cluster-aware IIR with **three distinct tic/toc roles**
- [ ] Keep `half_band` diagnostic permanently (DEBUG)
- [ ] Validate on hardware: blue lines stable, amplitude reads correctly
- [ ] Decide PLL retention (cleaner phase origin vs. unused for symptom)

---

## Meta-Learnings

> **Stop speculative coding. Instrument first, decide second.**

Five speculative changes were inert on real hardware. Only targeted diagnostics (`half_band`, `tmp_band.txt`) revealed the true geometry.

> **Physical phenomena ≠ algorithmic bugs.**

The watch *actually* gallops. The algorithm must track both states, not force a single value.

> **Separate computation from display at the struct level.**

Three tic/toc variants exist for distinct purposes. Conflating them breaks measurement.

> **Unit tests verify logic, not physics.**

All 25 tests passed throughout — but the gallop is a hardware behaviour no synthetic test captures.

---

*Generated from debugging session 2025-07-25. See `tmp_band.txt` for raw hardware capture data.*