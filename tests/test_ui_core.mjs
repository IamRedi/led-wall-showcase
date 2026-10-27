// LED-L009 dashboard JS-core suite (C-L009-6).
//
// Evaluates the exact LWCORE region embedded in the served artifact
// (firmware/src/api/ui_content.cpp) in a DOM-free sandbox and asserts the
// dashboard's client-side contract: command payload shapes vs
// docs/api/CONTRACT.md (LED-L009 additive endpoints included), accepted
// parameter bounds, the frozen + extended vocabularies, the typed-error map,
// the countdown formatter/interpolation mirroring the device, fail-safe state
// normalization, and bounded favorites storage.
//
// Run: node tests/test_ui_core.mjs   (from the repository root)

import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import vm from 'node:vm';
import assert from 'node:assert/strict';

const here = dirname(fileURLToPath(import.meta.url));
const source = readFileSync(
  join(here, '..', 'firmware', 'src', 'api', 'ui_content.cpp'),
  'utf8',
);

const START = '/*LWCORE-START*/';
const END = '/*LWCORE-END*/';
const begin = source.indexOf(START);
const finish = source.indexOf(END);
if (begin < 0 || finish < 0 || finish < begin) {
  console.error('FAIL: LWCORE markers not found in ui_content.cpp');
  process.exit(1);
}
const core = source.slice(begin + START.length, finish);

const LWCORE = vm.runInNewContext(core + '\n;LWCORE', {}, { filename: 'lwcore.js' });

let checks = 0;
let failures = 0;
const J = (x) => JSON.parse(JSON.stringify(x === undefined ? null : x));
function test(name, fn) {
  ++checks;
  try {
    fn();
  } catch (err) {
    ++failures;
    console.error(`FAIL ${name}: ${err && err.message ? err.message : err}`);
  }
}

// --- Vocabularies: frozen §7/§8 first, LED-L009 additions after. ------------
const SPEC_ANIMATIONS = ['corners', 'checker', 'border', 'row_sweep', 'bounce', 'wave', 'rain', 'plasma'];
const SPEC_EXPRESSIONS = ['idle', 'listening', 'thinking', 'speaking', 'happy', 'alert', 'error', 'sleeping'];
const L9_ANIMATIONS = ['smile', 'wink', 'heart', 'heart_pulse', 'eyes', 'loading', 'success',
  'warning', 'arrows', 'aurora', 'stars', 'rainbow_flow', 'fire', 'matrix', 'calm'];
// LED-L010 tail/combined additions (ADR-LED-L010 D3).
const L10_ANIMATIONS = ['comet', 'scanner', 'pulse', 'breathing', 'meteor', 'spark_trail',
  'lightning', 'fire_flow', 'progress', 'countdown_progress', 'chase', 'color_wave', 'heartbeat'];
const L11_ANIMATIONS = ['fireworks', 'meteor_impact', 'energy_bloom', 'rainbow_river',
  'storm', 'lava_fountain', 'rain_drain', 'aurora_current', 'starstream'];
const L9_EXPRESSIONS = ['bored', 'tired', 'focused', 'excited'];
const GAMES = ['pong', 'snake', 'breakout', 'reaction', 'dodger',
  // LED-L010 tail lane games (ADR-LED-L010 D6).
  'color_chain', 'light_race', 'reaction_stop', 'color_catch', 'memory', 'tug_of_war',
  'tail_tennis', 'light_quest', 'quick_draw'];
const INPUTS = ['up', 'down', 'left', 'right', 'confirm', 'back', 'start_pause', 'home', 'action_b'];
const STYLES = ['solid', 'color_bg', 'rainbow_text', 'rainbow_bg'];
const PALETTE = ['black', 'white', 'gray', 'red', 'orange', 'yellow', 'green', 'cyan',
  'blue', 'purple', 'pink', 'warm_white', 'heritage_green'];

test('animations mirror the frozen §7 registry then the additive additions', () => {
  assert.deepEqual(J(LWCORE.ANIMATIONS),
    [...SPEC_ANIMATIONS, ...L9_ANIMATIONS, ...L10_ANIMATIONS, ...L11_ANIMATIONS]);
});
test('expressions mirror the frozen §8 vocabulary then the LED-L009 additions', () => {
  assert.deepEqual(J(LWCORE.EXPRESSIONS), [...SPEC_EXPRESSIONS, ...L9_EXPRESSIONS]);
});
test('games vocabulary matches the device registry (incl. LED-L010 tail games)', () => {
  assert.deepEqual(J(LWCORE.GAMES), GAMES);
});
test('LED-L010 animation zones mirror the device registry metadata', () => {
  assert.deepEqual(J(LWCORE.ANIMATION_ZONES.wave), ['matrix']);
  assert.deepEqual(J(LWCORE.ANIMATION_ZONES.comet), ['tail']);
  assert.deepEqual(J(LWCORE.ANIMATION_ZONES.rainbow_flow), ['matrix', 'tail', 'both']);
  assert.deepEqual(J(LWCORE.ANIMATION_ZONES.pulse), ['tail', 'both']);
  assert.deepEqual(J(LWCORE.ANIMATION_ZONES.fireworks), ['both']);
  assert.equal(Object.keys(LWCORE.ANIMATION_ZONES).length, 45);
});
test('LED-L010 game metadata carries zone and player counts', () => {
  assert.deepEqual(J(LWCORE.GAME_META.pong), { zone: 'matrix', players: 2 });
  assert.deepEqual(J(LWCORE.GAME_META.light_race), { zone: 'both', players: 2, minPlayers: 2 });
  assert.deepEqual(J(LWCORE.GAME_META.reaction_stop), { zone: 'tail', players: 2 });
  assert.deepEqual(J(LWCORE.GAME_META.snake), { zone: 'matrix', players: 1 });
});
test('input vocabulary matches the unified input model', () => {
  assert.deepEqual(J(LWCORE.INPUTS), INPUTS);
});
test('text styles and palette match the semantic Text Studio model', () => {
  assert.deepEqual(J(LWCORE.TEXT_STYLES), STYLES);
  assert.deepEqual(J(LWCORE.PALETTE), PALETTE);
  assert.equal(LWCORE.PALETTE_RGB.heritage_green, '#37ff66');
});
test('WS event envelopes cover the device event channel', () => {
  assert.deepEqual(J(LWCORE.WS_EVENTS),
    ['state.changed', 'mode.changed', 'transient.expired', 'text.done', 'timer.done']);
});
test('typed-error map covers the §9 vocabulary plus unknownGame', () => {
  for (const code of ['textTooWide', 'unknownScene', 'unknownAnimation', 'unknownExpression',
    'unknownGame', 'invalidParameter', 'unsupportedCapability', 'ledUnavailable', 'internalError']) {
    assert.ok(LWCORE.ERROR_TEXT[code], `missing ${code}`);
  }
});

// --- buildTextShow (CONTRACT.md shapes and bounds). -------------------------
test('buildTextShow rejects empty and over-cap text without a payload', () => {
  assert.throws(() => LWCORE.buildTextShow({ text: '' }));
  assert.throws(() => LWCORE.buildTextShow({ text: 'x'.repeat(129) }));
});
test('buildTextShow sends only text by default (device defaults apply)', () => {
  assert.deepEqual(J(LWCORE.buildTextShow({ text: 'hi' })), { text: 'hi' });
});
test('buildTextShow accepts exactly 128 characters', () => {
  assert.equal(LWCORE.buildTextShow({ text: 'a'.repeat(128) }).text.length, 128);
});
test('buildTextShow clamps the marquee speed to 40..500', () => {
  assert.equal(LWCORE.buildTextShow({ text: 'a', mode: 'marquee', speedMsPerPx: 1 }).speedMsPerPx, 40);
  assert.equal(LWCORE.buildTextShow({ text: 'a', mode: 'marquee', speedMsPerPx: 9999 }).speedMsPerPx, 500);
  assert.equal(LWCORE.buildTextShow({ text: 'a', mode: 'marquee', speedMsPerPx: 140 }).speedMsPerPx, 140);
});
test('buildTextShow maps direction to the rtl/ltr wire form', () => {
  assert.equal(LWCORE.buildTextShow({ text: 'a', mode: 'marquee', rtl: true }).direction, 'rtl');
  assert.equal(LWCORE.buildTextShow({ text: 'a', mode: 'marquee', rtl: false }).direction, 'ltr');
});
test('buildTextShow clamps the word dwell to 200..60000 and forwards loop', () => {
  const lo = LWCORE.buildTextShow({ text: 'a', mode: 'words', wordDwellMs: 10 });
  assert.equal(lo.wordDwellMs, 200);
  const hi = LWCORE.buildTextShow({ text: 'a', mode: 'words', wordDwellMs: 100000 });
  assert.equal(hi.wordDwellMs, 60000);
  const one = LWCORE.buildTextShow({ text: 'a b', mode: 'words', loop: false, wordDwellMs: 1000 });
  assert.equal(one.loop, false);
});
test('buildTextShow validates mode, style, color and bgColor names', () => {
  assert.throws(() => LWCORE.buildTextShow({ text: 'a', mode: 'matrix' }));
  assert.throws(() => LWCORE.buildTextShow({ text: 'a', style: 'sparkle' }));
  assert.throws(() => LWCORE.buildTextShow({ text: 'a', color: 'neon' }));
  assert.throws(() => LWCORE.buildTextShow({ text: 'a', bgColor: 'neon' }));
  const ok = LWCORE.buildTextShow({ text: 'a', mode: 'words', style: 'rainbow_text',
    color: 'white', bgColor: 'black' });
  assert.equal(ok.style, 'rainbow_text');
  assert.equal(ok.color, 'white');
});
test('buildTextShow forwards a transient durationMs', () => {
  const b = LWCORE.buildTextShow({ text: 'a', durationMs: 5000 });
  assert.equal(b.durationMs, 5000);
});

// --- timer / notify / games / input / standby builders. ---------------------
test('buildTimerSet enforces the device duration window', () => {
  assert.equal(LWCORE.buildTimerSet(10000).durationMs, 10000);
  assert.throws(() => LWCORE.buildTimerSet(500));      // below 1 s
  assert.throws(() => LWCORE.buildTimerSet(5999001));  // above the 99:59 cap
});
test('buildNotify shapes text, bounded duration and priority', () => {
  assert.deepEqual(J(LWCORE.buildNotify({ text: 'hi' })), { text: 'hi' });
  assert.equal(LWCORE.buildNotify({ text: 'hi', durationMs: 4000 }).durationMs, 4000);
  assert.equal(LWCORE.buildNotify({ text: 'hi', priority: 'high' }).priority, 'high');
  assert.equal(LWCORE.buildNotify({ text: 'hi', priority: 'normal' }).priority, undefined);
  assert.throws(() => LWCORE.buildNotify({ text: '' }));
  assert.throws(() => LWCORE.buildNotify({ text: 'a', durationMs: 999999999 }));
});
test('buildGameSet validates against the games registry', () => {
  assert.deepEqual(J(LWCORE.buildGameSet('snake')), { game: 'snake' });
  assert.throws(() => LWCORE.buildGameSet('tetris'));
});
test('LED-L010 buildGameSet validates the player mode against game metadata', () => {
  assert.deepEqual(J(LWCORE.buildGameSet('light_race', 2)), { game: 'light_race', players: 2 });
  assert.deepEqual(J(LWCORE.buildGameSet('pong', 1)), { game: 'pong', players: 1 });
  assert.throws(() => LWCORE.buildGameSet('snake', 2));   // solo-only game
  assert.throws(() => LWCORE.buildGameSet('pong', 3));    // out of range
});
test('LED-L010 buildAnimationSet validates the zone target', () => {
  assert.deepEqual(J(LWCORE.buildAnimationSet('comet')), { animation: 'comet' });
  assert.deepEqual(J(LWCORE.buildAnimationSet('comet', 'tail')), { animation: 'comet', target: 'tail' });
  assert.deepEqual(J(LWCORE.buildAnimationSet('rainbow_flow', 'both')),
    { animation: 'rainbow_flow', target: 'both' });
  assert.throws(() => LWCORE.buildAnimationSet('wave', 'tail'));     // matrix-only
  assert.throws(() => LWCORE.buildAnimationSet('comet', 'both'));    // tail-only
  assert.throws(() => LWCORE.buildAnimationSet('comet', 'rope'));    // bad target
  assert.throws(() => LWCORE.buildAnimationSet('nope', 'tail'));
});
test('LED-L010 buildInput carries the optional player slot', () => {
  assert.deepEqual(J(LWCORE.buildInput('confirm')), { input: 'confirm' });
  assert.deepEqual(J(LWCORE.buildInput('up', 2)), { input: 'up', player: 2 });
  assert.throws(() => LWCORE.buildInput('up', 3));
  assert.throws(() => LWCORE.buildInput('up', 0));
});
test('buildGameDifficulty validates against the difficulty vocabulary', () => {
  assert.deepEqual(J(LWCORE.buildGameDifficulty('easy')), { difficulty: 'easy' });
  assert.deepEqual(J(LWCORE.buildGameDifficulty('normal')), { difficulty: 'normal' });
  assert.deepEqual(J(LWCORE.buildGameDifficulty('hard')), { difficulty: 'hard' });
  assert.throws(() => LWCORE.buildGameDifficulty('extreme'));
  assert.throws(() => LWCORE.buildGameDifficulty(undefined));
});

test('preview font mirrors the corrected U/W/u/w device glyphs (V-L009-2)', () => {
  const font = LWCORE.PREVIEW_FONT;
  assert.deepEqual(J(font.U), [63, 64, 64, 64, 63]);   // open top, connected bottom
  assert.deepEqual(J(font.W), [127, 40, 28, 40, 127]); // raised apex double-V
  assert.deepEqual(J(font.u), [120, 64, 64, 64, 124]);
  assert.deepEqual(J(font.w), [120, 64, 32, 64, 120]);
  assert.notDeepEqual(J(font.U), J(font.O));           // U is no longer O
});

test('buildInput validates against the unified input vocabulary', () => {
  assert.deepEqual(J(LWCORE.buildInput('confirm')), { input: 'confirm' });
  assert.throws(() => LWCORE.buildInput('warp'));
});
test('buildStandby coerces to a strict boolean', () => {
  assert.deepEqual(J(LWCORE.buildStandby(true)), { on: true });
  assert.deepEqual(J(LWCORE.buildStandby(0)), { on: false });
});

// --- countdown format mirrors the device (MM:SS / SS / S.mmm). --------------
test('countdownFormat matches the device formats', () => {
  assert.equal(LWCORE.countdownFormat(0), '0.000');
  assert.equal(LWCORE.countdownFormat(7354), '7.354');
  assert.equal(LWCORE.countdownFormat(9999), '9.999');
  assert.equal(LWCORE.countdownFormat(10000), '10');
  assert.equal(LWCORE.countdownFormat(42000), '42');
  assert.equal(LWCORE.countdownFormat(59999), '59');
  assert.equal(LWCORE.countdownFormat(60000), '01:00');
  assert.equal(LWCORE.countdownFormat(3625000), '60:25');
  assert.equal(LWCORE.countdownFormat(-5), '0.000');
});
test('timerLocal interpolates running countdowns and freezes others', () => {
  const t = { state: 'running', remainingMs: 7000, durationMs: 10000 };
  assert.equal(LWCORE.timerLocal(t, 1000, 3000).remainingMs, 5000);
  assert.equal(LWCORE.timerLocal(t, 1000, 99999).remainingMs, 0);
  const p = { state: 'paused', remainingMs: 7354, durationMs: 10000 };
  assert.equal(LWCORE.timerLocal(p, 1000, 5000).remainingMs, 7354);
  assert.equal(LWCORE.timerLocal(null, 0, 0).state, 'idle');
});

// --- normalizeState: fail-safe normalization of the §4+ device truth. -------
test('normalizeState provides neutral defaults for every subsystem', () => {
  const s = J(LWCORE.normalizeState({}));
  assert.equal(s.available, false);
  assert.equal(s.baseMode, 'idle');
  assert.deepEqual(s.timer, { state: 'idle', durationMs: 0, remainingMs: 0 });
  assert.deepEqual(s.game, { active: false, name: '', phase: '', score: 0 });
  assert.equal(s.standby.on, false);
  assert.equal(s.menu.active, false);
  assert.equal(s.clients, 0);
  assert.deepEqual(s.warnings, []);
  // LED-L010 zone/controller defaults.
  assert.deepEqual(s.tail, { enabled: false, length: 0, reverse: false });
  assert.equal(s.animationTarget, null);
  assert.deepEqual(s.ds4, { supported: false, enabled: false, state: 'unsupported', players: 0 });
});
test('normalizeState passes through real device fields', () => {
  const s = J(LWCORE.normalizeState({
    available: true, deviceId: 'led-wall-1', fwVersion: '0.2.0-alpha.1',
    baseMode: 'timer', currentMode: 'timer',
    brightness: { current: 40, max: 40 },
    timer: { state: 'running', durationMs: 10000, remainingMs: 4321 },
    tail: { enabled: true, length: 144, reverse: false },
    animationTarget: 'both',
    ds4: { supported: true, enabled: true, state: 'connected', players: 1 },
    clients: { count: 2 }, warnings: ['w'], uptimeSeconds: 99,
  }));
  assert.equal(s.deviceId, 'led-wall-1');
  assert.equal(s.timer.remainingMs, 4321);
  assert.equal(s.brightness.max, 40);
  assert.equal(s.clients, 2);
  assert.deepEqual(s.warnings, ['w']);
  // LED-L010 views pass through.
  assert.equal(s.tail.enabled, true);
  assert.equal(s.tail.length, 144);
  assert.equal(s.animationTarget, 'both');
  assert.equal(s.ds4.players, 1);
});

// --- favorites: browser-local, bounded (cap 20). ----------------------------
test('favorites stay bounded at 20 and dedupe by label', () => {
  const store = new Map();
  const fake = { getItem: (k) => (store.has(k) ? store.get(k) : null), setItem: (k, v) => store.set(k, v) };
  const fav = LWCORE.makeFavorites(fake);
  for (let i = 0; i < 30; ++i) fav.add('t' + i, { text: 't' + i });
  assert.equal(fav.list().length, 20);
  assert.equal(fav.list()[0].label, 't29');
  fav.add('t29', { text: 'again' });
  assert.equal(fav.list().length, 20);
  assert.equal(fav.list()[0].payload.text, 'again');
  fav.remove('t29');
  assert.equal(fav.list().some((f) => f.label === 't29'), false);
  const trunc = LWCORE.makeFavorites(fake);
  trunc.add('x'.repeat(64), {});
  assert.equal(trunc.list()[0].label.length, 32);
});

// --- previewPixels: mirrors the device Text Studio. -------------------------
const rgbAt = (px, x, y) => px[y * 30 + x];
const isLit = (p) => p[0] !== 0 || p[1] !== 0 || p[2] !== 0;
test('preview: fixed text renders centered glyphs and black background', () => {
  const px = LWCORE.previewPixels('HI', { mode: 'fixed', style: 'solid', color: 'white' }, 0);
  assert.equal(isLit(rgbAt(px, 0, 0)), false);           // background black
  assert.ok(isLit(rgbAt(px, 9, 4)));                     // H stem (probe-verified geometry)
  assert.ok(isLit(rgbAt(px, 16, 2)));                    // I cap
});
test('preview: unsupported characters render as blank space columns', () => {
  const px = LWCORE.previewPixels('A~B', { mode: 'fixed', style: 'solid', color: 'white' }, 0);
  // '~' is unsupported → its 6 px column band (x offset of char 2 = 12..17) stays dark.
  let lit = 0;
  for (let x = 12; x <= 17; ++x) if (isLit(rgbAt(px, x, 4))) ++lit;
  assert.equal(lit, 0);
});
test('preview: Albanian letters render via code points', () => {
  const px = LWCORE.previewPixels('Ë', { mode: 'fixed', style: 'solid', color: 'white' }, 0);
  let lit = 0;
  for (let i = 0; i < 300; ++i) if (isLit(px[i])) ++lit;
  assert.ok(lit > 6);  // the Ë glyph is drawn (diaeresis + E body)
});
test('preview: marquee rtl enters from the right, ltr from the left', () => {
  const opts = { mode: 'marquee', style: 'solid', color: 'white', speedMsPerPx: 100 };
  const rtl = LWCORE.previewPixels('ABC', { ...opts, rtl: true }, 900);
  let rtlRight = 0, rtlLeft = 0;
  const ltr = LWCORE.previewPixels('ABC', { ...opts, rtl: false }, 900);
  let ltrRight = 0, ltrLeft = 0;
  for (let y = 0; y < 10; ++y) for (let x = 0; x < 30; ++x) {
    if (isLit(rgbAt(rtl, x, y))) (x >= 15 ? rtlRight++ : rtlLeft++);
    if (isLit(rgbAt(ltr, x, y))) (x >= 15 ? ltrRight++ : ltrLeft++);
  }
  assert.ok(rtlRight > 0 && rtlLeft === 0);
  assert.ok(ltrLeft > 0 && ltrRight === 0);
});
test('preview: words mode cycles one word per dwell and blanks after a one-shot', () => {
  const base = { mode: 'words', style: 'solid', color: 'white', wordDwellMs: 1000 };
  const w0 = LWCORE.previewPixels('UNE JAM', { ...base, loop: false }, 0);
  const w1 = LWCORE.previewPixels('UNE JAM', { ...base, loop: false }, 1000);
  assert.deepEqual(J(w0), J(LWCORE.previewPixels('UNE', { mode: 'fixed', style: 'solid', color: 'white' }, 0)));
  assert.deepEqual(J(w1), J(LWCORE.previewPixels('JAM', { mode: 'fixed', style: 'solid', color: 'white' }, 0)));
  const done = LWCORE.previewPixels('UNE JAM', { ...base, loop: false }, 2500);
  assert.ok(done.every((p) => !isLit(p)));
  const looped = LWCORE.previewPixels('UNE JAM', { ...base, loop: true }, 2000);
  assert.deepEqual(J(looped), J(w0));
});
test('preview: styles realize color_bg readability and rainbow fields', () => {
  const bg = LWCORE.previewPixels('HI', { mode: 'fixed', style: 'color_bg', color: 'red', bgColor: 'white' }, 0);
  assert.deepEqual(J(rgbAt(bg, 0, 0)), [255, 255, 255]);  // white background
  assert.deepEqual(J(rgbAt(bg, 9, 4)), [0, 0, 0]);        // readable black glyphs
  const rb = LWCORE.previewPixels('HI', { mode: 'fixed', style: 'rainbow_text', color: 'white' }, 0);
  assert.ok(isLit(rgbAt(rb, 9, 4)));
  assert.deepEqual(J(rgbAt(rb, 0, 0)), [0, 0, 0]);        // black background
  const rbg = LWCORE.previewPixels('HI', { mode: 'fixed', style: 'rainbow_bg', color: 'white' }, 0);
  assert.ok(isLit(rgbAt(rbg, 0, 0)));                     // colored background
  assert.deepEqual(J(rgbAt(rbg, 9, 4)), [255, 255, 255]); // white glyphs
});

console.log(`${checks} checks, ${failures} failures`);
process.exit(failures === 0 ? 0 : 1);
