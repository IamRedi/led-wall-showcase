# Verification Guide — LED Wall

> Hinweis zur öffentlichen Fassung: Verweise auf `docs/work/` (Aufgabenliste, Status, Nachweise) und auf den internen Arbeitsstandard (RWS) beziehen sich auf private Entwicklungsunterlagen, die hier nicht veröffentlicht sind. Den Stand fasst [docs/PROJEKTSTAND.md](PROJEKTSTAND.md) zusammen.

Meaning of verification, the level model, and the deterministic check catalog for this
repository. Follows RWS §12 (verification model, result vocabulary, evidence identity).

## Principles

- **Deterministic checks first:** prefer scripted, reproducible checks over judgment calls.
- **Levels are not interchangeable:** passing a lower level never proves a higher one.
- **Results use exactly:** `PASS`, `FAIL`, `NOT_RUN`, `NOT_VERIFIABLE`, `NOT_APPLICABLE`, `STALE`.
- **Evidence identity:** every evidence record states task ID, revision/worktree fingerprint,
  timestamp+timezone, command/procedure, environment, result, failures/skips, and role.

## Verification levels (RWS §12.1)

| Level | Meaning | Status in this repo |
|---|---|---|
| V0 | Static inspection / document consistency | Active (governance tasks) |
| V1 | Formatting, linting, static/type checks | NOT_APPLICABLE (no code yet) |
| V2 | Unit/component tests | NOT_APPLICABLE (no code yet) |
| V3 | Integration tests | NOT_APPLICABLE |
| V4 | End-to-end/runtime tests | NOT_APPLICABLE |
| V5 | Device/provider/staging validation | NOT_APPLICABLE (no device access authorized) |
| V6 | Production/hardware acceptance | NOT_APPLICABLE (no hardware access authorized) |

No firmware compilation is possible or authorized in this repository's current state.

## Check catalog — LED-L001 (V0, deterministic)

| # | Check | Command / procedure | PASS criterion |
|---|---|---|---|
| C1 | Required control-plane files exist | file listing of repo root + `docs/` tree | every file in the LED-L001 required-set present, non-empty |
| C2 | RWS copy integrity | `sha256sum` source vs `docs/standards/REDION_WORK_STANDARD.md` | hashes identical |
| C3 | Baseline copies integrity (×5) | `sha256sum` each `LED_WALL_V1_*.md` source vs `docs/baselines/` copy | all 5 hash pairs identical |
| C4 | TASKS.yaml machine-valid | `python3 -c "import yaml;yaml.safe_load(open('docs/work/TASKS.yaml'))"` | parses without error |
| C5 | Git hygiene | `git diff --check` (and `--cached` variant) | no whitespace/conflict-marker errors; no commit created; no remote configured |
| C6 | No product source created | search repo for `src/`, `include/`, `lib/`, `firmware/`, `platformio.ini`, `*.ino`, server/UI code | none present |
| C7 | Legacy repository untouched | `find` + `sha256sum` fingerprint of `PLUTO EQ ACCESSORY` vs pre-task fingerprint | fingerprints identical |
| C8 | LED-L002 not authorized | inspect TASKS.yaml + worktree | LED-L002 recorded (roadmap) with `not_authorized`, no `authorized`/`in_progress` state, no LED-L002 files |
| C9 | Task lifecycle honest | inspect TASKS.yaml/STATUS.md | LED-L001 `locally_implemented`; never marked `reviewed`/`accepted`/`committed` |

## Check catalog — LED-L002 (core runtime foundation; executor: LED-L002)

Levels used: V0 (static/consistency), V1 (compiler warnings / firmware build as static analysis), V2 (host unit tests). V3/V4 NOT_APPLICABLE (no API/runtime endpoints yet). V5/V6 (panel verification: mapping on the physical panel, boot on real power) require explicit device authorization — NOT authorized for LED-L002; those gate lines stay pending device validation.

| # | Check | Command / procedure | PASS criterion |
|---|---|---|---|
| C-L002-1 | Host unit tests (V2) | `bash tests/run_tests.sh` | all checks pass; exit code 0 |
| C-L002-2 | Static warnings (V1) | host build uses `-Wall -Wextra -Werror` (same command as C-L002-1) | compiles with zero warnings (warnings are errors) |
| C-L002-3 | ESP32 firmware build (V1) | `~/.platformio/penv/bin/pio run -e esp32_eq -d firmware` | firmware compiles for esp32_eq; if the environment cannot build (offline, missing packages), record NOT_RUN with the exact error |
| C-L002-4 | LED-L001 non-regression (V0) | RWS 3-way hash, 5 baseline `cmp` pairs, legacy fingerprint, ledger lifecycle assertions, git hygiene checks | all unchanged / passing |
| C-L002-5 | Product-scope guard (V0) | search `firmware/` for LED-L003+ symbols/files (HTTP/WS server, ArduinoJson, WebSockets, FFT/audio, font/text, HUGO/HA) | none present |
| C-L002-6 | Mode-machine semantics vs frozen spec (V0) | review `mode_machine` against spec §4/§11 + transient-timer test results | exactly one baseMode; at most one transient; replace-not-queue; persistent cancels transient; expiry→baseMode; clear→idle/blank/no-animation; no stack; no queue |

Host test scope note: only pure modules (matrix mapping, frame throttle, mode machine, brightness, config fallback logic, public state) compile and test on the host. Arduino/FastLED/Preferences-dependent files (`main.cpp`, `led_driver`, `config_store`, `runtime` glue) are device-side; they are covered by C-L002-3 when the toolchain allows, otherwise recorded NOT_RUN with the exact environment reason.

## Check catalog — LED-L003 (semantic local API + device state; executor: LED-L003)

Levels used: V0 (static/consistency), V1 (compiler warnings / firmware build as static analysis), V2 (host unit tests). V3/V4 (live HTTP/WS round-trip on a runtime) require a device or emulator — NOT authorized; V5/V6 hardware gates remain pending device authorization.

| # | Check | Command / procedure | PASS criterion |
|---|---|---|---|
| C-L003-1 | Host unit tests incl. L003 suites (V2) | `bash tests/run_tests.sh` | all checks pass; exit code 0 |
| C-L003-2 | Static warnings (V1) | host build uses `-Wall -Wextra -Werror` (same command as C-L003-1); device glue compiled clean by C-L003-3 | zero warnings |
| C-L003-3 | ESP32 firmware build (V1) | `~/.platformio/penv/bin/pio run -e esp32_eq -d firmware` | firmware compiles incl. HTTP/WS transport; if the environment cannot build, record NOT_RUN with the exact error |
| C-L003-4 | LED-L002/L001 non-regression (V0) | L002 host suites within C-L003-1; RWS 3-way hash; 5 baseline `cmp`; legacy fingerprint; ledger lifecycle assertions; git hygiene | all unchanged / passing |
| C-L003-5 | Product-scope guard (V0) | search new API sources for LED-L004+ symbols (HTML/JS/CSS assets, UI strings, HA/MQTT, HUGO, FFT/audio) | none present |
| C-L003-6 | API semantics vs frozen spec (V0) | §5/§6/§9/§10/§11 clause → test mapping (evidence record) | every implemented clause mapped to a passing test; renderer-deferred clauses explicitly listed as pending |

## Check catalog — LED-L004 (Phone/Web UI; executor: LED-L004)

Levels used: V0 (static/consistency), V1 (compiler warnings / firmware build as static analysis), V2 (host unit tests incl. the dashboard's JS core suite). V3/V4 (live browser/device HTTP+WS behavior) require a device — NOT authorized; V5/V6 hardware gates remain pending device authorization.

| # | Check | Command / procedure | PASS criterion |
|---|---|---|---|
| C-L004-1 | Host unit tests incl. L004 suites (V2) | `bash tests/run_tests.sh` | all checks pass; exit code 0; dashboard JS-core suite executed (node present) or explicitly recorded NOT_RUN with the exact reason |
| C-L004-2 | Static warnings (V1) | host build uses `-Wall -Wextra -Werror` (same command as C-L004-1); device glue compiled clean by C-L004-3 | zero warnings |
| C-L004-3 | ESP32 firmware build incl. dashboard asset (V1) | `~/.platformio/penv/bin/pio run -e esp32_eq -d firmware` | firmware compiles with the embedded PROGMEM dashboard; artifact size/SHA-256 recorded; if the environment cannot build, record NOT_RUN with the exact error |
| C-L004-4 | LED-L003/L002/L001 non-regression (V0) | L003 host suites within C-L004-1; RWS 3-way hash; 5 baseline `cmp`; legacy fingerprint; ledger lifecycle assertions; git hygiene | all unchanged / passing |
| C-L004-5 | Product-scope guard (V0) | search new/changed sources for LED-L005+ symbols (HA/MQTT/HUGO/FFT/audio) and for renderer-internal references in the dashboard (FastLED/GPIO/serpentine/LED-index) | none present |
| C-L004-6 | Dashboard-contract consistency (V0) | C++ suite asserts the served content references only documented endpoints/vocabularies and bounds; node suite asserts LWCORE payload shapes vs CONTRACT.md | page is a pure same-origin semantic-API client; payload shapes match CONTRACT.md |

## Check catalog — LED-L005 (Home Assistant external adapter; executor: LED-L005)

Levels used: V0 (static/consistency), V1 (Python compile/static pass + ESP32 build as static analysis), V2 (host unit tests incl. the adapter suites and a deterministic in-process integration round-trip over a fake MQTT broker + fake device HTTP server). Live V3/V4 (real broker, real Home Assistant, real device) and V5/V6 (hardware) require infrastructure/device authorization — NOT authorized; recorded pending. The firmware itself has ZERO changes in LED-L005 (ADR-015: MQTT never enters the firmware) — the build check is a non-regression gate, not new device code.

| # | Check | Command / procedure | PASS criterion |
|---|---|---|---|
| C-L005-1 | Adapter host suites incl. integration round-trip (V2) | `for s in test_adapter_config test_adapter_mqtt_proto test_adapter_device test_adapter_entities test_adapter_integration; do python3 tests/$s.py; done` (via `bash tests/run_tests.sh`) | all adapter tests pass; exit code 0; python3 absent ⇒ recorded NOT_RUN with the exact reason |
| C-L005-2 | Adapter static compile pass (V1) | `python3 -m compileall -q ha_adapter` (via `bash tests/run_tests.sh` pre-step) | byte-compiles with no output/errors |
| C-L005-3 | Full C++ host suite incl. L002/L003/L004 (V2/V1) | `bash tests/run_tests.sh` | all checks pass; zero warnings (`-Wall -Wextra -Werror`); node JS-core suite pass or NOT_RUN with reason |
| C-L005-4 | Firmware non-regression build (V1) | `~/.platformio/penv/bin/pio run -e esp32_eq -d firmware` | build SUCCESS; `firmware.bin` size/SHA-256 recorded and compared to the accepted LED-L004 artifact `79770f33…` (byte-identical expected — zero firmware changes; any drift is recorded with its exact cause) |
| C-L005-5 | LED-L001..L004 non-regression (V0) | RWS 3-way hash; 5 frozen baseline `cmp` pairs; legacy fingerprint; ledger lifecycle assertions; git hygiene | all unchanged / passing |
| C-L005-6 | Firmware-scope + successor-scope guard (V0) | grep `firmware/` for MQTT/HA symbols (must be absent); grep `ha_adapter/` for HUGO/DeviceAdapter and renderer internals (FastLED/GPIO/serpentine/LED-index; must be absent) | firmware untouched by integration code; adapter is a pure API client; no LED-L006 code |
| C-L005-7 | Adapter-contract consistency (V0) | adapter suites assert discovery/entity shapes vs CONTRACT.md + frozen §7/§8 vocabularies (independently transcribed in tests) + ADR-015 mapping; ADR/README/config-example cross-checked | mapping table realized exactly; no undocumented endpoint use; vocabularies byte-equal to frozen sets |
| C-L005-8 | Secret hygiene (V0) | inspect tracked files for credentials; verify `ha_adapter/config.json` (runtime config) is git-ignored; example file contains placeholders only | no real credentials tracked; ignore rule effective (`git check-ignore`) |

## Check catalog — LED-L006 (HUGO DeviceAdapter; executor: LED-L006)

Levels used: V0 (static/consistency), V1 (Python compile/static pass + ESP32 build as static analysis), V2 (host unit tests incl. the adapter suites and a deterministic in-process integration round-trip over a fake device HTTP server + the stdio service loop). Live V3/V4 (real HUGO runtime, real device) and V5/V6 (hardware) require infrastructure/device authorization — NOT authorized; recorded pending. The firmware has ZERO changes in LED-L006 (§23/ADR-014: zero HUGO/LOOI code in firmware) — the build check is a non-regression gate, not new device code. `ha_adapter/` (accepted LED-L005) is untouched.

| # | Check | Command / procedure | PASS criterion |
|---|---|---|---|
| C-L006-1 | Adapter host suites incl. integration round-trip (V2) | `for s in test_hugo_config test_hugo_rpc test_hugo_intents test_hugo_device test_hugo_integration; do python3 tests/$s.py; done` (via `bash tests/run_tests.sh`) | all adapter tests pass; exit code 0; python3 absent ⇒ recorded NOT_RUN with the exact reason |
| C-L006-2 | Adapter static compile pass (V1) | `python3 -m compileall -q hugo_adapter` (via `bash tests/run_tests.sh` pre-step) | byte-compiles with no output/errors |
| C-L006-3 | Full prior battery (V2/V1) | `bash tests/run_tests.sh` | C++ host suite (L002–L004) + node JS-core + LED-L005 adapter suites all pass unmodified in expectations; zero warnings (`-Wall -Wextra -Werror`) |
| C-L006-4 | Firmware non-regression build (V1) | `~/.platformio/penv/bin/pio run -e esp32_eq -d firmware` | build SUCCESS; `firmware.bin` size/SHA-256 recorded and compared to the accepted artifact `79770f33…` (878032 B, byte-identical expected — zero firmware changes; any drift is recorded with its exact cause) |
| C-L006-5 | LED-L001..L005 non-regression (V0) | RWS 3-way hash; 5 frozen baseline `cmp` pairs; legacy literal-command manifest (`97095cc7…`); ledger lifecycle assertions; TASKS↔STATUS consistency; YAML parse + duplicate-key guard; git hygiene | all unchanged / passing |
| C-L006-6 | Firmware-scope + boundary guard (V0) | grep `firmware/` for HUGO/LOOI/DeviceAdapter symbols (must be absent — §23/gate/ADR-014); grep `hugo_adapter/` for renderer internals (FastLED/GPIO/serpentine/LED-index/screenToIndex; must be absent) and for MQTT/HA surfaces (must be absent); `ha_adapter/` diff-free vs HEAD | zero HUGO/LOOI code in firmware; adapter is a pure API client; HA adapter untouched |
| C-L006-7 | §16 boundary consistency (V0) | intent catalog == independently transcribed §16 allowed set; tool→endpoint mapping table realized; frozen §7/§8 vocabularies byte-equal in descriptions (asserted in suites); no tool exposes raw coordinates/renderer controls | catalog exactly the allowed surface; no extra authority |
| C-L006-8 | Adapter-contract consistency (V0) | adapter suites assert exact CONTRACT.md endpoints/payload shapes against a fake device (independently derived expectations); conditional tools gated on advertised capabilities[] only | only documented endpoints called with contract-conformant minimal payloads |
| C-L006-9 | Config/secret hygiene (V0) | inspect tracked files for credentials; verify `hugo_adapter/config.json` (runtime config) is git-ignored; example file contains placeholders only; stdout protocol purity asserted in suites | no credentials tracked; ignore rule effective (`git check-ignore`); no secrets/logs on stdout |

## Check catalog — LED-L007 (Visual renderers + text pipeline; executor: LED-L007)

Levels used: V0 (static/consistency), V1 (compiler warnings / firmware build as static analysis), V2 (host unit tests incl. golden-frame renderer suites). Live V3/V4 (browser/device round-trips) and V5/V6 (hardware) require device authorization — NOT authorized for LED-L007; recorded pending. The milestone intentionally changes firmware (renderers), so the build check records the NEW artifact identity with the exact delta vs the accepted LED-L006 artifact `79770f33…` (878032 B).

| # | Check | Command / procedure | PASS criterion |
|---|---|---|---|
| C-L007-1 | Host unit tests incl. L007 golden-frame suites (V2) | `bash tests/run_tests.sh` | all checks pass; exit code 0; new renderer/text/effects suites executed; golden expectations independently derived (frozen/heritage facts or fixed fixtures), never produced by calling the implementation under test |
| C-L007-2 | Static warnings (V1) | host build uses `-Wall -Wextra -Werror` (same command as C-L007-1); device glue compiled clean by C-L007-3 | zero warnings |
| C-L007-3 | ESP32 firmware build with renderers (V1) | `~/.platformio/penv/bin/pio run -e esp32_eq -d firmware` | build SUCCESS; firmware.bin size/SHA-256 recorded; RAM/Flash from the build report; delta vs accepted LED-L006 artifact recorded and explained; flash usage safe for the board partition |
| C-L007-4 | LED-L001..L006 non-regression (V0) | prior host suites within C-L007-1 (with the documented canonical textTooWide expectation updates only); RWS 3-way hash; 5 frozen baseline `cmp` pairs; legacy literal-command manifest (`97095cc7…`); ledger lifecycle assertions; TASKS↔STATUS consistency; YAML parse + duplicate-key guard; git hygiene | all unchanged / passing; historical accepted debt remains visible |
| C-L007-5 | Renderer-layer boundary guard (V0) | grep `firmware/src/render/`, `firmware/src/text/`, `firmware/src/effects/` for HTTP/WS/ArduinoJson/WebSockets/HA/MQTT/HUGO symbols and command-parsing references (must be absent); grep `ha_adapter/`+`hugo_adapter/` diff-free vs HEAD; grep firmware for MQTT/HA/HUGO symbols (must be absent) | renderer layer touches only matrix mapping/framebuffer/brightness-free palette state; adapters untouched; zero integration code in firmware |
| C-L007-6 | Rendering-contract consistency (V0) | frozen §6/§7/§8 clause → test mapping (evidence record): textTooWide/skippedCharacters/128-cap/speed bounds; 8 animation names as scene AND animation; 8-intent expression vocabulary byte-equal to the registry; idle/unknown ⇒ blank | every implemented rendering clause mapped to a passing test; spectrum/paint clauses explicitly recorded as conditional/out-of-scope (ADR-012, §7) |
| C-L007-7 | Renderer determinism/failure audit (V0) | adversarial matrix over the renderer layer (empty/1/128-char text, unsupported/multibyte bytes, speed bounds, width overflow, scroll/animation rollover, transient expiry mid-frame, brightness bounds, renderer switch at frame boundary, unknown mode/program, repeated frames, long-running counters) executed as host tests | every adversarial case: bounded deterministic behavior, no out-of-bounds write, no garbage output; genuine defects repaired with permanent regression coverage |



- Product implementation tasks (Phase 1+) MUST add their own catalog entries here before
  execution, including exact commands, prerequisites, side effects, and failure meaning.
- Hardware (V5/V6) checks require explicit task authorization, owner supervision where
  hazardous, and safe-state behavior (RWS §19.4).
- Evidence goes to `docs/work/evidence/` and is linked from the task record and STATUS.

## Check catalog — LED-L009 (Product experience reconstruction; executor: LED-L009)

Levels used: V0 (static/consistency), V1 (compiler warnings / firmware build as static analysis), V2 (host unit tests incl. the rewritten dashboard JS-core suite), V5 (bounded device/provider validation — Owner-authorized live hardware verification AFTER the host/build gates passed, per the task brief §29). V6 PRODUCTION electrical gates remain NOT VERIFIED (no instruments; a separate future campaign).

| # | Check | Command / procedure | PASS criterion |
|---|---|---|---|
| C-L009-1 | Host unit tests incl. L009 suites (V2) | `bash tests/run_tests.sh` | all checks pass; exit 0; new suites executed: countdown, input (decoder/queue), local menu, games, text studio, visualizer, API extensions, registry/identity/font updates; dashboard JS-core suite (`node tests/test_ui_core.mjs`) green |
| C-L009-2 | Static warnings (V1) | host build uses `-Wall -Wextra -Werror` (same command as C-L009-1) | zero warnings (warnings are errors) |
| C-L009-3 | Sanitizer pass (V2, LED-L007 review precedent) | ASAN+UBSAN instrumented rebuild of the full C++ suite with `-fno-sanitize-recover=all` | full suite passes under sanitizers (no UB, no leaks in scope) |
| C-L009-4 | ESP32 firmware build + resource budget (V1) | `~/.platformio/penv/bin/pio run -e esp32_eq -d firmware`; clean rebuild ×2 | build SUCCESS; artifact size/SHA-256 recorded; RAM/Flash from the build report; before/after delta recorded and safe (flash ≪ partition); byte-identical clean rebuild |
| C-L009-5 | LED-L001..L008 non-regression (V0/V2) | prior host suites within C-L009-1 (with the DISCLOSED expectation updates: registry growth additive-prefix pinned, code-point skippedCharacters, corrected D-L008-4 glyph goldens, identity extended capabilities, static-icon time-varying set); RWS 3-way hash; 5 frozen baseline `cmp` pairs; legacy fingerprint; ledger lifecycle; git hygiene | all pass; historical accepted debt remains visible; the five D-L008 fixes regression-pinned |
| C-L009-6 | Dashboard-contract consistency (V0) | C++ suite needles (endpoints incl. LED-L009 routes, event names, vocabularies, error vocabulary, ASCII-only, no forbidden constructs) + node LWCORE payload/bounds/vocab/preview suites vs CONTRACT.md | page is a pure same-origin semantic-API client; payload shapes match the contract; preview mirrors the device Text Studio |
| C-L009-7 | Boundary guard (V0) | grep render/text/effects/input/menu/games/timer/visualizer/sound for HTTP/WS/ArduinoJson/HA/MQTT/HUGO symbols (must be absent); grep adapters diff-free vs HEAD | renderer/game layers stay pure; adapters untouched |
| C-L009-8 | Bounded hardware verification (V5) | pre-change 4 MB chip backup (hash recorded) → flash candidate (hash recorded per flash) → live checks: rejoin, /identity caps, /state subsystem truth, dashboard GET /, text modes/styles/charset, textTooWide, timer set/start/pause/resume/finish/stop + disconnect independence, game lifecycle + input, menu navigation, unknownGame, standby (brightness non-wake + command wake), notification transient restore, visualizer feed, registry, WS handshake + state.changed, reboot recovery | every executed gate PASS; everything unobservable (eyes-on visual judgment, soak, electrical) recorded NOT VERIFIED |
| C-L009-9 | Governance truthfulness (V0) | TASKS.yaml/STATUS.md/evidence cross-check | LED-L008 recorded `incomplete` (NOT accepted); LED-L009 recorded per lifecycle; no fabricated claims; no push/remote |

### LED-L009 review cycle 1 (2026-10-10) and documentation repair

Independent review cycle 1 returned **FAIL — B-L009-R1 (BLOCKING, documentation only): the
canonical API CONTRACT.md was incomplete and contradicted the implemented firmware** (root
cause: the original CONTRACT documentation patch aborted at an anchor assertion and never
wrote; a follow-up micro-patch fixed only the status line of the original document). The
Owner-authorized **documentation repair cycle 1** (same day, strictly documentation-only)
rewrote CONTRACT.md from the implemented source and corrected the evidence-accuracy items
relayed with the verdict (adapter counts HA 107 / HUGO 158 — the recorded "23" was one
suite's output line; worktree fingerprint reconciled with the superseded value preserved
and explained; node-suite reduction 45→31 recorded as observation O-L009-4). The C-L009-*
catalog above is unchanged; C-L009-6's CONTRACT-consistency criterion is now verifiable
against the rewritten document. Product code, firmware, tests, adapters and the built
artifact were hash-verified untouched by the repair.

### LED-L009 repair cycle 2 (2026-10-10) — Owner visual acceptance failed: V-L009-1/2/3

The independent re-review (cycle 2) returned PASS, but the **Owner's physical panel
acceptance was withheld** with three product defects (TASKS.yaml `owner_visual_findings`).
The Owner authorized one bounded repair cycle. Battery after the repair: **C++ host
31072 checks / 0 failures** (`-Wall -Wextra -Werror`; prior accepted anchor 30810/0 —
the delta is the new permanent repair coverage), **ASAN+UBSAN 31072/0 clean**, **node
JS-core 33/0** (was 31/0 — +2 repair checks), **HA 107/0 + HUGO 158/0 unchanged**,
firmware **byte-identical clean rebuild ×2** (candidate `c7fc4fef…`, 942352 B,
RAM 15.4 % / 50504 B, Flash 71.4 % / 935777 B). New checks: independent golden
framebuffer scroll tests (`test_text_scroll_goldens.cpp` — reference renderer shares no
code with the firmware renderer), U/W/u/w letterform goldens with the documented golden
updates, five-game difficulty behavior, `/display/game/difficulty` dispatcher semantics,
dashboard difficulty needles, LWCORE payload/vocabulary checks. Contract additions are
documented in CONTRACT.md (one additive route + one additive state field).

### LED-L009 repair cycle 3 (2026-10-10) — Owner visual retest failed: V-L009-4

The Owner's dashboard retest found **V-L009-4 (BLOCKING): the premium dashboard was
dead on every fresh page load** — the RC2 difficulty-selector wiring closed a
`forEach` statement with an IIFE closer (`ui_content.cpp:706` read `})();`),
producing `TypeError: document.querySelectorAll(...).forEach(...) is not a function`
inside the single boot IIFE; every handler wired after it plus the `/state` fetch,
WebSocket and page activation never ran (blank content area, pristine red status dot).
Valid syntax, so substring needles and the LWCORE-only node suite could not see it —
the class of defect only executing the served script catches. The Owner-authorized
repair cycle 3 executed: **one-token closer fix** (`})();` → `});`; genuine IIFE
closers untouched), **deterministic static Home activation** (`id="page-home"
class="page on"`), and **permanent real-script boot coverage** — new
`tests/test_ui_boot.mjs` (wired into `run_tests.sh`) extracts the EXACT served
document, parses it into a real DOM, executes the EXACT application script in a
sandbox with recording fetch/WebSocket/timers, and asserts the boot contract
(no-exception load, Home visible immediately with exactly one active page, six tabs,
difficulty selector init + command payloads, `/identity`+`/state`+WS init, offline
indicators without blanking, every wired control, refresh identity). It **fails on the
pre-repair RC2 source 6/6 with the recorded TypeError** and passes 74/0 after; a
closer-only variant still fails the Home-activation checks. Plus one C++ needle pinning
the static Home activation. Battery after RC3: **C++ host 31073/0**, **ASAN+UBSAN
31073/0 clean**, **node JS-core 33/0 + boot suite 74/0**, **HA 107/0 + HUGO 158/0
unchanged**, firmware **byte-identical clean rebuild ×2** (candidate `0f7a38de…`,
942352 B; RAM/Flash unchanged vs RC2 — the edits net ~zero bytes). Bounded live
hardware verification re-executed after the host gates; real-phone browser recorded
NOT VERIFIED (desktop browser + phone-viewport emulation only).

## Check catalog — LED-L010 (V0–V2, deterministic; V5 bounded)

Catalog added BEFORE execution (evidence-ordering discipline). All checks executed by
the executor and recorded in `docs/work/evidence/LED-L010-EXPANSION.md`; anything not
executed is NOT VERIFIED there.

| # | Check | Command / procedure | PASS criterion |
|---|---|---|---|
| C-L010-1 | Host battery (V2) | `bash tests/run_tests.sh` | C++ suite 0 failures (`-Wall -Wextra -Werror`) incl. the new suites test_display_topology, test_tail_games, test_players, test_ds4_wired, test_led_l010; node JS-core + boot suites 0 failures |
| C-L010-2 | Static warnings (V1) | the battery's `-Werror` compile | zero warnings |
| C-L010-3 | Sanitizer re-run (V2) | g++ `-fsanitize=address,undefined -fno-sanitize-recover=all` over the full suite | 0 failures, no sanitizer reports |
| C-L010-4 | Zone model (V2) | test_display_topology | target wire roundtrip; mask validation; TailFramebuffer bounds/config/clamps; 14 tail renderers deterministic over 6 lengths × 6 times; progress/chase/pulse cores |
| C-L010-5 | Tail games (V2) | test_tail_games | six games through the host lifecycle (ready/run/pause-frozen/resume/back/exit); determinism twins; 2P routing (light_race, tug_of_war); reaction_stop render-truth hit; color_catch life model; color_chain lose condition; memory playback swallow; difficulty + lane injection |
| C-L010-6 | P1/P2 API (V2) | test_players + test_led_l010 | input.send player attribution (absent=P1, validation); game.set players vs registry metadata; Pong 2P no-cross-talk; zone targets (defaults, mask validation, ledUnavailable disabled / invalidParameter unsupported); config tail.* keys; public-state zone views |
| C-L010-7 | DS4 core + wired seam (V2) | test_ds4_wired | face mapping + edges; d-pad auto-repeat schedule; slots/lifecycle (P1/P2, disconnect/reconnect, pairing transitions); inert-when-disabled; wired packet codec roundtrip + per-byte corruption rejection + CRC-8 KAT (0xF4) |
| C-L010-8 | UI V3 (V2) | test_ui_boot.mjs + test_ui_core.mjs + test_ui_dashboard needles | boot-exact-script contract (no-exception load, static Home, six tabs, difficulty, offline, wired controls, refresh) on the V3 artifact; LWCORE payload/zone/player builders + normalized state tail/ds4; C++ V3 structure needles |
| C-L010-9 | Adapters non-regression (V2) | the 5 HA + 5 HUGO suites; `git diff HEAD -- ha_adapter hugo_adapter` | all suites OK; adapter trees diff-free vs HEAD |
| C-L010-10 | Firmware build (V1) | `pio run -e esp32_eq` (clean ×2) | SUCCESS; byte-identical; artifact size + SHA-256 recorded; RAM/Flash recorded |
| C-L010-11 | DS4 env build (V1) | `pio run -e esp32_eq_ds4` | SUCCESS with the Bluepad32 core (4.1.0) + huge_app partition; resource deltas recorded |
| C-L010-12 | Baseline non-regression (V0) | RWS 3-way hash; five frozen baseline hashes; legacy fingerprint; ledger lifecycle | unchanged |
| C-L010-13 | Bounded hardware bring-up (V5) | pre-flash backup integrity; artifact hash at flash; live /identity /state /registry; matrix regression; zone-target + ledUnavailable live; game/player live; DS4-image boot/coexistence evidence as far as the physical controller permits | recorded honestly; unobservable gates NOT_VERIFIED |

### LED-L010 repair cycle 1 addendum (2026-10-14)

The authorized repair-cycle battery extends C-L010-1/3/8/9/10/11. In addition to the
checks above, `bash tests/run_tests.sh` runs `python3 tests/test_nvs_provision.py`
(exact pre-change backup bytes, exclusive/private backup creation, and secret redaction),
and `node tests/test_ui_boot.mjs` executes the exact served dashboard script with V3
interaction assertions. The completed run produced C++ 35,055/0, Node core 38/0, UI
boot 142/0, provisioning 4/4, HA 107/0, and HUGO 158/0; the complete C++ suite also
passed under ASAN+UBSAN (35,055/0).

Canonical and DS4 PlatformIO builds were repeated from the same isolated project path
after `pio run -t clean`; each pair was byte-identical. The evidence record captures
artifact sizes, SHA-256 identities, resolved dependency versions, and the distinction
between path-derived metadata when comparing separate project directories. No upload,
device access, or NVS write to hardware is part of these checks.

### LED-L011 V4.1 software candidate gates (2026-10-15)

These are executor checks for the isolated LED-L011 candidate. They do not
replace LED-L008 physical gates, LED-L009/L010 independent review, or Owner
acceptance. Exact outcomes and binary identities belong in
`docs/work/evidence/LED-L011-V4_1.md`.

| Gate | Procedure | Evidence expected |
|---|---|---|
| L011-SRC | Git HEAD/status in original and isolated checkout; source checksum comparison; frozen-baseline hashes | Original dirty/untracked files preserved; V4.1 changes only in isolated worktree; frozen files byte-identical |
| L011-HOST | `bash tests/run_tests.sh` | Host C++, exact served JS core/boot, provisioning and HA/HUGO adapter suites; counts, failures and skips reported separately |
| L011-CANONICAL | `~/.platformio/penv/bin/pio run -e esp32_eq -d firmware` | Build exit code, pinned dependency versions, RAM, flash, binary bytes and SHA-256 |
| L011-DS4 | `~/.platformio/penv/bin/pio run -e esp32_eq_ds4 -d firmware` | Build-only resource/binary evidence; DS4 Wi-Fi coexistence remains NOT_VERIFIED |
| L011-PIXEL | `test_v41_pixel_canvas.cpp`, exact served UI boot; device storage source audit | Frame atomicity, stale revisions, invalid frames, live cap, local Save vs Send ACK/offline behavior. Real SPIFFS writes and power-loss recovery NOT_RUN |
| L011-FRAME | `test_v41_fireworks.cpp`, `test_v41_combined.cpp`, `test_v41_output_limiter.cpp`; `bash tests/capture_v41_frames.sh` | Deterministic cross-zone frame sequence, shared frame identity, branch/combined estimated limit and production-renderer PPM snapshots; real current, electrical/thermal behavior and latch timing NOT_VERIFIED |
| L011-GAMES | `test_v41_games.cpp` plus existing game/repair/players suites | Registered IDs, real game mechanics, lifecycle, per-player input, scores and rule bounds; phone transport latency and eyes-on fairness NOT_VERIFIED |
| L011-SCENE-SOUND | `test_v41_scene.cpp`, `test_v41_sound.cpp`, exact served UI boot | Bounded timeline, preset order, priority/mute scheduler; physical buzzer output NOT_RUN |
| L011-VISUAL | Headless Chrome render of the exact embedded UI at 390×844 and host DOM checks | Desktop/browser fixture only; real phone, gesture ergonomics and Owner panel visuals NOT_VERIFIED |
| L011-DEVICE | Owner-supervised hardware plan under separate authorization | No flash, GPIO/Tail command, NVS write, DS4 pairing, buzzer, power or Production gate in this task |

A build PASS never promotes a hardware gate. The canonical app partition
remains 1,310,720 bytes; the DS4 build uses the existing `huge_app` layout and
has no filesystem project storage. Firmware should report unavailable rather
than format or replace a partition. Findings from each gate must remain in
LED-L011 evidence, including any FAIL, NOT_RUN or NOT_VERIFIED outcome.

## LED-L014 current DS4 app-only installation gate (2026-10-17)

The earlier LED-L011 `huge_app` note above is historical. The current DS4
build uses the exact existing partition CSV, with the 1,310,720 B app0 limit.

| Gate | Procedure | PASS criterion / current result |
|---|---|---|
| L014-SIZE | Generate gzip UI with pinned tooling; `pio run -e esp32_eq_ds4 -d firmware`; `stat` and SHA-256 image | PASS: 1,310,720 / 1,310,720 B, SHA recorded in LED-L014 evidence; zero byte margin |
| L014-HOST | `bash tests/run_tests.sh`; test exact decompressed/minified served UI with `tests/test_ui_boot.mjs` | PASS: C++ 44,197/0, JS core 38/0, raw boot 37/0, served boot 37/0; 269 Python adapter/provisioning tests PASS |
| L014-PARTITION | Decode protected backup partition sector and byte-compare generated CSV table (3,072-byte table plus `0xff` padding) | PASS: same six entries, including SPIFFS and NVS |
| L014-PREFLIGHT | Reidentify USB chip MAC, 4 MiB flash, partition table, V5 version, safe idle brightness and backup integrity | PASS: exact MAC/chip/4 MiB/partition/V5 image/low idle and new full backup confirmed |
| L014-INSTALL | App-only `esptool` write at `0x10000`, SHA readback; reboot and verify identity, UI, Wi-Fi, protected regions | PARTIAL PASS: app-only write hash/boot/Wi-Fi/UI/BT-ready; full postflash readback interrupted by CH340 disconnect at 85%; never use `pio upload` |
| L014-DS4-LIVE | Owner SHARE+PS pairing, state/input counter, P1 DS4 + P2 phone game, disconnect/reconnect, Wi-Fi/UI responsiveness and no observed reset | NOT_RUN pending Owner physical cable/safety check and SHARE+PS action |
