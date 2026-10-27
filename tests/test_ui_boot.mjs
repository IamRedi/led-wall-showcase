// Execute the exact V5 HTML/JavaScript embedded in ui_content.cpp in a
// recording DOM and network harness. Cover boot, six-screen navigation,
// command payloads/ACKs, two-player sessions, Pixel Studio, and reconnect.
// Device and physical-output checks are separate from this host suite.

import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import vm from 'node:vm';

const here = dirname(fileURLToPath(import.meta.url));
const sourcePath = process.argv[2] || join(here, '..', 'firmware', 'src', 'api', 'ui_content.cpp');
const source = readFileSync(sourcePath, 'utf8');

// ---- extract the exact served document --------------------------------------
const RAW_BEGIN = 'R"lwui_v1(';
const RAW_END = ')lwui_v1"';
const rawStart = source.indexOf(RAW_BEGIN);
const rawEnd = source.indexOf(RAW_END, rawStart);
if (rawStart < 0 || rawEnd < 0) {
  console.error('FAIL: dashboard raw string not found in ' + sourcePath);
  process.exit(1);
}
const html = source.slice(rawStart + RAW_BEGIN.length, rawEnd);
const scriptStart = html.lastIndexOf('<script>');
const scriptEnd = html.indexOf('</script>', scriptStart);
if (scriptStart < 0 || scriptEnd < 0) {
  console.error('FAIL: application <script> not found in the served document');
  process.exit(1);
}
const script = html.slice(scriptStart + '<script>'.length, scriptEnd);

// ---- minimal real DOM --------------------------------------------------------
const VOID_TAGS = new Set(['meta', 'link', 'br', 'hr', 'img', 'input', 'source', 'wbr']);
const RAW_TEXT_TAGS = new Set(['script', 'style']);

function makeElement(tag) {
  const el = {
    nodeType: 'element',
    tagName: String(tag).toUpperCase(),
    attrs: Object.create(null),
    childNodes: [],
    parentNode: null,
    style: {},
    _cls: new Set(),
  };
  const classList = {
    add: (...names) => names.forEach((n) => el._cls.add(n)),
    remove: (...names) => names.forEach((n) => el._cls.delete(n)),
    contains: (n) => el._cls.has(n),
    toggle: (n, force) => {
      const want = force === undefined ? !el._cls.has(n) : !!force;
      if (want) el._cls.add(n); else el._cls.delete(n);
      return want;
    },
  };
  Object.defineProperty(el, 'classList', { value: classList });
  Object.defineProperty(el, 'className', {
    get: () => [...el._cls].join(' '),
    set: (v) => { el._cls = new Set(String(v).split(/\s+/).filter(Boolean)); },
  });
  Object.defineProperty(el, 'id', {
    get: () => el.attrs.id || '',
    set: (v) => { el.attrs.id = String(v); },
  });
  // Form-control model values (M-L010-7 behavioral coverage: the artifact
  // reads .value / .checked when building payloads).
  Object.defineProperty(el, 'value', {
    get: () => (el.attrs.value !== undefined ? el.attrs.value :
      el.tagName === 'SELECT' && el.childNodes.length ? el.childNodes[0].value : ''),
    set: (v) => { el.attrs.value = String(v); },
  });
  Object.defineProperty(el, 'width', { get: () => Number(el.attrs.width || 0) });
  Object.defineProperty(el, 'height', { get: () => Number(el.attrs.height || 0) });
  el.getBoundingClientRect = () => ({ left: 0, top: 0,
    width: el.id === 'pxTailDetail' ? 1500 : el.id === 'pxMatrix' ? 300 : el.width,
    height: el.id === 'pxMatrix' ? 100 : 44 });
  el.setPointerCapture = () => {};
  el.releasePointerCapture = () => {};
  Object.defineProperty(el, 'checked', {
    get: () => el.attrs.checked === 'checked' || el.attrs.checked === 'true',
    set: (v) => { el.attrs.checked = v ? 'checked' : 'false'; },
  });
  Object.defineProperty(el, 'textContent', {
    get: () => el.childNodes.map((n) => (n.nodeType === 'text' ? n.text : n.textContent)).join(''),
    set: (v) => {
      el.childNodes = [];
      if (v !== '') el.appendChild(makeText(String(v)));
    },
  });
  el.getAttribute = (n) => (n in el.attrs ? el.attrs[n] : null);
  el.setAttribute = (n, v) => { el.attrs[n] = String(v); };
  el.appendChild = (c) => { c.parentNode = el; el.childNodes.push(c); return c; };
  el.append = (...children) => children.forEach((c) => el.appendChild(
    typeof c === 'string' ? makeText(c) : c));
  el.replaceChildren = (...children) => { el.childNodes = []; el.append(...children); };
  el._listeners = Object.create(null);
  el.addEventListener = (name, fn) => { (el._listeners[name] ||= []).push(fn); };
  el.dispatch = (name, extra = {}) => {
    const e = { target: el, preventDefault() {}, pointerId: 1, clientX: 0, clientY: 0, ...extra };
    for (const fn of el._listeners[name] || []) fn(e);
    if (name === 'click' && typeof el.onclick === 'function') el.onclick(e);
  };
  Object.defineProperty(el, 'dataset', { get: () => new Proxy({}, {
    get: (_, key) => el.attrs['data-' + String(key).replace(/[A-Z]/g, (x) => '-' + x.toLowerCase())],
  }) });
  // Child manipulation used by the artifact's grid/list rebuilds (M-L010-7).
  Object.defineProperty(el, 'firstChild', {
    get: () => el.childNodes[0] || null,
  });
  el.removeChild = (c) => {
    const idx = el.childNodes.indexOf(c);
    if (idx >= 0) el.childNodes.splice(idx, 1);
    return c;
  };
  // Element-scoped selector queries (M-L010-7: the artifact queries within
  // rows/cards, e.g. row.querySelectorAll(".sw") in the picker sheets).
  el.querySelectorAll = (sel) => {
    const parts = String(sel).trim().split(/\s+/);
    const out = [];
    (function walk(n) {
      n.childNodes.forEach((ch) => {
        if (ch.nodeType !== 'element') return;
        if (matchesSequence(ch, parts)) out.push(ch);
        walk(ch);
      });
    })(el);
    return out;
  };
  el.querySelector = (sel) => el.querySelectorAll(sel)[0] || null;
  el.getContext = () => ({
    fillStyle: '',
    fillRect() {},
    clearRect() {},
    drawImage() {},
  });
  return el;
}

function makeText(text) {
  return { nodeType: 'text', text, parentNode: null };
}

function matchCompound(el, comp) {
  let rest = comp;
  const tag = rest.match(/^([a-zA-Z][a-zA-Z0-9-]*)/);
  if (tag) {
    if (el.tagName !== tag[1].toUpperCase()) return false;
    rest = rest.slice(tag[1].length);
  }
  while (rest.length > 0) {
    if (rest[0] === '#') {
      const m = rest.match(/^#([\w-]+)/);
      if (!m || el.id !== m[1]) return false;
      rest = rest.slice(m[0].length);
    } else if (rest[0] === '.') {
      const m = rest.match(/^\.([\w-]+)/);
      if (!m || !el._cls.has(m[1])) return false;
      rest = rest.slice(m[0].length);
    } else if (rest[0] === '[') {
      const m = rest.match(/^\[([\w-]+)(?:="([^"]*)")?\]/);
      if (!m || !(m[1] in el.attrs)) return false;
      if (m[2] !== undefined && el.attrs[m[1]] !== m[2]) return false;
      rest = rest.slice(m[0].length);
    } else {
      return false;
    }
  }
  return true;
}

function matchesSequence(el, parts) {
  if (!matchCompound(el, parts[parts.length - 1])) return false;
  if (parts.length === 1) return true;
  for (let a = el.parentNode; a && a.nodeType === 'element'; a = a.parentNode) {
    if (matchesSequence(a, parts.slice(0, -1))) return true;
  }
  return false;
}

function parseHtml(htmlText) {
  const root = makeElement('#document');
  const stack = [root];
  let i = 0;
  const addText = (owner, t) => {
    if (t.trim() !== '') owner.appendChild(makeText(t));
  };
  while (i < htmlText.length) {
    const lt = htmlText.indexOf('<', i);
    if (lt < 0) { addText(stack[stack.length - 1], htmlText.slice(i)); break; }
    if (lt > i) addText(stack[stack.length - 1], htmlText.slice(i, lt));
    if (htmlText.startsWith('<!--', lt)) { i = htmlText.indexOf('-->', lt) + 3; continue; }
    if (htmlText.startsWith('</', lt)) {
      const closeEnd = htmlText.indexOf('>', lt);
      const name = htmlText.slice(lt + 2, closeEnd).trim().toUpperCase();
      for (let s = stack.length - 1; s > 0; --s) {
        if (stack[s].tagName === name) { stack.length = s; break; }
      }
      i = closeEnd + 1;
      continue;
    }
    const tagMatch = htmlText.slice(lt).match(/^<([a-zA-Z][a-zA-Z0-9-]*)/);
    if (!tagMatch) { addText(stack[stack.length - 1], '<'); i = lt + 1; continue; }
    const el = makeElement(tagMatch[1]);
    let j = lt + tagMatch[0].length;
    const closeIdx = htmlText.indexOf('>', j);
    const tagChunk = htmlText.slice(j, closeIdx);
    const attrRe = /([a-zA-Z_:][\w:.-]*)(?:\s*=\s*("([^"]*)"|'([^']*)'|[^\s>]+))?/g;
    let m;
    while ((m = attrRe.exec(tagChunk)) !== null) {
      let val = m[2] === undefined ? '' : m[2];
      if (m[3] !== undefined) val = m[3];
      else if (m[4] !== undefined) val = m[4];
      el.attrs[m[1]] = val;
      if (m[1] === 'class' && val) el._cls = new Set(val.split(/\s+/).filter(Boolean));
    }
    stack[stack.length - 1].appendChild(el);
    const selfClosing = tagChunk.endsWith('/');
    i = closeIdx + 1;
    const lower = tagMatch[1].toLowerCase();
    if (RAW_TEXT_TAGS.has(lower)) {
      const rawEndIdx = htmlText.indexOf('</' + lower, i);
      const raw = htmlText.slice(i, rawEndIdx < 0 ? htmlText.length : rawEndIdx);
      if (raw.trim() !== '') el.appendChild(makeText(raw));
      i = rawEndIdx < 0 ? htmlText.length : rawEndIdx;
      continue;
    }
    if (!VOID_TAGS.has(lower) && !selfClosing) stack.push(el);
  }
  return root;
}

// ---- browser environment stubs ----------------------------------------------
const STATE_PAYLOAD = {
  available: true, deviceId: '', deviceType: 'visual_display_v1', protocolVersion: 1,
  fwVersion: '0.2.0-alpha.1', baseMode: 'idle', currentMode: 'idle',
  activeTransient: null, activeScene: null, activeAnimation: null, activeExpression: null,
  text: null,
  brightness: { current: 40, max: 40 },
  timer: { state: 'idle', durationMs: 0, remainingMs: 0 },
  menu: { active: false },
  game: { active: false, name: '', phase: 'ready', score: 0, difficulty: 'easy' },
  standby: { on: false },
  notification: { active: false },
  visualizer: { active: false },
  spectrum: { available: false, active: false },
  matrix: { width: 30, height: 10 },
  clients: { count: 0 }, warnings: [], uptimeSeconds: 447,
};
const IDENTITY_PAYLOAD = {
  deviceId: '', deviceType: 'visual_display_v1', role: 'display', protocolVersion: 1,
  fwVersion: '0.2.0-alpha.1',
  capabilities: ['display.text.show', 'display.text.clear', 'display.scene.set',
    'display.animation.set', 'display.expression.set', 'display.clear',
    'display.brightness.set', 'display.state.read', 'display.timer', 'display.games',
    'display.menu', 'display.input', 'display.notify', 'display.standby',
    'display.visualizer', 'text.styles'],
  matrix: { width: 30, height: 10 },
};

class FakeWebSocket {
  constructor(url) {
    this.url = url;
    this.readyState = 0;
    FakeWebSocket.created.push(url);
    FakeWebSocket.last = this;
  }
  close() {
    this.readyState = 3;
    if (typeof this.onclose === 'function') this.onclose();
  }
  send() {}
  static simulateOpen() {
    const ws = FakeWebSocket.last;
    ws.readyState = 1;
    if (typeof ws.onopen === 'function') ws.onopen();
  }
}
FakeWebSocket.created = [];
FakeWebSocket.last = null;

function makeEnv({ fetchMode, stateOverrides } = {}) {
  const mode = fetchMode || 'ok';
  // M-L010-7: per-env state payload so scenarios can drive game-active /
  // timer-running / menu-active rendering paths.
  const statePayload = JSON.parse(JSON.stringify(STATE_PAYLOAD));
  if (stateOverrides) Object.assign(statePayload, stateOverrides);
  const root = parseHtml(html);
  const byId = new Map();
  (function index(n) {
    if (n.nodeType !== 'element') return;
    if (n.id) byId.set(n.id, n);
    n.childNodes.forEach(index);
  })(root);

  const requests = [];
  const timersOut = [];   // setTimeout records
  const intervals = [];   // setInterval records
  let clock = 1000000;
  const sandboxFetch = (path, opts) => {
    requests.push({
      path,
      method: opts && opts.method ? opts.method : 'GET',
      body: opts && opts.body ? JSON.parse(opts.body) : null,
    });
    if (mode !== 'ok') return Promise.reject(new Error('network down'));
    const payload = path === '/state' ? statePayload
      : path === '/identity' ? IDENTITY_PAYLOAD
      : path === '/display/pixel/projects' ? { available: false, projects: [] }
      : path === '/display/pixel/session' ? { ok: true, session: 1, revision: 0 }
      : path === '/display/input/session' ? { ok: true, player: JSON.parse(opts.body).player,
                                              session: 101, sequence: 0 }
      : path === '/display/pixel/frame' ? { ok: true, session: JSON.parse(opts.body).session,
                                            revision: JSON.parse(opts.body).revision, applied: true }
      : { ok: true };
    return Promise.resolve({ ok: true, status: 200, json: () => Promise.resolve(payload) });
  };

  const documentObj = {
    getElementById: (id) => byId.get(id) || null,
    querySelectorAll: (sel) => {
      const parts = sel.trim().split(/\s+/);
      const out = [];
      (function walk(n) {
        n.childNodes.forEach((ch) => {
          if (ch.nodeType !== 'element') return;
          if (matchesSequence(ch, parts)) out.push(ch);
          walk(ch);
        });
      })(root);
      return out;
    },
    querySelector: (sel) => documentObj.querySelectorAll(sel)[0] || null,
    createElement: (t) => makeElement(t),
  };

  const storage = {};
  const sandbox = {
    document: documentObj,
    location: { protocol: 'http:', hostname: 'device.lan' },
    localStorage: {
      getItem: (k) => (k in storage ? storage[k] : null),
      setItem: (k, v) => { storage[k] = String(v); },
      removeItem: (k) => { delete storage[k]; },
    },
    performance: { now: () => clock },
    fetch: sandboxFetch,
    WebSocket: FakeWebSocket,
    setTimeout: (fn, ms) => { timersOut.push({ fn, ms }); return timersOut.length; },
    clearTimeout: () => {},
    setInterval: (fn, ms) => { intervals.push({ fn, ms }); return intervals.length; },
    clearInterval: () => {},
    console: { log() {}, error() {}, warn() {} },
    Blob: class {}, URL: { createObjectURL() { return 'blob:test'; }, revokeObjectURL() {} },
  };
  sandbox.window = sandbox;
  sandbox.scrollTo = () => {};
  sandbox.globalThis = sandbox;
  const context = vm.createContext(sandbox);
  return { doc: documentObj, byId, requests, timersOut, intervals, context, storage, localStorage: sandbox.localStorage };
}

function computedDisplay(el) {
  // Mirrors the artifact's own rules: .page{display:none}, .page.on{display:block};
  // the offline banner and toast toggle style.display directly.
  if (el.style && el.style.display) return el.style.display;
  if (el._cls.has('page')) return el._cls.has('on') ? 'block' : 'none';
  return 'block';
}

function flush() {
  return new Promise((resolve) => setImmediate(resolve));
}

// V5 boot and interaction checks execute the exact embedded script against a
// small DOM/network harness. The old V3 tab/filter assertions were replaced
// with consumer flows and ACK-bound command assertions.
let checks=0,failures=0;
function check(value,label){checks++;if(!value){failures++;console.error('FAIL '+label);}}
async function tick(){for(let i=0;i<5;i++)await flush();}
async function boot(mode='ok'){
  const env=makeEnv({fetchMode:mode});
  try{vm.runInContext(script,env.context,{filename:'led-wall-v5-ui.js'});}catch(e){console.error('BOOT exception',e);failures++;}
  await tick();return env;
}
const env=await boot();
const id=n=>env.doc.getElementById(n);
check(id('view-home').classList.contains('active'),'Home visible on first paint');
check(env.doc.querySelectorAll('.view.active').length===1,'one active view');
check(env.requests.some(r=>r.path==='/state'),'state fetched at boot');
check(env.requests.some(r=>r.path==='/display/pixel/projects'),'storage capability fetched');
check(FakeWebSocket.created.length>=1,'WebSocket connected');
check(id('connectionText').textContent==='Connected','authoritative connected state');
for(const name of ['scenes','games','create','text','timer','device']){
  const b=env.doc.querySelector('[data-go="'+name+'"]');
  check(!!b,'home destination '+name);
  b?.dispatch('click');
  check(id('view-'+name).classList.contains('active'),'destination opens '+name);
  check(env.doc.querySelectorAll('.view.active').length===1,'single view '+name);
}
const scene=env.doc.getElementById('sceneGrid').childNodes[0];
scene?.dispatch('click');
check(id('sheetWrap').classList.contains('open'),'scene sheet opens');
const playScene=id('sheetContent').querySelector('button');
playScene?.dispatch('click');await tick();
check(env.requests.some(r=>r.path==='/display/animation/set'&&r.body.animation==='fireworks'&&r.body.target==='both'),'scene commands integrated renderer');
check(!id('sheetWrap').classList.contains('open'),'scene sheet closes after ACK');
const game=env.doc.getElementById('gameGrid').childNodes[0];game?.dispatch('click');
const start=id('sheetContent').querySelector('button');start?.dispatch('click');await tick();
check(env.requests.some(r=>r.path==='/display/game/difficulty'),'game sets engine difficulty');
check(env.requests.some(r=>r.path==='/display/game/set'&&r.body.game==='pong'),'game starts by semantic API');
check(id('view-play').classList.contains('active'),'play view after game ACK');
id('claimP2').dispatch('click');await tick();
const heldButton=id('gamePad').childNodes.find(b=>b.getAttribute('aria-label')==='up');
check(!!heldButton,'P2 pad exposes Pong up control');
heldButton?.dispatch('pointerdown');await tick();
FakeWebSocket.last.onmessage();await tick();
check(id('gamePad').childNodes.includes(heldButton),'state refresh preserves held pad button');
heldButton?.dispatch('pointerup');await tick();
check(env.requests.some(r=>r.path==='/display/input/event'&&r.body.player===2&&r.body.input==='up'&&r.body.state==='press'),'P2 press reaches session API');
check(env.requests.some(r=>r.path==='/display/input/event'&&r.body.player===2&&r.body.input==='up'&&r.body.state==='release'),'P2 release survives state refresh');
id('releasePlayers').dispatch('click');await tick();
id('pixelSend').dispatch('click');await tick();
check(env.requests.some(r=>r.path==='/display/pixel/session'),'pixel session started');
check(env.requests.some(r=>r.path==='/display/pixel/frame'&&r.body.pixels.length===2700),'450-cell frame sent');
check(id('pixelStatus').textContent==='Applied','pixel Applied follows matching ACK');
id('textMessage').value='HELLO';id('textShow').dispatch('click');await tick();
check(env.requests.some(r=>r.path==='/display/text/show'&&r.body.text==='HELLO'),'text API command');
id('homeStop').dispatch('click');await tick();
check(env.requests.some(r=>r.path==='/display/clear'),'Home Stop sends lifecycle Clear');
const off=await boot('offline');
check(off.doc.getElementById('view-home').classList.contains('active'),'offline Home remains visible');
check(off.doc.getElementById('connectionText').textContent==='Offline','offline state truthful');
console.log(checks+' V5 UI checks, '+failures+' failures');
if(failures)process.exit(1);
