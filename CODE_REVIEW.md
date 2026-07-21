# tg-timer Code Review

**Scope:** Full source review of `src/{tg.h,algo.c,audio.c,computer.c,interface.c,output_panel.c,serializer.c,config.c}` after the 20-issue fix batch.

Findings are sorted by **severity**. Each finding lists the file/line, the problem, why it matters, and a minimal fix.

---

## Critical (correctness bugs that produce wrong output today)

### C1. `pb_clone` does not copy `amp_fail_reason` (real, observable bug)
**File:** `src/algo.c:151-172` (`pb_clone`)

`pb_clone()` copies ~15 fields (`period`, `sigma`, `be`, `amp`, `tic`, `toc`, `tic_pulse`, `toc_pulse`, `waveform_max`, `ready`, `timestamp`, …) but omits `amp_fail_reason`. The cloned `pb` is allocated with `malloc()` (uninitialised), so `pb->amp_fail_reason` holds garbage.

The pipeline then does (in `compute_results`, `computer.c:215`):
```c
s->amp_fail_reason = s->pb->amp_fail_reason;   // reads garbage
if (s->amp < 135 || s->amp > 360) {
    s->amp = 0;
    if (s->amp_fail_reason == AMP_OK)          // garbage != AMP_OK(0) usually
        s->amp_fail_reason = AMP_TIC_TOC_OUT_OF_RANGE;
}
```
Since the clamp override only fires when the garbage equals `AMP_OK` (enum value 0), the tooltip will usually display **whatever random enum value malloc returned** — e.g. "Period estimation failed" when the real reason was "amplitude outside valid range". This is exactly the diagnostic the v0.8.0 amplitude feature was added to surface.

**Fix:**
```c
new->ready = p->ready;
new->amp_fail_reason = p->amp_fail_reason;   // ADD THIS
new->timestamp = p->timestamp;
```

Also consider copying `phase`, `waveform_max_i`, `last_tic`, `last_toc`, `cal_phase` if any display path needs them — they're currently dropped too (see C2).

---

### C2. `pb_clone` misses fields that downstream code uses
**File:** `src/algo.c:151-172`

Beyond C1, `pb_clone` does not copy: `phase`, `waveform_max_i`, `last_tic`, `last_toc`, `cal_phase`, `events_from`, `timestamp` (wait — `timestamp` is copied). Of these:
- `waveform_max_i` is read in `add_sample_cal` but only for the calibration `pb` (never cloned).
- `last_tic`/`last_toc` are read on the **processing buffers** in `compute_parameters`/`locate_events` (before clone) — not on a clone.
- `phase` is read on `pb->phase` indirectly (via `do_locate_events` offsets) — also before clone.

So in practice only **`amp_fail_reason`** is observably wrong (C1). The others are latent risk: any future code that reads these on a cloned display snapshot would see garbage. Recommend copying the whole scalar block to be safe.

---

### C3. Dangling-else / mis-braced block in `compute_amplitude`
**File:** `src/algo.c:836-842`

```c
} else
    if(!(135 < tic_amp && tic_amp < 360 && 135 < toc_amp && toc_amp < 360))
            p->amp_fail_reason = AMP_TIC_TOC_OUT_OF_RANGE;
            else
            p->amp_fail_reason = AMP_TIC_TOC_DIFF_TOO_LARGE;
            debug("amp rejected\n");
```

Despite the indentation, C parses this as:
```c
} else
    if(...out_of_range...)
        p->amp_fail_reason = AMP_TIC_TOC_OUT_OF_RANGE;
    else
        p->amp_fail_reason = AMP_TIC_TOC_DIFF_TOO_LARGE;
debug("amp rejected\n");   // runs EVERY loop iteration, unconditionally
```

So:
1. `debug("amp rejected\n")` runs on **every** iteration of the `while` loop, not just when amp is rejected. In DEBUG builds this floods the log with misleading "amp rejected" lines (including iterations that ultimately *succeed*).
2. The `AMP_TIC_TOC_DIFF_TOO_LARGE` branch only triggers when tic_amp/toc_amp are individually valid but differ by ≥60°. The inner `else` binds correctly. So fail-reason *assignment* is actually correct here — the bug is purely the runaway `debug` line plus the misleading indentation.

**Fix:** add braces so the code matches intent:
```c
} else {
    if(!(135 < tic_amp && tic_amp < 360 && 135 < toc_amp && toc_amp < 360)) {
        p->amp_fail_reason = AMP_TIC_TOC_OUT_OF_RANGE;
    } else {
        p->amp_fail_reason = AMP_TIC_TOC_DIFF_TOO_LARGE;
    }
    debug("amp rejected\n");
}
```

This is a real bug, not a style nit — the `-Wextra` flag has nothing to warn against here because the code parses unambiguously wrong.

---

### C4. Serializer round-trip loses windowed-stats history for loaded snapshots
**File:** `src/serializer.c:357-491` (`serialize_snapshot` / `scan_snapshot`)

`serialize_snapshot` writes lots of fields but **never** writes `rate_hist`, `be_hist`, `amp_hist`, `hist_time`, `hist_count`, `hist_wp`, `hist_max`. `scan_snapshot` does `memset(*s, 0, ...)`, leaving all of them NULL/0.

Result: any loaded `.tgj` snapshot shows "collecting data..." in the stats panel forever — even though the snapshot contains a perfectly good single-shot rate/BE/amp. The tooltips for rate/BE/amp still work (they read `s->rate`/`s->be`/`s->amp`), but the windowed-stats strip stays empty.

Two acceptable fixes:
- **Minimal** — treat loaded snapshots as "stats not available": set `hist_count = 0` (already happens via memset), and have the stats panel detect this *separately* from `hist_count == 0`-while-live, so it can show the single-shot values instead of "collecting data".
- **Proper** — serialise the four history arrays (`rate_hist`, `be_hist`, `amp_hist`, `hist_time`) tagged by `hist_count`/`hist_wp`/`hist_max`, then on load decide a sensible window-using est value.

The minimal fix is recommended for now.

---

## Attribution: pre-existing vs introduced by the fix batch

This appendix cross-references each finding against git history (baseline
`f77cb49`, "feat: windowed averaging stats, amplitude failure diagnostics,
UI improvements") to distinguish bugs that were already in the codebase from
those introduced by the 20-issue fix batch (the uncommitted `git diff HEAD`).

### Introduced by the fix batch (mine to own)

| ID | Where introduced | Why |
|----|------------------|-----|
| **M6** | `algo.c` `peak_detector` | I changed the stack VLA to `malloc`/`free`. The malloc-on-hot-path note is a direct consequence. |
| **M8** | `algo.c` `mean_less_two_greatests` | I added the `if(n < 3) return 0;` guard (Issue 2). The "return 0 hides upstream bugs" note is mine. |
| **M3** | `output_panel.c` `compute_window_stats` | I rewrote the function to use `hist_time` (Issue 1) instead of the original beat-count estimate. The "relies on monotonic timestamps" note describes my code. |
| **N3** | `tg.h` `last_refresh_ts` + `interface.c` | I added the field and two assignments (intended to wire a `kick_computer` throttle for Issue 11). The throttle edit failed to apply, so the field is **dead code I introduced** — never read. |
| **(part of H7)** | `computer.c` `start_computer` error path | I added `hist_time = calloc(...)` as a 4th allocation. The error path already leaked the first three (`rate_hist`/`be_hist`/`amp_hist`, introduced in f77cb49); my change adds a 4th leaked array under the same buggy cleanup. |

### Pre-existing in `f77cb49` (or earlier); not touched by the fix batch

| ID | Where it lives | Why I didn't touch it |
|----|----------------|-----------------------|
| **C1** | `algo.c` `pb_clone` | `pb_clone` is byte-identical between f77cb49 and my diff. The field `amp_fail_reason` + the read `s->amp_fail_reason = s->pb->amp_fail_reason` (in `compute_results`) were both **introduced by f77cb49**; the omission from `pb_clone` has been there since the field was created. My only change to `compute_results` was adding the clamp-override — that reads the garbage, it doesn't create it. |
| **C2** | `algo.c` `pb_clone` | Same as C1 — `pb_clone` never copied `phase`/`waveform_max_i`/`last_tic`/`last_toc`/`cal_phase`; long-standing. Only `amp_fail_reason` is observably wrong because it's the only one newly read. |
| **C3** | `algo.c` `compute_amplitude` else-block | The dangling-else + `debug("amp rejected\n")` exists verbatim in f77cb49 (lines 827-832 there). My diff does not touch `compute_amplitude`. |
| **C4** | `serializer.c` | I never touched `serializer.c` (no diff). The `hist_*` fields were added to the snapshot in f77cb49 without being serialized — the gap is as old as the feature. |
| **C5** | `computer.c` `compute_update` | The `signal = analyze_pa_data(...)` + `i == NSTEPS-1 && p[i].amp < 0 ? signal-1 : signal` logic is unchanged from f77cb49 — core DSP I didn't touch. |
| **H1** | `algo.c` `compute_phase` | `p->waveform[i] /= j;` exists in f77cb49 untouched. |
| **H2** | `audio.c` `analyze_pa_data_cal` | The unbounded `for(j=0; ...; j++);` exists in f77cb49 untouched. |
| **H3** | `algo.c` `smooth` / callers | `smooth()` is byte-identical to f77cb49. My `compute_parameters` change added a `fold_size <= window` guard (Issue 6) but didn't touch `smooth` itself. |
| **H4** | `algo.c` `noise_suppressor` | Untouched — the aliasing comment is about pre-existing buffer arithmetic. |
| **H5** | `computer.c` `computer_destroy` | The `pthread_mutex_destroy`/`cond_destroy`-then-`pthread_join` order exists in f77cb49 untouched. |
| **H6** | `serializer.c` | Untouched (I don't diff `serializer.c`). |
| **H8** | `algo.c` `pb_clone` waveform malloc | `pb_clone` is unchanged from f77cb49; the `malloc(sample_count * sizeof(float))` with no `sample_count > 0` guard predates me. |
| **M1** | `computer.c` `start_computer` | `s->hist_max = 36000;` is in f77cb49 (introduced by the feature). My change added `hist_time` allocation alongside the existing three. |
| **M2** | `computer.c` `snapshot_clone` | The "deep-copy the entire ring" pattern (4 `memcpy`s of `hist_max` entries) existed in f77cb49 with the same shape (the original did 3, mine adds a 4th for `hist_time`). The pattern itself is pre-existing. |
| **M4** | `output_panel.c` drawing functions | `draw_graph`, `period_draw_event`, `paperstrip_draw_event` pre-existing; my diff only touches `compute_window_stats`, `stats_draw_event`, `output_draw_event`, `amplitude_to_time`, `handle_copy_stats`, `output_query_tooltip`. The period-division guards in `draw_graph`/`period_draw_event` are pre-existing. |
| **M5** | `algo.c` `vmax` | Untouched: `float max = v[a];` with no bounds check is original. |
| **M7** | `algo.c` `tmean` | Untouched; the `quickselect`-mutates-`x` contract predates me. |
| **M9** | `interface.c` `start_interface` | `real_sr` from PortAudio is pre-existing behaviour; the display strings are pre-existing. |
| **L1–L10** | various | All cosmetic, pre-existing. |

### Summary numbers

- **Pre-existing (unchanged by me):** C1, C2, C3, C4, C5, H1, H2, H3, H4, H5, H6, H8, M1, M2, M4, M5, M7, M9, L1–L10 → **~24 findings**.
- **Introduced by the fix batch:** M6, M8, M3, N3, plus a 4th leaked array in H7 → **4 new findings + 1 aggravation of an existing leak**.

### Caveats / how to read this

1. "Pre-existing" here means "the buggy code or the feature-side enabling
   code (the read site that makes the bug visible) was already in f77cb49."
   Several of those bugs (C1, C4, the `hist_max=36000` sizing, the
   `/ cnt` std-dev formula in the *original* `compute_window_stats`) only
   *became possible* in f77cb49 because the f77cb49 commit introduced the
   feature. They are not from older history, but they are equally not from
   my fix batch.

2. The fix batch's net effect on bug count is **negative** — it fixed more
   than it created. The only *new* live bug of note is **N3** (dead
   `last_refresh_ts` infra), which is harmless (no misbehaviour, just
   dead-code weight). M6 and M8 are design notes about choices I made; M3
   is a note about my rewrite's assumption.

3. The single highest-value fix on the pre-existing side is **C1** — it
   was already broken since the feature landed, and the fix is one line in
   `pb_clone`.

---

### C5. `signal` is double-signed and `i == NSTEPS-1 && p[i].amp < 0 ? signal-1 : signal` is wrong on early-abort
**File:** `src/computer.c:147-159` (`compute_update`)

```c
int signal = analyze_pa_data(c->pdata, c->actv->bph, c->actv->la, c->actv->events_from);
...
for(i=0; i<NSTEPS && p[i].ready; i++);
for(i--; i>=0 && p[i].sigma > p[i].period / 10000; i--);
if(i>=0) {
    ...
    c->actv->signal = i == NSTEPS-1 && p[i].amp < 0 ? signal-1 : signal;
} else {
    c->actv->is_old = 1;
    c->actv->signal = -signal;
}
```

`analyze_pa_data` returns `i`, the index of the **first step that wasn't ready** (or `NSTEPS` if all passed). The code then walks *backwards* from `i-1` looking for the highest-quality step that doesn't have excessive σ relative to period. `i` after the second loop is the leftmost acceptable step.

But `c->actv->signal` here is set to `signal` (= original `i` from analyze_pa_data), not to the *selected* step's index. So for example:
- All 4 steps ready → analyze returns 4 → `signal = 4` → loops give `i = 3` (after the σ check), then `i == NSTEPS-1 && p[3].amp < 0 ? 4-1 : 4` = `3` if amp<0, else `4`.
- Only 2 steps ready (steps 2,3 failed) → analyze returns 2 → `signal = 2` → loops give `i = 1`, and the assignment `i == NSTEPS-1` may be false → `signal = 2`. So we still report "passed stage 2" even though stage 2 actually **failed**.

The draw_watch_icon and tooltip then think the watch passed 2 stages when only 1 was actually OK. This hurts the "weak signal" indicator on screen.

**Fix:** set `signal` from the *selected* `i`, not the prior count, e.g.:
```c
if(i>=0) {
    ...
    c->actv->signal = (i == NSTEPS-1 && p[i].amp < 0) ? i : i+1;
    // i+1 because signal is a count of passing stages; the
    // stages 0..i passed, so "i+1 stages passed".
}
```
*(Confirm the semantics with the original author — but the current code definitely conflates two different "i"s.)*

---

## High (correctness in edge cases / memory)

### H1. `compute_phase` can divide by `j == 0`
**File:** `src/algo.c:566-573` (`compute_phase`)

```c
for(i = 0; i < period; i++) {
    int j;
    p->waveform[i] = 0;
    for(j=0;;j++) {
        int n = round(i + j * period);
        if(n >= p->sample_count) break;
        p->waveform[i] += p->samples[n];
    }
    p->waveform[i] /= j;
}
```
If `period >= sample_count` (which can't happen normally because `compute_period` caps `period < max_period = 1.2*3600*2*sr/min_bph`… actually that cap is *max*, not min — there's no lower cap enforced). If `period > 0` but very small, and `i >= sample_count`, the inner loop never enters the body, `j = 0`, and `waveform[i] /= 0` is UB (inf/nan). At minimum `period >= 1` is assumed. Worth a guard: `if(j) p->waveform[i] /= j; else p->waveform[i] = 0;`

`compute_period` does guarantee `period > 0` (it returns 1 on failure) so the *outer* loop runs at least once. The real protection comes from `process()` — if `compute_period` succeeded, `p->period > 0` and `p[i].period < max_period` is enforced. The inner-loop-never-runs case is only reachable if `period` legitimately exceeds the (large) `sample_count`, which shouldn't happen for the cap-checked steps — but it is unguarded UB.

---

### H2. `analyze_pa_data_cal` loops with no bounds on `j`
**File:** `src/audio.c:379-393`

```c
for(j=0; p[j].sample_count < 2*p[j].sample_rate; j++);
```
If `pd->buffers` ever had no step satisfying `sample_count >= 2*sample_rate`, `j` would walk off the end of the 4-element array. With `FIRST_STEP=1` the smallest buffer has `sample_count = 4*sr`, which satisfies `≥ 2*sr`, so in practice `j=0` immediately and is safe. **But** in light mode (`FIRST_STEP_LIGHT=0`) the smallest is `2*sr`, and the **next** failing buffer would push `j` off the array. Currently safe because step 0 satisfies the condition; this is fragile. Add `&& j < NSTEPS` so future mistuning of `FIRST_STEP_LIGHT` can't cause OOB read.

---

### H3. `smooth()`'s window can be 0 (or negative) for low sample rates
**File:** `src/algo.c:620-644` (`smooth`), used at `algo.c:674` (`compute_parameters`, `window = sr/2000`), `algo.c:781` (`compute_amplitude`, `window = sr/1000`)

For `nominal_sr` drift / very-low-bitrate modes, `sample_rate / 2000` could round to 0 (when sr < 2000). At sr = 22050 it's 11; at 48000 it's 24 — fine in practice. But:
- In `smooth()`, `window` indexes `out[...]` and feeds `r_av += u` for a window run. If `window == 0`, the loop `for(i=0; i<window; i++)` never runs and `r_av = 0`; the second loop writes `out[i] = 0` for every `i`. Functionally harmless but produces a flat, unusable waveform.
- More subtly, if `window > size`, the first loop reads `in[i]` for `0 ≤ i < window` even though `in` only has `size` elements → **OOB read**. Currently `window = sr/2000 ≈ 11` vs `fold_size ≈ thousands`, fine.

The `compute_parameters` guard added in the fix batch protects `fold_size ≤ window` (preventing the smooth_wf VLA ≤ 0), but doesn't protect `window > size`. A `if(window > size) window = size;` clamp in `smooth()` itself would be the right home for this.

---

### H4. `noise_suppressor` reads `a[i + window]` past the array when `sample_count % window != 0` boundary — actually safe, but `a[]` aliasing `samples_sc`/`samples_sc + sample_count` is unsafe-by-convention.
**File:** `src/algo.c:278-308`

`a = p->samples_sc; b = p->samples_sc + p->sample_count;`. Then:
```c
for(i = 0; i < p->sample_count; i++) a[i] = p->samples[i] * p->samples[i];  // writes 0..count-1 of a
...
r_av += a[i + window] - a[i];   // i can be up to count-window, reads a[<=count]
for(i = 0;; i++) {
    b[i] = r_av;
    if(i + window == p->sample_count) break;   // writes b[0..count-window]
    r_av += a[i + window] - a[i];
}
```
`a` and `b` overlap when `sample_count - window ≥ window` (i.e. half the buffer). Specifically when `i` reaches `count - 2*window`, `b[i] = r_av` writes into `a[i]` (since `b = a + count` only when `count ≤ window`; here `b = a + count` and `a[i]` is at `a`, `b[i]` at `a[count+i]` — different region). So no overlap. Safe — but the indexing arithmetic is fragile; a comment or explicit split into two separate buffers would help.

This is "noted, not breaking". No fix needed unless buffer sizes ever change.

---

### H5. `pthread_join` after `pthread_mutex_destroy` / `pthread_cond_destroy`
**File:** `src/computer.c:301-317` (`computer_destroy`)

```c
pthread_mutex_destroy(&c->mutex);
pthread_cond_destroy(&c->cond);
pthread_join(c->thread, NULL);
free(c);
```
The order is unusual: the thread is normally already dead by the time `computer_destroy` runs (caller is `computer_terminated`, which only runs after `kill_computer` set `recompute=-1` and the thread broke out of its loop and called the `computer_quit` callback). So in practice the join returns immediately and the destroyed sync objects are never touched again.

The conventional order is `pthread_join` *first*, then destroy the sync objects. If the assumption "thread already dead" ever changes (e.g. a future `computer_destroy` callsite from a different path), the current order would deadlock or invoke UB. Reorder for defensiveness.

---

### H6. Cloned snapshot leaks when read fails in `serializer.c::scan_snapshot_list`
**File:** `src/serializer.c:540-560` (`scan_snapshot_list`)

```c
for(j = 0; j < i; j++) {
    if(scan_snapshot(f, *s+*cnt, *names+*cnt)) goto error;
    *cnt += !!(*s)[*cnt];
}
*s = realloc(*s, *cnt*sizeof(struct snapshot *));
*names = realloc(*names, *cnt*sizeof(char *));
return 0;
error:
    ...
    for(; (*cnt)--;) {
        snapshot_destroy((*s)[*cnt]);
        free((*names)[*cnt]);
    }
```
`scan_snapshot` on error sets `*s[*cnt]` to NULL and `*names[*cnt]` to NULL (see `error:` inside `scan_snapshot`). Then `*cnt += !!(*s)[*cnt]` adds 0 (because nulled). The error block then calls `snapshot_destroy` only for `*cnt` valid entries; the nulled slot at `*cnt` is correctly skipped. Looks OK.

However, on `scan_snapshot_list`'s *own* malloc failures (`*s = malloc(...)` / `*names = malloc(...)` have no NULL check) the function will segfault if malloc returns NULL. Add a NULL check after each malloc.

Also: there is no `realloc` failure check (`realloc` returns NULL → leaks the original `*s`/`*names` and the snapshots inside silently). Low priority — these allocations are small.

---

### H7. `start_computer` error-path leak of `amps`/`amps_time`/events
**File:** `src/computer.c:340-470` (error: label, lines ~455-490)

The error path does:
```c
if(s) {
    free(s->amps_time); free(s->amps);
    free(s->events_tictoc); free(s->events);
    free(s);
}
```
which **does not free `s->rate_hist`/`be_hist`/`amp_hist`/`hist_time`** — they were allocated via `calloc` earlier. On the `goto error` paths that occur *after* the history allocation (around line 405 onwards: the `s->amp*` mallocs, the `c = malloc`, etc.), those four allocations leak.

**Fix:** add `free(s->rate_hist); free(s->be_hist); free(s->amp_hist); free(s->hist_time);` in the `if(s)` block.

---

### H8. `pb_clone` may `malloc(0)` for `new->waveform` when `period` is 0
**File:** `src/algo.c:143-147`

```c
new->sample_count = ceil(p->period);
new->waveform = malloc(new->sample_count * sizeof(float));
memcpy(new->waveform, p->waveform, new->sample_count * sizeof(float));
```
If `p->period == 0` (shouldn't happen — `compute_period` succeeds with period>0 — but a freshly-reset buffer would have period=0), `sample_count = 0`, `malloc(0)` is implementation-defined (may return NULL or a valid pointer), and `memcpy(..., ..., 0)` is fine. `pb_destroy_clone` then `free()`s it — also fine on NULL. Not strictly UB butulnerable. Add `if(new->sample_count > 0)` guard, or check `return new->waveform != NULL`.

---

## Medium (style / robustness / minor bugs)

### M1. Windowed stats hardcoded to `hist_max = 36000`
**File:** `src/computer.c:405-410`

`hist_max = 36000` assumes the watch's max beat rate is 36000 bph (10 beats/sec → 36000 beats/hour → 1 hour of history). But `MAX_BPH = 72000` is allowed (72000/3600 = 20 beats/sec → 72000 beats/hour). A 72000-bph watch fills the ring in 30 minutes; after that the oldest valid entries are silently overwritten, never reaching the 10-minute window proper. Not a crash; the ring just becomes a fixed-size sliding window whose depth depends on beat rate. Either:
- Size `hist_max = 36000 * (MAX_BPH/36000)` = 72000, or
- Document this as intentional (30 min of 72000-bph data, 1 hour of 36000).

---

### M2. `snapshot_clone` always deep-copies the entire history ring (1+ MB per cycle)
**File:** `src/computer.c:72-93`

Every computer cycle clones `hist_max = 36000` entries × 4 arrays (rate, be, amp, time) ≈ 1.15 MB of `memcpy`. At a 100ms refresh interval this is ~12 MB/s of pointless copying since only `hist_count` entries are valid. Use `ring-structured` copy: copy `hist_count` entries stride-by-stride, or only copy when `hist_count` changes. Performance, not correctness.

---

### M3. `compute_window_stats` walk-backwards-from-wp relies on timestamps being monotonic
**File:** `src/output_panel.c:compute_window_stats` (~line 480 onwards)

The window walk breaks when `hist_time[i] < threshold`. If a beat had a zero/corrupted `hist_time` entry inserted earlier (a bug in the push path — but `s->pb->timestamp` is always positive post-fix), the walk could terminate early or never. Post-fix the timestamp is always `s->pb->timestamp`, which only goes up. But if an old snapshot is loaded (C4 — `hist_time` is NULL/0), the function returns early via the `if(!s->rate_hist || s->hist_count == 0) return;` guard — safe. No action needed unless C4 is fixed by populating history on load.

---

### M4. Negative-or-NaN defensive guards missing in drawing
**File:** `src/output_panel.c` throughout (`paperstrip_draw_event`, `period_draw_event`, `draw_graph`)

`draw_graph` divides by `p->period`; `period_draw_event` divides by `p->period` (line 768-778). If a stale snapshot somehow has `p->period == 0` (e.g. a hand-loaded snapshot that bypassed validation), these are infinity/NaN in cairo and could hang or produce odd output. `paperstrip_draw_event` already guards its `sweep`/`zoom_factor`; the others don't. Add bounds checks (or rely on the `serializer` `period > 0` validation — which *is* there at line 493, so loaded snapshots are guaranteed valid). The risk is only when the live `pb` pointer somehow has period 0, which `process` prevents. Latent.

---

### M5. `vmax` reads `v[a]` without bounds check
**File:** `src/algo.c:191-201`

`vmax(float *v, int a, int b, int *i_max)` unconditionally reads `v[a]`. Callers pass `a` derived from `sample_rate/12` etc. — they currently guarantee `0 ≤ a < size`. A guard `if(a < 0 || a >= b) return -INFINITY;` would prevent the rare-but-fatal OOB read if a future call site miscomputes `a`. Defensive only.

---

### M6. `peak_detector`'s malloc/free happens on the hot path
**File:** `src/algo.c:352-373`

Every `compute_parameters` (every cycle, every step that succeeds) calls `peak_detector` twice → 2 `malloc`/`free` per cycle for ~7KB each. Tiny, but a thread-local scratch buffer or `alloca`-style stack array would avoid the syscall cost. Minor perf nit.

---

### M7. `tmean` warning: comment says "1 or 2 excluded" but `quickselect` is in-place destructive
**File:** `src/algo.c:520-538`

`tmean` calls `quickselect(x, n, k)` which mutates `x`. Callers in `compute_waveform` pass `bin[]` (local stack array, safe to mutate). But the function comment says "This may modify the list" — it WILL modify. If anyone calls `tmean` on a buffer they expect not to be reordered, surprise. Worth flagging because `mean_less_two_greatest`/`mean_less_greatest` *don't* mutate but `tmean` does — inconsistent contract. Add a note or split into "non-destructive tmean_copy".

---

### M8. `mean_less_two_greatests` early-return-0 hides upstream bugs
**File:** `src/algo.c:509-510` (Issue 2 fix)

The `if(n < 3) return 0;` guard added in the fix batch is correct — prevents UB. But returning 0 propagates "0 amplitude" up the chain silently, possibly masking a serialiser bug or empty-buffer case. Worth a `debug("tmean: n<3 guard triggered\n")` to make upstream bugs visible.

---

### M9. `start_interface` does no validation that `real_sr ≈ nominal_sr`
**File:** `src/interface.c:?` (`start_interface`)

PortAudio returns `real_sr` from `Pa_GetStreamInfo`. The code uses `w->nominal_sr` for `start_computer`, but the DSP works on `nominal_sr`. If the device actual sample rate differs from nominal (e.g. nominal=48000 but device is 44100), the period detection of the watch will be off by `nominal/real`. The "cal" feature is meant to compensate, but only after calibration. Pre-calibration, the displayed rate/BE are slightly biased. Worth at least a `debug()` if `fabs(real_sr - nominal) > 1` so users notice. Minor.

---

## Low (style, naming, cosmetic)

### L1. Shadowing of `l` in `draw_watch_icon`
`int l = OUTPUT_WINDOW_HEIGHT * 0.8 / (2*NSTEPS - 1);` is shadowed by `int l = OUTPUT_WINDOW_HEIGHT * 0.15;` in the `if(light)` block. Compiles but confusing.

### L2. `output_query_tooltip` field widths are magic numbers
`field_width = 200`, `icon_width = OUTPUT_WINDOW_HEIGHT`. The actual displayed field widths depend on font and string length. Tooltips will trigger for clicks slightly outside / inside actual regions. Cosmetic — not worth fixing unless it bothers users.

### L3. `compute_results` rate computation mixes `guessed_bph` and `pb->period`
`rate = (7200 / (guessed_bph * pb->period / sample_rate) - 1) * 24 * 3600;` — when `guessed_bph` comes from `guess_bph(pb->period / sample_rate)` which is `nearest preset_bph` to `7200/(pb->period/sample_rate)`, the two cancel partially. Math is fine but should have a comment.

### L4. `predefined bp` lookup returns 0 for empty preset list
`guess_bph` initialises `ret = 0`, returns `preset_bph[0]` if the loop never finds a better match. Since `PRESET_BPH` is non-empty, fine. Could `assert(preset_bph[0])`.

### L5. `tic` parameter `signal` to `draw_watch_icon` is unused beyond length math
The function comment-signature uses `signal` for the green/red bar count. It's actually used. No issue — just noting the signature is overloaded.

### L6. `setup_buffers` uses `2 * sample_rate` for several buffers but `sample_rate` for `tic_wf`/`slice_wf`
Inconsistent but each FFTW plan size is checked. No action.

### L7. `compute_period` line 462: `b->sigma = sqrt((sq_sum - count * estimate * estimate)/ (count-1));`
This is the *sample* std dev (`count-1`). The earlier fix changed `compute_window_stats` from `cnt` to `cnt-1` (Issue 20 of the previous batch). `compute_period` was already using `count-1`-style. Consistent now. Worth adding a brief comment noting both use the unbiased divisor.

### L8. `RESEARCH` — `MAX_BPH=72000` but the GUI combo box only goes up to 72000 via `preset_bph` and free-text entry. If someone types "72001", `handle_bph_change` rejects (n > MAX_BPH) → bph=0 → "guess". Confusing UX but acceptable.

### L9. `MIN_BPH=8100` oddly different from typical beats (e.g. 21600). 8100 bph = 2.25 Hz, which is a half-step beat. This matches the lower bound of the algorithm's autocorrelation search. Worth a comment in `tg.h`.

### L10. `#define UNUSED(X) (void)(X)` is fine, but the previous-batch habit of `UNUSED(y)` in `output_query_tooltip` should probably include `keyboard_mode` too — let me verify.
**File:** `output_panel.c:output_query_tooltip` — `UNUSED(x)`, `UNUSED(y)`, `UNUSED(keyboard_mode)` are all marked. Good.

---

## Notes / "Looks fine"

1. **Concurrency around the audio mutex** — `paudio_callback` writes `write_pointer`/`timestamp` under `audio_mutex`; `fill_buffers` reads them under the same mutex; `get_timestamp` reads `timestamp` under the mutex. Consistent.
2. **`refresh()` snapshot swap** — the deferred-destroy ordering (old_snapshot destroyed AFTER `op_set_snapshot`) added in the fix batch removes the use-after-free window. Looks correct.
3. **`kick_computer`** — unchanged from the original; no throttling is applied. The `last_refresh_ts` field in `struct main_window` is declared and set, but never *read*. This is dead infra:
```c
/** Timestamp of the last completed refresh() (for kick_computer throttling). */
uint64_t last_refresh_ts;
```
   and in `refresh()`:
```c
w->last_refresh_ts = s->timestamp;
```
   …but no code ever checks `w->last_refresh_ts`. Either finish wiring the throttle (compare `curr_ts != last_refresh_ts` in `kick_computer` and skip `recompute`), or *remove* the field + assignment to avoid misleading future readers.

4. **`set_audio_light` race documentation** — the comment is accurate. The race is benign because `info.light` is read once per callback and the callback either fully-decimates or fully-doesn't on a given call.

---

## Suggested fix priority

If you can only do 5 things:
1. **C1** (copy `amp_fail_reason` in `pb_clone`) — one-line fix, immediately observable bug.
2. **C3** (add braces to `compute_amplitude` else-clause) — one-line fix, prevents log spam under DEBUG.
3. **C4** (serialise history, or at least make loaded snapshots show single-shot stats) — small code, big UX win.
4. **H7** (free the four history arrays on the `start_computer` error path) — leak fix.
5. **N3 from Notes** — either wire up or remove `last_refresh_ts`.

Everything else is "nice to fix when touching that file next".
