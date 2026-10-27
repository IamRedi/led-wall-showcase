#include "api/ui_content.h"
#include <cstring>
#if defined(ARDUINO)
#include <pgmspace.h>
#define LW_UI_PROGMEM PROGMEM
#else
#define LW_UI_PROGMEM
#endif
namespace ledwall { namespace ui {
namespace {
#ifdef LED_WALL_ENABLE_DS4
#include "ui_gzip.inc"
#else
const char kIndexHtmlStore[] LW_UI_PROGMEM = R"lwui_v1(<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover"><meta name="theme-color" content="#090d17"><title>LED Wall V5</title><style>
:root{--bg:#090d17;--surface:#121b2a;--raised:#1a2738;--line:#2d3a4b;--text:#f5f7fb;--muted:#a6b4c5;--accent:#91dfcb;--violet:#baa4ef;--amber:#f3c987;--red:#f08989;--ok:#8be0ab;--radius:22px}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}html,body{margin:0;min-height:100%;background:var(--bg);color:var(--text);font:15px/1.4 system-ui,-apple-system,"Segoe UI",sans-serif}body{background:radial-gradient(circle at 75% -10%,#26374a 0,transparent 35%),var(--bg)}button,input,select,textarea{font:inherit}button{cursor:pointer;color:inherit;border:1px solid var(--line);background:var(--raised);border-radius:14px;min-height:42px;padding:9px 13px}button:disabled{opacity:.4;cursor:default}button:active{transform:scale(.98)}button.primary{background:var(--accent);color:#09211c;border-color:var(--accent);font-weight:760}button.danger{border-color:#8b5056;color:#ffe1e2}button.ghost{background:transparent}button.selected{border-color:var(--accent);color:var(--accent)}input,select,textarea{border:1px solid var(--line);border-radius:12px;background:#0b1422;color:var(--text);padding:10px;min-height:42px;max-width:100%}input[type=range]{accent-color:var(--accent);padding:0;width:100%}input[type=color]{width:52px;padding:3px}textarea{width:100%;resize:vertical}label{display:block;color:var(--muted);font-size:12px;font-weight:650;margin-bottom:6px}small,.muted{color:var(--muted)}h1,h2,h3,p{margin-top:0}h1{font-size:25px;letter-spacing:-.035em;margin-bottom:4px}h2{font-size:21px;letter-spacing:-.025em}h3{font-size:16px;margin-bottom:8px}.eyebrow{text-transform:uppercase;letter-spacing:.16em;color:var(--accent);font-size:10px;font-weight:800}.shell{max-width:560px;margin:auto;padding:0 16px calc(28px + env(safe-area-inset-bottom))}.topbar{position:sticky;z-index:10;top:0;background:rgba(9,13,23,.94);backdrop-filter:blur(12px);display:flex;align-items:center;gap:10px;padding:13px 0 11px;border-bottom:1px solid #273247}.brand{font-weight:800;letter-spacing:.09em;font-size:12px}.topbar .spacer{flex:1}.connection{font-size:11px;color:var(--muted);display:flex;align-items:center;gap:7px}.dot{width:7px;height:7px;border-radius:50%;background:var(--red)}.dot.online{background:var(--ok);box-shadow:0 0 10px var(--ok)}.view{display:none;animation:appear .18s ease}.view.active{display:block}@keyframes appear{from{opacity:.45;transform:translateY(6px)}to{opacity:1;transform:none}}.hero{padding:24px 0 12px}.hero p{color:var(--muted);max-width:42ch}.status-card{display:flex;align-items:center;gap:14px;padding:15px;border:1px solid #38515b;border-radius:20px;background:linear-gradient(125deg,#152a30,#142033);margin:8px 0 19px}.status-card .orb{width:42px;height:42px;border-radius:50%;background:radial-gradient(circle at 32% 30%,#d7fff4,#62adab 45%,#244661 75%);box-shadow:0 0 24px #4eac9b55;flex:none}.status-card strong{display:block}.status-card small{display:block}.tile-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}.tile{position:relative;min-height:130px;text-align:left;overflow:hidden;background:linear-gradient(145deg,#1b2c3d,#121d2c);border:1px solid #344658;border-radius:21px;padding:15px;display:flex;flex-direction:column;justify-content:space-between}.tile:before{content:"";position:absolute;right:-20px;top:-25px;width:115px;height:115px;border-radius:50%;background:radial-gradient(circle,var(--glow),transparent 70%);opacity:.58}.tile .glyph{font-size:28px;line-height:1;color:var(--tone)}.tile strong{display:block;font-size:17px;letter-spacing:-.02em}.tile small{display:block;font-size:11px;line-height:1.25;margin-top:3px}.tile.scenes{--tone:#e8c394;--glow:#a97a36}.tile.games{--tone:#a8cff8;--glow:#547caf}.tile.create{--tone:#dbb2ec;--glow:#a36aae}.tile.text{--tone:#99e2d0;--glow:#4aab8e}.tile.timer{--tone:#e2b5a2;--glow:#b77964}.tile.device{--tone:#b8c8ef;--glow:#7385b6}.head{display:flex;align-items:center;gap:11px;padding:19px 0 8px}.head .back{font-size:21px;width:42px;padding:0}.head h2{margin:0;flex:1}.lead{color:var(--muted);font-size:13px;margin-bottom:15px}.section{margin:20px 0 10px}.section-title{display:flex;justify-content:space-between;align-items:baseline;margin-bottom:10px}.section-title h3{margin:0}.card-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:9px}.scene-card,.game-card{min-height:102px;text-align:left;background:linear-gradient(135deg,#1a2838,#111a2b);border:1px solid #354357;border-radius:18px;padding:13px;display:flex;flex-direction:column;justify-content:space-between}.scene-card .mark,.game-card .mark{font-size:23px;color:var(--accent)}.scene-card strong,.game-card strong{font-size:14px;line-height:1.15}.scene-card small,.game-card small{font-size:11px}.filter-row,.tool-row,.action-row{display:flex;gap:7px;overflow-x:auto;padding:2px 0 6px;scrollbar-width:none}.filter-row button,.tool-row button{white-space:nowrap;flex:none}.panel{background:var(--surface);border:1px solid var(--line);border-radius:20px;padding:16px;margin:10px 0}.row{display:flex;align-items:center;gap:9px}.row>*{min-width:0}.row.between{justify-content:space-between}.stack{display:grid;gap:11px}.form-grid{display:grid;grid-template-columns:repeat(2,1fr);gap:10px}.form-grid .wide{grid-column:1/-1}.stat{display:flex;justify-content:space-between;border-bottom:1px solid #263448;padding:8px 0;gap:10px}.stat:last-child{border:0}.stat span{color:var(--muted)}.stat b{text-align:right}.sheet-wrap{position:fixed;inset:0;background:#030710aa;display:none;z-index:30;align-items:flex-end}.sheet-wrap.open{display:flex}.sheet{background:#142030;border:1px solid #384c5d;border-radius:25px 25px 0 0;padding:15px 18px calc(20px + env(safe-area-inset-bottom));width:100%;max-width:560px;max-height:85vh;overflow:auto;margin:auto auto 0;box-shadow:0 -15px 50px #0009}.grab{width:36px;height:4px;border-radius:9px;background:#63768a;margin:0 auto 16px}.sheet h2{margin-bottom:6px}.sheet p{color:var(--muted);font-size:13px}.toast{position:fixed;left:50%;bottom:20px;transform:translateX(-50%);background:#223645;border:1px solid #5d827f;padding:10px 14px;border-radius:14px;z-index:60;min-width:180px;max-width:90vw;text-align:center;display:none;box-shadow:0 10px 30px #0008}.toast.error{border-color:var(--red);color:#ffe1df}.canvas-wrap{position:relative;background:#09121f;border:1px solid #405268;border-radius:18px;padding:12px;overflow:auto;touch-action:pan-x pan-y}.canvas-wrap canvas{image-rendering:pixelated;display:block;background:#000;touch-action:none}.matrix-canvas{width:100%;max-width:450px;aspect-ratio:3/1}.connector{width:2px;height:22px;background:var(--accent);margin:0 auto;box-shadow:0 0 12px var(--accent)}.tail-overview{width:100%;height:26px}.tail-detail{width:1500px;height:60px}.tail-scroll{overflow-x:auto;border:1px solid #35475b;border-radius:14px;padding:8px;background:#050a11}.tail-scroll canvas{touch-action:none}.editor-top{display:flex;justify-content:space-between;align-items:center;gap:9px}.badge{padding:5px 9px;border-radius:999px;background:#26394b;color:var(--muted);font-size:11px}.pad{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;max-width:310px;margin:10px auto}.pad button{min-height:54px;font-size:16px;touch-action:none}.game-hud{font-size:20px;font-weight:800;text-align:center;padding:14px}.subtle{font-size:11px;color:var(--muted)}.advanced{margin-top:18px}.advanced summary{color:var(--muted);cursor:pointer}pre{overflow:auto;max-height:200px;font-size:11px;background:#080f1c;padding:10px;border-radius:12px}.hidden{display:none!important}@media(max-width:370px){.tile{min-height:116px;padding:12px}.tile strong{font-size:16px}.shell{padding-left:12px;padding-right:12px}}@media(prefers-reduced-motion:reduce){*,*:before{animation:none!important;transition:none!important}}
</style></head><body><div class="shell"><header class="topbar"><span class="brand">LED WALL <span style="color:var(--accent)">V5</span></span><span class="spacer"></span><span class="connection"><span id="dot" class="dot"></span><span id="connectionText">Connecting</span></span></header><main>
<section class="view active" id="view-home"><div class="hero"><div class="eyebrow">ONE BODY · 450 LIGHTS</div><h1>Your wall, alive.</h1><p>Create a moment across the entire Matrix and Tail.</p></div><div class="status-card"><div class="orb"></div><div><strong id="activeLabel">Connecting to wall…</strong><small id="activeDetail">Waiting for device state</small></div><button id="homeStop" class="ghost" aria-label="Stop current program">Stop</button></div><div class="tile-grid">
<button class="tile scenes" data-go="scenes"><span class="glyph">✧</span><span><strong>Scenes</strong><small>Motion & atmosphere</small></span></button><button class="tile games" data-go="games"><span class="glyph">◆</span><span><strong>Games</strong><small>Play together</small></span></button><button class="tile create" data-go="create"><span class="glyph">▦</span><span><strong>Create</strong><small>Draw across 450 lights</small></span></button><button class="tile text" data-go="text"><span class="glyph">Aa</span><span><strong>Text</strong><small>Messages with motion</small></span></button><button class="tile timer" data-go="timer"><span class="glyph">◷</span><span><strong>Timer</strong><small>Count down together</small></span></button><button class="tile device" data-go="device"><span class="glyph">⌘</span><span><strong>Device</strong><small>Connection & settings</small></span></button></div></section>
<section class="view" id="view-scenes"><div class="head"><button class="back" data-go="home" aria-label="Home">‹</button><h2>Scenes</h2></div><p class="lead">Curated motion for the whole wall. Each scene gives Matrix and Tail a role.</p><div class="filter-row" id="sceneFilters"><button class="selected" data-filter="all">All</button><button data-filter="celebrate">Celebrate</button><button data-filter="ambient">Ambient</button><button data-filter="motion">Motion</button></div><div class="card-grid" id="sceneGrid"></div><div class="section"><div class="section-title"><h3>Moments</h3><small>Device-owned timelines</small></div><div class="card-grid" id="partyGrid"></div></div></section>
<section class="view" id="view-games"><div class="head"><button class="back" data-go="home" aria-label="Home">‹</button><h2>Games</h2></div><p class="lead">Twelve families, one connected playfield. Choose a game to see its rules and controls.</p><small id="gameInputMode">Phone controls available</small><div class="filter-row" id="gameFilters"><button class="selected" data-filter="featured">Featured</button><button data-filter="all">All</button><button data-filter="classic">Classics</button></div><div class="card-grid" id="gameGrid"></div></section>
<section class="view" id="view-play"><div class="head"><button class="back" id="gameBack" aria-label="Exit game">‹</button><h2 id="playTitle">Game</h2></div><div class="panel"><div class="game-hud" id="gameHud">Waiting for device…</div><div class="row between"><span class="badge" id="gamePhase">READY</span><span class="subtle" id="playerStatus">Claim your controller slot</span></div></div><div class="panel stack"><div class="row"><button id="claimP1">Claim P1</button><button id="claimP2">Claim P2</button><button id="releasePlayers">Release</button></div><div class="pad" id="gamePad"></div><div class="action-row"><button id="gamePause">Pause</button><button id="gameResume">Resume</button><button id="gameRestart">Restart</button><button class="danger" id="gameExit">Exit</button></div><small>Each phone claims its own slot. Start with A; use Home or Exit to leave.</small></div></section>
<section class="view" id="view-create"><div class="head"><button class="back" data-go="home" aria-label="Home">‹</button><h2>Create</h2><span class="badge" id="pixelStatus">Draft</span></div><p class="lead">Draw one artwork across the Matrix and the connected Tail.</p><div class="canvas-wrap"><canvas id="matrixCanvas" class="matrix-canvas" width="300" height="100" aria-label="Matrix 30 by 10"></canvas><div class="connector"></div><canvas id="tailOverview" class="tail-overview" width="450" height="26" aria-label="Tail overview"></canvas></div><div class="section-title" style="margin-top:14px"><h3>Tail detail</h3><small>Scroll to reach every light</small></div><div class="tail-scroll"><canvas id="tailDetail" class="tail-detail" width="1500" height="60" aria-label="Tail pixels 0 to 149"></canvas></div><div class="panel"><div class="editor-top"><span class="badge" id="pixelToolLabel">Pencil · 1 px</span><button id="pixelTools">Tools</button></div><div class="tool-row"><button data-tool="pencil" class="selected">Pencil</button><button data-tool="eraser">Eraser</button><button data-tool="fill">Fill</button><button data-tool="eyedropper">Pick</button><button data-tool="pan">Pan</button></div><div class="row"><input id="pixelColor" type="color" value="#ff834d" aria-label="Brush color"><label style="margin:0;flex:1">Brush <input id="brushSize" type="range" min="1" max="5" value="1"></label><button id="pixelUndo">Undo</button><button id="pixelRedo">Redo</button></div><div class="row"><label style="margin:0;flex:1">Zoom <input id="pixelZoom" type="range" min="1" max="3" value="1"></label><button id="pixelClear" class="danger">Clear draft</button></div></div><div class="panel stack"><div class="row between"><strong id="pixelTransfer">Local draft</strong><label style="margin:0"><input id="pixelLive" type="checkbox"> Live</label></div><div class="action-row"><button id="pixelSend" class="primary">Send to wall</button><button id="pixelSave">Save draft</button><button id="pixelProjects">Projects</button></div><small id="pixelHint">Send shows Applied only after the ESP32 confirms the exact revision. Save stores in this browser.</small></div></section>
<section class="view" id="view-text"><div class="head"><button class="back" data-go="home" aria-label="Home">‹</button><h2>Text</h2></div><p class="lead">A message on Matrix, with a Tail response tied to its timing.</p><div class="panel stack"><div><label for="textMessage">Message</label><input id="textMessage" maxlength="128" placeholder="Your message"></div><div class="form-grid"><div><label for="textMode">Movement</label><select id="textMode"><option value="fixed">Still</option><option value="marquee">Scroll</option><option value="words">Words</option></select></div><div><label for="textTail">Tail response</label><select id="textTail"><option value="solid" selected>Solid glow</option><option value="text_synced">Synced pulse</option><option value="progress">Progress</option><option value="pulse">Gentle pulse</option><option value="rainbow_flow">Rainbow</option><option value="custom">Custom artwork</option><option value="off">Off</option></select></div><div><label for="textColor">Color</label><select id="textColor"><option value="heritage_green">Green</option><option value="white">White</option><option value="orange">Orange</option><option value="cyan">Cyan</option><option value="pink">Pink</option></select></div><div><label for="textStyle">Style</label><select id="textStyle"><option value="solid">Solid</option><option value="rainbow_text">Rainbow text</option><option value="color_bg">Color background</option></select></div><div class="wide"><label for="textSpeed">Motion speed</label><input id="textSpeed" type="range" min="40" max="500" value="140"></div></div><div class="action-row"><button id="textShow" class="primary">Show on wall</button><button id="textSave">Save preset</button><button id="textPresets">Presets</button></div><canvas id="textPreview" width="300" height="100" style="width:100%;image-rendering:pixelated;background:#000;border-radius:10px" aria-label="Illustrative text preview"></canvas><small>Illustrative preview. The device renderer owns the actual LED frame.</small></div></section>
<section class="view" id="view-timer"><div class="head"><button class="back" data-go="home" aria-label="Home">‹</button><h2>Timer</h2></div><p class="lead">The ESP32 keeps time. Tail shows the remaining journey.</p><div class="panel stack"><div class="game-hud" id="timerReadout">00:00</div><div class="row between"><span class="badge" id="timerPhase">IDLE</span><small>Finale plays when the timer owns the display.</small></div><div class="form-grid"><div><label for="timerMin">Minutes</label><input id="timerMin" type="number" min="0" max="99" value="1"></div><div><label for="timerSec">Seconds</label><input id="timerSec" type="number" min="0" max="59" value="0"></div></div><div class="action-row"><button id="timerStart" class="primary">Start</button><button id="timerPause">Pause</button><button id="timerResume">Resume</button><button id="timerReset">Reset</button><button id="timerStop" class="danger">Stop</button></div></div></section>
<section class="view" id="view-device"><div class="head"><button class="back" data-go="home" aria-label="Home">‹</button><h2>Device</h2></div><p class="lead">Connection, safe brightness and storage truth.</p><div class="panel"><div class="stat"><span>Firmware</span><b id="deviceVersion">—</b></div><div class="stat"><span>Connection</span><b id="deviceConnection">—</b></div><div class="stat"><span>Matrix + Tail</span><b id="deviceBody">—</b></div><div class="stat"><span>Device projects</span><b id="deviceStorage">Checking</b></div><div class="stat"><span>Controllers</span><b id="deviceControllers">—</b></div></div><div class="panel"><div class="row between"><strong>Controllers</strong><span class="badge" id="controllerState">Checking</span></div><small id="controllerDetail">Reading device status…</small><div class="action-row" style="margin-top:10px"><button id="controllerPair">Pair controller</button><button id="controllerP1">Assign P1</button><button id="controllerP2">Assign P2</button></div></div><div class="panel"><label for="brightness">Brightness · ceiling 40</label><input id="brightness" type="range" min="0" max="100" value="0"><small id="brightnessLabel">Device state loading…</small><div class="action-row" style="margin-top:12px"><button id="deviceStandby">Standby</button><button id="deviceWake">Wake</button><button id="deviceClear" class="danger">Clear output</button></div></div><details class="advanced"><summary>Diagnostics</summary><pre id="rawState"></pre><small>Estimated software current limits are not electrical measurements. Physical buzzer is unavailable. Controller status above comes from the device.</small></details></section>
</main></div><div class="sheet-wrap" id="sheetWrap" role="presentation"><div class="sheet" role="dialog" aria-modal="true" aria-labelledby="sheetTitle"><div class="grab"></div><div class="row between"><h2 id="sheetTitle">Details</h2><button id="sheetClose" class="ghost" aria-label="Close">✕</button></div><p id="sheetDescription"></p><div id="sheetContent"></div></div></div><div class="toast" id="toast" role="status"></div><script>
/*LWCORE-START*/
var ANIMATIONS=["corners","checker","border","row_sweep","bounce","wave","rain","plasma","smile","wink","heart","heart_pulse","eyes","loading","success","warning","arrows","aurora","stars","rainbow_flow","fire","matrix","calm","comet","scanner","pulse","breathing","meteor","spark_trail","lightning","fire_flow","progress","countdown_progress","chase","color_wave","heartbeat","fireworks","meteor_impact","energy_bloom","rainbow_river","storm","lava_fountain","rain_drain","aurora_current","starstream"];
var ANIMATION_CATS={"corners":"effects","checker":"effects","border":"effects","row_sweep":"effects","bounce":"effects","wave":"effects","rain":"effects","plasma":"effects","smile":"icons","wink":"icons","heart":"icons","heart_pulse":"icons","eyes":"icons","loading":"icons","success":"icons","warning":"icons","arrows":"icons","aurora":"ambient","stars":"ambient","rainbow_flow":"ambient","fire":"ambient","matrix":"ambient","calm":"ambient","comet":"tail","scanner":"tail","pulse":"tail","breathing":"tail","meteor":"tail","spark_trail":"tail","lightning":"tail","fire_flow":"tail","progress":"tail","countdown_progress":"tail","chase":"tail","color_wave":"tail","heartbeat":"tail","fireworks":"effects","meteor_impact":"effects","energy_bloom":"effects","rainbow_river":"ambient","storm":"effects","lava_fountain":"effects","rain_drain":"ambient","aurora_current":"ambient","starstream":"ambient"};
var ANIMATION_ZONES={"corners":["matrix"],"checker":["matrix"],"border":["matrix"],"row_sweep":["matrix"],"bounce":["matrix"],"wave":["matrix"],"rain":["matrix"],"plasma":["matrix"],"smile":["matrix"],"wink":["matrix"],"heart":["matrix"],"heart_pulse":["matrix"],"eyes":["matrix"],"loading":["matrix"],"success":["matrix"],"warning":["matrix"],"arrows":["matrix"],"aurora":["matrix"],"stars":["matrix"],"rainbow_flow":["matrix","tail","both"],"fire":["matrix"],"matrix":["matrix"],"calm":["matrix"],"comet":["tail"],"scanner":["tail"],"pulse":["tail","both"],"breathing":["tail","both"],"meteor":["tail"],"spark_trail":["tail"],"lightning":["tail","both"],"fire_flow":["tail"],"progress":["tail","both"],"countdown_progress":["tail","both"],"chase":["tail","both"],"color_wave":["tail","both"],"heartbeat":["tail","both"],"fireworks":["both"],"meteor_impact":["both"],"energy_bloom":["both"],"rainbow_river":["both"],"storm":["both"],"lava_fountain":["both"],"rain_drain":["both"],"aurora_current":["both"],"starstream":["both"]};
var EXPRESSIONS=["idle","listening","thinking","speaking","happy","alert","error","sleeping","bored","tired","focused","excited"];
var GAMES=["pong","snake","breakout","reaction","dodger","color_chain","light_race","reaction_stop","color_catch","memory","tug_of_war","tail_tennis","light_quest","quick_draw"];
var GAME_META={"pong":{"zone":"matrix","players":2},"snake":{"zone":"matrix","players":1},"breakout":{"zone":"matrix","players":1},"reaction":{"zone":"matrix","players":1},"dodger":{"zone":"matrix","players":1},"color_chain":{"zone":"both","players":1},"light_race":{"zone":"both","players":2,"minPlayers":2},"reaction_stop":{"zone":"tail","players":2},"color_catch":{"zone":"both","players":2},"memory":{"zone":"both","players":2},"tug_of_war":{"zone":"both","players":2},"tail_tennis":{"zone":"both","players":2},"light_quest":{"zone":"both","players":1},"quick_draw":{"zone":"both","players":2,"minPlayers":2}};
var GAME_MIN_LANE={color_chain:30,light_race:12,reaction_stop:12,color_catch:12,memory:12,
  tug_of_war:12,tail_tennis:12,light_quest:12,quick_draw:12};
var GAME_CONTROLS={pong:["up","down"],snake:["up","down","left","right"],
  breakout:["left","right","confirm"],reaction:["confirm"],dodger:["left","right"],
  color_chain:["left","right","confirm","action_b"],light_race:["confirm","action_b"],
  reaction_stop:["confirm"],color_catch:["confirm"],
  memory:["up","right","down","left"],tug_of_war:["confirm"],
  tail_tennis:["confirm"],light_quest:["left","right","confirm"],quick_draw:["confirm"]};
var INPUTS=["up","down","left","right","confirm","back","start_pause","home","action_b"];
var TEXT_STYLES=["solid","color_bg","rainbow_text","rainbow_bg"];
var TEXT_MODES=["fixed","marquee","words"];
var PALETTE=["black","white","gray","red","orange","yellow","green","cyan","blue","purple","pink","warm_white","heritage_green"];
var PALETTE_RGB={"black":"#000000","white":"#ffffff","gray":"#6e6e6e","red":"#ff0000","orange":"#ff5a00","yellow":"#ffc800","green":"#00a028","cyan":"#00c8c8","blue":"#003cff","purple":"#9600ff","pink":"#ff3c8c","warm_white":"#ff8c3c","heritage_green":"#37ff66"};
// 5x7 column font mirroring firmware/src/text/font5x7.cpp (generated).
var PREVIEW_FONT={"A":[126,9,9,126,0],"E":[127,73,73,73,65],"H":[127,8,8,8,127],"I":[0,65,127,65,0],"L":[127,64,64,64,64],"M":[127,2,12,2,127],"N":[127,4,8,16,127],"O":[62,65,65,65,62],"P":[127,9,9,6,0],"R":[127,9,25,41,70],"T":[0,1,127,1,0],"U":[63,64,64,64,63],"0":[62,65,65,65,62],"8":[127,73,73,73,127],"-":[0,0,8,8,0],"B":[127,73,73,73,54],"C":[126,65,65,65,99],"D":[127,65,65,65,62],"F":[127,9,9,9,1],"G":[126,65,65,73,121],"J":[32,64,65,31,1],"K":[127,8,20,34,65],"Q":[126,65,65,33,94],"S":[70,73,73,57,49],"V":[31,112,64,112,31],"W":[127,40,28,40,127],"X":[99,20,8,20,99],"Y":[3,4,120,4,3],"Z":[65,81,73,69,67],"1":[0,66,127,64,0],"2":[66,97,89,69,71],"3":[65,73,73,73,54],"4":[24,20,18,127,0],"5":[7,73,73,89,113],"6":[62,73,73,73,113],"7":[1,1,121,5,3],"9":[6,73,73,73,119],".":[0,64,0,0,0],"!":[0,0,95,0,0],"a":[32,84,84,84,120],"b":[127,72,72,72,48],"c":[48,72,72,72,0],"d":[48,72,72,72,127],"e":[56,68,84,84,88],"f":[8,126,9,9,0],"g":[24,36,36,36,124],"h":[127,4,4,4,120],"i":[0,0,122,0,0],"j":[0,64,64,64,122],"k":[127,16,40,108,68],"l":[0,0,127,0,0],"m":[120,8,120,8,120],"n":[120,8,8,8,112],"o":[48,72,72,72,48],"p":[124,72,72,72,48],"q":[48,72,72,72,124],"r":[120,8,8,0,0],"s":[80,104,104,104,8],"t":[4,127,68,68,0],"u":[120,64,64,64,124],"v":[28,32,64,32,28],"w":[120,64,32,64,120],"x":[8,80,32,80,8],"y":[24,24,24,24,120],"z":[72,72,104,56,72],",":[0,0,0,64,32],"?":[2,1,81,9,6],":":[0,0,36,0,0],";":[0,0,36,32,64],"'":[0,0,3,0,0],"\"":[0,3,0,3,0],"(":[0,56,34,65,0],")":[65,34,56,0,0],"+":[16,16,124,16,16],"=":[72,72,72,72,72],"%":[38,16,8,4,50],"/":[16,8,4,2,1],"*":[4,8,30,8,4],"#":[20,62,20,62,20],"_":[64,64,64,64,64],"<":[0,8,20,34,0],">":[0,34,20,8,0],"0xCB":[126,75,74,75,66],"0xEB":[56,69,84,85,88],"0xC7":[62,33,97,33,33],"0xE7":[24,36,100,36,36]};

var WS_EVENTS=["state.changed","mode.changed","transient.expired","text.done","timer.done"];
var ERROR_TEXT={textTooWide:"Text is too wide for a static show - use marquee.",unknownScene:"Unknown scene.",unknownAnimation:"Unknown animation.",unknownExpression:"Unknown expression.",unknownGame:"Unknown game.",invalidParameter:"The device rejected a parameter.",unsupportedCapability:"This device does not support that capability.",ledUnavailable:"The tail zone is disabled on the device.",internalError:"Device internal error."};
function clampInt(v,lo,hi){v=Math.floor(Number(v));if(!isFinite(v))return NaN;return Math.min(hi,Math.max(lo,v));}

// Payload builders (wire shapes pinned by tests; bounds per CONTRACT.md).
function buildTextShow(o){
  var b={text:String(o.text==null?"":o.text)};
  if(b.text.length<1||b.text.length>128)throw new Error("text must be 1..128 characters");
  if(o.mode!=null){if(TEXT_MODES.indexOf(o.mode)<0)throw new Error("bad mode");b.mode=o.mode;}
  if(o.rtl!=null)b.direction=o.rtl?"rtl":"ltr";
  if(o.loop!=null)b.loop=!!o.loop;
  if(o.speedMsPerPx!=null)b.speedMsPerPx=clampInt(o.speedMsPerPx,40,500);
  if(o.wordDwellMs!=null)b.wordDwellMs=clampInt(o.wordDwellMs,200,60000);
  if(o.style!=null){if(TEXT_STYLES.indexOf(o.style)<0)throw new Error("bad style");b.style=o.style;}
  if(o.color!=null){if(PALETTE.indexOf(o.color)<0)throw new Error("bad color");b.color=o.color;}
  if(o.bgColor!=null){if(PALETTE.indexOf(o.bgColor)<0)throw new Error("bad bgColor");b.bgColor=o.bgColor;}
  if(o.tailMode!=null){if(["off","solid","rainbow_flow","pulse","progress","text_synced","custom"].indexOf(o.tailMode)<0)throw new Error("bad tailMode");b.tailMode=o.tailMode;}
  if(o.tailColor!=null){if(PALETTE.indexOf(o.tailColor)<0)throw new Error("bad tailColor");b.tailColor=o.tailColor;}
  if(o.durationMs!=null)b.durationMs=clampInt(o.durationMs,1,2147483647);
  return b;
}
function buildTimerSet(ms){
  var raw=Math.floor(Number(ms));
  if(!isFinite(raw)||raw<1000)throw new Error("duration must be at least 1 s");
  if(raw>5999000)throw new Error("duration above the 99:59 display cap");
  return{durationMs:raw};
}
function buildNotify(o){
  var b={text:String(o.text==null?"":o.text)};
  if(b.text.length<1||b.text.length>128)throw new Error("text must be 1..128 characters");
  if(o.durationMs!=null){
    var raw=Math.floor(Number(o.durationMs));
    if(!isFinite(raw)||raw<1000||raw>300000)throw new Error("notification duration must be 1..300 s");
    b.durationMs=raw;
  }
  if(o.priority==="high")b.priority="high";
  return b;
}
function buildGameSet(name,players){
  if(GAMES.indexOf(name)<0)throw new Error("unknown game");
  if((GAME_META[name]||{}).minPlayers===2&&players!==2)throw new Error("game requires 2 players");
  var b={game:name};
  if(players!=null){
    var p=clampInt(players,1,2);
    if(p!==players)throw new Error("players must be 1 or 2");
    if(p===2&&(GAME_META[name]||{}).players<2)throw new Error("game does not support 2 players");
    b.players=p;
  }
  return b;
}
var GAME_DIFFICULTIES=["easy","normal","hard"];
function buildGameDifficulty(name){if(GAME_DIFFICULTIES.indexOf(name)<0)throw new Error("bad difficulty");return{difficulty:name};}
function buildInput(name,player){
  if(INPUTS.indexOf(name)<0)throw new Error("bad input");
  var b={input:name};
  if(player!=null){
    var p=clampInt(player,1,2);
    if(p!==player)throw new Error("player must be 1 or 2");
    b.player=p;
  }
  return b;
}
// LED-L010: zone target on animation/scene selection (validated against the
// registry zone metadata the device also enforces).
function buildAnimationSet(name,target){
  var zones=ANIMATION_ZONES[name];
  if(!zones)throw new Error("unknown animation");
  var b={animation:name};
  if(target!=null){
    if(zones.indexOf(target)<0)throw new Error("animation does not support target "+target);
    b.target=target;
  }
  return b;
}
function buildStandby(on){return{on:!!on};}

// Countdown display format mirroring the device (MM:SS / SS / S.mmm).
function countdownFormat(ms){
  ms=Math.max(0,Math.floor(ms));
  var totalSec=Math.floor(ms/1000);
  if(totalSec>=60){var m=Math.floor(totalSec/60),s=totalSec%60;
    return (m<10?"0":"")+m+":"+(s<10?"0":"")+s;}
  if(totalSec>=10)return String(totalSec);
  return totalSec+"."+String(ms%1000).padStart(3,"0");
}
// Local interpolation: the device reports exact remainingMs at fetch time;
// the UI advances it locally between updates (device stays authoritative).
function timerLocal(timer,fetchedAtMs,nowMs){
  if(!timer)return{state:"idle",remainingMs:0,durationMs:0};
  var r=timer.remainingMs;
  if(timer.state==="running"&&typeof fetchedAtMs==="number"){
    r=Math.max(0,timer.remainingMs-Math.max(0,nowMs-fetchedAtMs));
  }
  return{state:timer.state,remainingMs:r,durationMs:timer.durationMs};
}

function normalizeState(raw){
  return{
    available:!!raw.available,
    deviceId:raw.deviceId||"",
    fwVersion:raw.fwVersion||"",
    baseMode:raw.baseMode||"idle",
    currentMode:raw.currentMode||"idle",
    activeScene:raw.activeScene||null,
    activeAnimation:raw.activeAnimation||null,
    brightness:raw.brightness||{current:0,max:100},
    text:raw.text||null,
    timer:raw.timer||{state:"idle",durationMs:0,remainingMs:0},
    menu:raw.menu||{active:false},
    game:raw.game||{active:false,name:"",phase:"",score:0},
    standby:raw.standby||{on:false},
    notification:raw.notification||{active:false},
    visualizer:raw.visualizer||{active:false},
    tail:raw.tail||{enabled:false,length:0,reverse:false},
    animationTarget:raw.animationTarget||null,
    ds4:raw.ds4||{supported:false,enabled:false,state:"unsupported",players:0},
    clients:(raw.clients||{}).count||0,
    warnings:raw.warnings||[],
    uptimeSeconds:raw.uptimeSeconds||0,
    matrix:raw.matrix||{width:30,height:10}
  };
}

// Bounded favorites (browser-local, never device state; cap 20).
function makeFavorites(storage){
  var KEY="ledwall.favorites.v2";
  function load(){try{return JSON.parse(storage.getItem(KEY))||[]}catch(e){return[]}}
  function save(list){storage.setItem(KEY,JSON.stringify(list.slice(0,20)))}
  return{
    list:load,
    add:function(label,payload){
      var l=load().filter(function(f){return f.label!==label});
      l.unshift({label:String(label).slice(0,32),payload:payload});
      save(l);return l;
    },
    remove:function(label){save(load().filter(function(f){return f.label!==label}))}
  };
}

// 30x10 local preview mirroring the device Text Studio (fixed/marquee/words
// x solid/color_bg/rainbow_text/rainbow_bg). Returns 300 RGB arrays.
function utf8cp(s){var cp=[];var i=0;while(i<s.length){var c=s.codePointAt(i);cp.push(c);i+=c>0xFFFF?2:1;}return cp;}
function previewPixels(text,o,elapsedMs){
  var W=30,H=10;
  var px=new Array(W*H);
  for(var i=0;i<W*H;i++)px[i]=[0,0,0];
  function hex(c){var h=PALETTE_RGB[c]||"#000000";return[parseInt(h.substr(1,2),16),parseInt(h.substr(3,2),16),parseInt(h.substr(5,2),16)];}
  var fg=hex(o.color||"heritage_green"),bg=hex(o.bgColor||"black");
  function lum(c){return 299*c[0]+587*c[1]+114*c[2];}
  if(o.style==="color_bg"&&lum(bg)>=400)fg=[0,0,0];
  function hsv(h,s,v){h=((h%256)+256)%256;var sec=Math.floor(h/43),off=h%43,ramp=Math.floor(off*255/42);
    var lo=Math.floor(v*(255-s)/255),amp=v-lo,r=lo,g=lo,b=lo;
    if(sec===0){r=v;g=lo+Math.floor(amp*ramp/255);}
    else if(sec===1){r=lo+Math.floor(amp*(255-ramp)/255);g=v;}
    else if(sec===2){g=v;b=lo+Math.floor(amp*ramp/255);}
    else if(sec===3){g=lo+Math.floor(amp*(255-ramp)/255);b=v;}
    else if(sec===4){r=lo+Math.floor(amp*ramp/255);b=v;}
    else{r=v;b=lo+Math.floor(amp*(255-ramp)/255);}
    return[r,g,b];}
  function paintBg(){
    if(o.style==="rainbow_bg"){for(var y=0;y<H;y++)for(var x=0;x<W;x++)px[y*W+x]=hsv(x*4+Math.floor(elapsedMs/8),255,90);}
    else if(o.style==="color_bg"){for(var y2=0;y2<H;y2++)for(var x2=0;x2<W;x2++)px[y2*W+x2]=bg;}
  }
  function drawGlyph(cp,x0){var g=PREVIEW_FONT[cp];if(!g&&cp<128)g=PREVIEW_FONT[String.fromCharCode(cp)];
    if(!g&&cp>127){var hx=cp.toString(16).toUpperCase();if(hx.length%2)hx="0"+hx;
      g=PREVIEW_FONT["0x"+hx];}
    if(!g)return;
    for(var col=0;col<5;col++)for(var row=0;row<7;row++){
      if(g[col]&(1<<row)){var x=x0+col,y=2+row;if(x<0||x>=W||y<0||y>=H)continue;
        px[y*W+x]=o.style==="rainbow_text"?hsv(x*8+Math.floor(elapsedMs/4),255,255):fg;}}}
  function drawString(str,x0){var cps=utf8cp(str);for(var i=0;i<cps.length;i++)drawGlyph(cps[i],x0+i*6);}
  function width(str){return utf8cp(str).length*6;}
  function center(str){return Math.max(0,Math.floor((W-width(str))/2));}
  var mode=o.mode||"fixed";
  if(mode==="marquee"){
    paintBg();
    var w=width(text),span=w+W;
    var off=Math.floor(elapsedMs/Math.max(1,o.speedMsPerPx||140))%span;
    var x0=(o.rtl!==false)?(W-off):(off-w);
    drawString(text,x0);
  }else if(mode==="words"){
    paintBg();
    var words=text.split(/\s+/).filter(Boolean);
    var dwell=o.wordDwellMs||2000;
    if(words.length){
      var idx=Math.floor(elapsedMs/dwell);
      if(o.loop!==false)idx=idx%words.length;
      else if(idx>=words.length){return px;}
      var wd=words[idx];drawString(wd,center(wd));
    }
  }else{
    paintBg();
    drawString(text,center(text));
  }
  return px;
}
var LWCORE={ANIMATIONS:ANIMATIONS,ANIMATION_CATS:ANIMATION_CATS,ANIMATION_ZONES:ANIMATION_ZONES,
EXPRESSIONS:EXPRESSIONS,GAMES:GAMES,GAME_META:GAME_META,INPUTS:INPUTS,
TEXT_STYLES:TEXT_STYLES,TEXT_MODES:TEXT_MODES,PALETTE:PALETTE,PALETTE_RGB:PALETTE_RGB,
WS_EVENTS:WS_EVENTS,ERROR_TEXT:ERROR_TEXT,clampInt:clampInt,buildTextShow:buildTextShow,
buildTimerSet:buildTimerSet,buildNotify:buildNotify,buildGameSet:buildGameSet,
buildInput:buildInput,buildAnimationSet:buildAnimationSet,buildStandby:buildStandby,
countdownFormat:countdownFormat,timerLocal:timerLocal,normalizeState:normalizeState,
makeFavorites:makeFavorites,previewPixels:previewPixels,PREVIEW_FONT:PREVIEW_FONT,
GAME_DIFFICULTIES:GAME_DIFFICULTIES,buildGameDifficulty:buildGameDifficulty};
/*LWCORE-END*/

(function(){'use strict';
var $=id=>document.getElementById(id),state=null,online=false,view='home',selectedGame='',sceneFilter='all',gameFilter='featured';
var pending=0,toastTimer=0,ws=null,wsDelay=1000,lastStateAt=0;
var effectCards=[
 ['fireworks','Fireworks','Celebrate','✹','A white rocket rises through Tail and bursts on Matrix.'],
 ['meteor_impact','Meteor Impact','Motion','☄','Impact at the joint sends energy down Tail.'],
 ['energy_bloom','Energy Bloom','Celebrate','✧','Energy climbs into an expanding Matrix bloom.'],
 ['rainbow_river','Rainbow River','Ambient','◒','One continuous color current through the whole body.'],
 ['storm','Storm','Motion','ϟ','Lightning branches from Matrix into Tail.'],
 ['lava_fountain','Lava Fountain','Celebrate','♨','Warm light rises and fans out above the joint.'],
 ['rain_drain','Rain Drain','Ambient','☂','Rain gathers at the attachment and drains away.'],
 ['aurora_current','Aurora Current','Ambient','≈','A quiet field with a shared slow color phase.'],
 ['starstream','Starstream','Motion','⋆','Light travels upward to become a field of stars.'],
 ['heartbeat','Heartbeat','Ambient','♡','A paired rhythm passes through both regions.']
];
var moments=[['welcome','Welcome','A greeting across the body.','✦'],['birthday','Birthday','A playful celebration.','✷'],['celebration','Celebration','A bright moment.','✹'],['game_night','Game Night','A welcome into play.','◆'],['calm','Calm','A softer mood.','◒'],['countdown_finale','Countdown Finale','A final shared burst.','◷']];
var games=[
 ['pong','Pong','Featured','Matrix court; Tail tracks each rally. Up/Down, A to start.'],
 ['tail_tennis','Tail Tennis','Featured','Tail ball and hit windows; Matrix score. A to hit.'],
 ['light_race','Light Race','Featured','Two players race on Tail. Matrix shows the result. A/B to move.'],
 ['reaction','Reaction','Featured','Stop the Matrix clock close to zero. A to stop.'],
 ['reaction_stop','Reaction Stop','Featured','Catch the target on Tail. Matrix tracks score.'],
 ['quick_draw','Quick Draw','Featured','Two player timing duel. Press A at GO.'],
 ['tug_of_war','Tug of War','Featured','Pull the Tail marker with A; Matrix counts rounds.'],
 ['memory','Memory','Featured','Repeat Tail sequences with directional controls.'],
 ['color_chain','Color Chain','Featured','Match grouped Tail colors; Matrix shows your choices.'],
 ['light_quest','Light Quest','Featured','Five Tail stages, with lives on Matrix.'],
 ['breakout','Breakout','Classic','Break bricks on Matrix; Tail shows remaining lives.'],
 ['snake','Snake','Classic','Grow on Matrix; Tail shows length and food position.'],
 ['dodger','Dodger','Classic','Avoid hazards on Matrix; Tail shows survival and danger.'],
 ['color_catch','Color Catch','Classic','Match Matrix targets to Tail color and timing.']
];
var $toast=$('toast');
function say(message,error){$toast.textContent=message;$toast.className='toast'+(error?' error':'');$toast.style.display='block';clearTimeout(toastTimer);toastTimer=setTimeout(()=>$toast.style.display='none',3300);}
function busy(button,work){if(button)button.disabled=true;return Promise.resolve().then(work).finally(()=>{if(button)button.disabled=false;});}
async function get(path){let response=await fetch(path,{cache:'no-store'});if(!response.ok)throw Error('HTTP '+response.status);return response.json();}
async function api(path,body){pending++;try{let response=await fetch(path,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body||{}),cache:'no-store'});let result=await response.json();if(!response.ok||!result.ok)throw Error((result.error&&result.error.message)||'Device rejected command');return result;}finally{pending--;}}
function command(path,body,label,button){return busy(button,()=>api(path,body).then(r=>{say(label||'Confirmed by wall');refresh();return r;}).catch(e=>{say(e.message,true);throw e;}));}
function go(next){if(!$("view-"+next))return;document.querySelectorAll('.view').forEach(el=>el.classList.toggle('active',el.id==='view-'+next));view=next;window.scrollTo(0,0);if(next==='create')drawPixels();if(next==='play')renderGame();}
document.querySelectorAll('[data-go]').forEach(b=>b.addEventListener('click',()=>go(b.dataset.go)));
function closeSheet(){$('sheetWrap').classList.remove('open');$('sheetContent').replaceChildren();}
$('sheetClose').onclick=closeSheet;$('sheetWrap').addEventListener('click',e=>{if(e.target===$('sheetWrap'))closeSheet();});
function sheet(title,description,content){$('sheetTitle').textContent=title;$('sheetDescription').textContent=description;$('sheetContent').replaceChildren(content);$('sheetWrap').classList.add('open');}
function el(tag,className,text){let n=document.createElement(tag);if(className)n.className=className;if(text!=null)n.textContent=text;return n;}
function action(label,fn,primary){let b=el('button',primary?'primary':'',label);b.onclick=fn;return b;}
function drawScenes(){let grid=$('sceneGrid');grid.replaceChildren();effectCards.filter(s=>sceneFilter==='all'||s[2].toLowerCase()===sceneFilter).forEach(s=>{let b=el('button','scene-card');b.append(el('span','mark',s[3]),el('strong','',s[1]),el('small','',s[2]));b.onclick=()=>{let box=el('div','stack');let row=el('div','action-row');row.append(action('Play on wall',()=>busy(row.firstChild,()=>api('/display/animation/set',{animation:s[0],target:'both'}).then(()=>{say(s[1]+' playing');closeSheet();refresh();}).catch(e=>say(e.message,true))),true),action('Stop',()=>command('/display/clear',{},'Stopped').then(closeSheet)));box.append(row);sheet(s[1],s[4],box);};grid.append(b);});}
function drawMoments(){let grid=$('partyGrid');grid.replaceChildren();moments.forEach(s=>{let b=el('button','scene-card');b.append(el('span','mark',s[3]),el('strong','',s[1]),el('small','','Party scene'));b.onclick=()=>{let box=el('div','stack');if(s[0]==='game_night'){let select=el('select');games.forEach(g=>{let option=el('option','',g[1]);option.value=g[0];select.append(option);});select.id='momentGame';box.append(select);}box.append(action('Play moment',()=>{let req={preset:s[0]};if(s[0]==='game_night')req.game=$('momentGame').value;return command('/display/party/play',req,s[1]+' playing').then(closeSheet);},true),action('Stop',()=>command('/display/party/stop',{},'Moment stopped').then(closeSheet)));sheet(s[1],s[2],box);};grid.append(b);});}
$('sceneFilters').querySelectorAll('button').forEach(b=>b.onclick=()=>{sceneFilter=b.dataset.filter;$('sceneFilters').querySelectorAll('button').forEach(x=>x.classList.toggle('selected',x===b));drawScenes();});drawScenes();drawMoments();
function drawGames(){let grid=$('gameGrid');grid.replaceChildren();games.filter(g=>gameFilter==='all'||(gameFilter==='featured'?g[2]==='Featured':g[2]==='Classic')).forEach(g=>{let b=el('button','game-card');b.append(el('span','mark','◆'),el('strong','',g[1]),el('small','',g[2]));b.onclick=()=>openGame(g);grid.append(b);});}
$('gameFilters').querySelectorAll('button').forEach(b=>b.onclick=()=>{gameFilter=b.dataset.filter;$('gameFilters').querySelectorAll('button').forEach(x=>x.classList.toggle('selected',x===b));drawGames();});drawGames();
function openGame(g){selectedGame=g[0];let meta=GAME_META[g[0]]||{},box=el('div','stack'),players=el('select'),diff=el('select');let allowed=meta.players===2?[1,2]:[1];if(meta.minPlayers===2)allowed=[2];allowed.forEach(n=>{let o=el('option','',n+' player'+(n===2?'s':''));o.value=String(n);players.append(o);});['easy','normal','hard'].forEach(d=>{let o=el('option','',d[0].toUpperCase()+d.slice(1));o.value=d;diff.append(o);});box.append(el('label','','Players'),players,el('label','','Difficulty'),diff);box.append(action('Start game',()=>busy(box.lastChild,async()=>{try{await api('/display/game/difficulty',{difficulty:diff.value});await api('/display/game/set',buildGameSet(g[0],Number(players.value)));closeSheet();$('playTitle').textContent=g[1];go('play');say('Game ready · press A to begin');refresh();}catch(e){say(e.message,true);}}),true));sheet(g[1],g[3],box);}
var slots={1:{token:0,seq:0,chain:Promise.resolve()},2:{token:0,seq:0,chain:Promise.resolve()}};
function slotMessage(){let claimed=[1,2].filter(n=>slots[n].token).map(n=>'Phone P'+n),mask=state&&state.ds4&&state.ds4.assignedMask||0;if(mask&1)claimed.unshift('DS4 P1');if(mask&2)claimed.push('DS4 P2');$('playerStatus').textContent=claimed.length?claimed.join(' & '):'Claim a controller slot';drawPad();}
async function claim(player){try{let r=await api('/display/input/session',{player});if(r.player!==player||!Number.isInteger(r.session)||!r.session)throw Error('Invalid session ACK');slots[player]={token:r.session,seq:0,chain:Promise.resolve()};slotMessage();say('P'+player+' claimed');}catch(e){say('P'+player+': '+e.message,true);}}
$('claimP1').onclick=()=>claim(1);$('claimP2').onclick=()=>claim(2);
function input(player,name,pressed){let s=slots[player];if(!s.token){say('Claim P'+player+' first',true);return;}let req={player,session:s.token,sequence:++s.seq,input:name,state:pressed?'press':'release'};s.chain=s.chain.then(()=>api('/display/input/event',req)).catch(e=>{s.token=0;slotMessage();say('P'+player+' session ended: '+e.message,true);});}
function releaseAll(){[1,2].forEach(player=>{let s=slots[player];if(!s.token)return;let token=s.token;s.chain.then(()=>api('/display/input/release',{player,session:token})).catch(()=>{});s.token=0;});slotMessage();}
$('releasePlayers').onclick=releaseAll;$('gameBack').onclick=()=>command('/display/game/command',{action:'exit'},'Game exited').then(()=>{releaseAll();go('games');});
setInterval(()=>{if(document.hidden)return;[1,2].forEach(player=>{let s=slots[player];if(!s.token)return;let token=s.token,sequence=++s.seq;s.chain=s.chain.then(()=>{if(s.token===token)return api('/display/input/heartbeat',{player,session:token,sequence});}).catch(()=>{s.token=0;slotMessage();say('P'+player+' disconnected',true);});});},5000);
function drawPad(){let pad=$('gamePad'),keys=GAME_CONTROLS[selectedGame]||['confirm'],active=slots[2].token&&!slots[1].token?2:1;if(pad._s===selectedGame+active)return;pad._s=selectedGame+active;pad.replaceChildren();let layout=[['up','↑'],['confirm','A'],['action_b','B'],['left','←'],['down','↓'],['right','→']];layout.forEach(([key,label])=>{let b=el('button','',label);b.disabled=!keys.includes(key)&&key!=='confirm';b.setAttribute('aria-label',key);b.addEventListener('pointerdown',e=>{e.preventDefault();b.setPointerCapture(e.pointerId);input(active,key,true);});let done=e=>{e.preventDefault();input(active,key,false);};b.addEventListener('pointerup',done);b.addEventListener('pointercancel',done);pad.append(b);});}
function renderGame(){let g=state&&state.game;if(!g||!g.active){$('gameHud').textContent='Choose a game';$('gamePhase').textContent='IDLE';return;}$('playTitle').textContent=games.find(x=>x[0]===g.name)?.[1]||g.name;$('gameHud').textContent=g.players===2?'P1 '+(g.scoreP1||0)+'  :  P2 '+(g.scoreP2||0):'Score '+(g.score||0);$('gamePhase').textContent=(g.phase||'ready').toUpperCase();$('gamePause').disabled=g.phase!=='running';$('gameResume').disabled=g.phase!=='paused';selectedGame=g.name;drawPad();}
[['gamePause','pause'],['gameResume','resume'],['gameRestart','restart'],['gameExit','exit']].forEach(([id,a])=>{$(id).onclick=()=>command('/display/game/command',{action:a},a==='exit'?'Game exited':'Game '+a+' confirmed').then(()=>{if(a==='exit'){releaseAll();go('games');}});});
$('homeStop').onclick=()=>command('/display/clear',{},'Wall cleared');
function textPayload(){return buildTextShow({text:$('textMessage').value,mode:$('textMode').value,style:$('textStyle').value,color:$('textColor').value,tailMode:$('textTail').value,tailColor:$('textColor').value,speedMsPerPx:Number($('textSpeed').value),rtl:true,loop:true});}
function textDraft(){return{text:$('textMessage').value,mode:$('textMode').value,style:$('textStyle').value,color:$('textColor').value,tailMode:$('textTail').value,speed:$('textSpeed').value};}
function renderTextPreview(){let canvas=$('textPreview'),ctx=canvas.getContext('2d'),draft=textDraft(),pixels=previewPixels(draft.text||'',{mode:draft.mode,style:draft.style,color:draft.color,bgColor:'black',rtl:true,speedMsPerPx:Number(draft.speed)},0);ctx.fillStyle='#000';ctx.fillRect(0,0,300,100);pixels.forEach((p,i)=>{if(p===0||p==='#000000')return;ctx.fillStyle=typeof p==='string'?p:'#6ad59c';ctx.fillRect((i%30)*10,Math.floor(i/30)*10,9,9);});}
['textMessage','textMode','textStyle','textColor','textTail','textSpeed'].forEach(id=>$(id).addEventListener('input',renderTextPreview));$('textMode').addEventListener('change',()=>{$('textTail').value=$('textMode').value==='fixed'?'solid':'text_synced';});renderTextPreview();
$('textShow').onclick=()=>{try{command('/display/text/show',textPayload(),'Text shown on wall',$('textShow'));}catch(e){say(e.message,true);}};
var textPresets=loadStore('ledwall-v5-text',{});$('textSave').onclick=()=>{let draft=textDraft();if(!draft.text.trim()){say('Write a message first',true);return;}let name=draft.text.trim().slice(0,24);textPresets[name]=draft;saveStore('ledwall-v5-text',textPresets);say('Saved in this browser');};
$('textPresets').onclick=()=>{let box=el('div','stack');Object.keys(textPresets).forEach(name=>{box.append(action(name,()=>{let p=textPresets[name];$('textMessage').value=p.text;$('textMode').value=p.mode;$('textStyle').value=p.style;$('textColor').value=p.color;$('textTail').value=p.tailMode;$('textSpeed').value=p.speed;renderTextPreview();closeSheet();}));});if(!box.children.length)box.append(el('small','','No saved messages yet.'));sheet('Saved text','These presets stay in this browser.',box);};
function timerDuration(){return buildTimerSet((Math.max(0,Number($('timerMin').value)||0)*60+Math.max(0,Number($('timerSec').value)||0))*1000);}
$('timerStart').onclick=()=>{try{busy($('timerStart'),async()=>{try{let t=state&&state.timer;if(!t||t.state==='idle'||t.state==='finished'){await api('/display/timer/set',timerDuration());}await api('/display/timer/start',{});say('Timer started');refresh();}catch(e){say(e.message,true);}});}catch(e){say(e.message,true);}};
[['timerPause','pause'],['timerResume','resume'],['timerReset','reset'],['timerStop','stop']].forEach(([id,a])=>{$(id).onclick=()=>command('/display/timer/'+a,{},'Timer '+a+' confirmed');});
function updateTimer(){let t=state&&state.timer;if(!t)return;let value=timerLocal(t,lastStateAt,performance.now());$('timerReadout').textContent=countdownFormat(value.remainingMs);$('timerPhase').textContent=value.state.toUpperCase();}
$('brightness').onchange=()=>command('/display/brightness/set',{level:Number($('brightness').value)},'Brightness confirmed');
$('deviceStandby').onclick=()=>command('/display/standby/set',{on:true},'Standby on');$('deviceWake').onclick=()=>command('/display/standby/set',{on:false},'Wall awake');$('deviceClear').onclick=()=>command('/display/clear',{},'Wall cleared');
function loadStore(key,fallback){try{return JSON.parse(localStorage.getItem(key))||fallback;}catch(e){return fallback;}}
function saveStore(key,value){try{localStorage.setItem(key,JSON.stringify(value));return true;}catch(e){say('Browser storage unavailable; export your work',true);return false;}}
var px={cells:Array(450).fill('000000'),undo:[],redo:[],tool:'pencil',size:1,session:0,rev:0,inflight:false,live:false,lastLive:0,dirty:false,generation:0};
var saved=loadStore('ledwall-v5-pixels',{}),draft=loadStore('ledwall-v5-draft',null);if(Array.isArray(draft)&&draft.length===450&&draft.every(c=>/^[0-9a-f]{6}$/i.test(c)))px.cells=draft.slice();
function pxSnapshot(){px.undo.push(px.cells.slice());if(px.undo.length>20)px.undo.shift();px.redo=[];}
function pxChanged(){px.generation++;px.dirty=true;$('pixelStatus').textContent='Draft';$('pixelTransfer').textContent='Unsaved changes';saveStore('ledwall-v5-draft',px.cells);drawPixels();if(px.live&&online&&performance.now()-px.lastLive>120){px.lastLive=performance.now();sendPixels(true);}}
function drawPixels(){let m=$('matrixCanvas'),ctx=m.getContext('2d');ctx.fillStyle='#000';ctx.fillRect(0,0,300,100);for(let i=0;i<300;i++){ctx.fillStyle='#'+px.cells[i];ctx.fillRect(i%30*10,Math.floor(i/30)*10,10,10);}let t=$('tailDetail'),tc=t.getContext('2d');tc.fillStyle='#000';tc.fillRect(0,0,1500,60);for(let i=0;i<150;i++){tc.fillStyle='#'+px.cells[300+i];tc.fillRect(i*10,0,10,60);}let o=$('tailOverview'),oc=o.getContext('2d');oc.fillStyle='#000';oc.fillRect(0,0,450,26);for(let i=0;i<150;i++){oc.fillStyle='#'+px.cells[300+i];oc.fillRect(i*3,0,3,26);}}
function cellFor(canvas,event){let r=canvas.getBoundingClientRect(),x=Math.floor((event.clientX-r.left)*canvas.width/r.width),y=Math.floor((event.clientY-r.top)*canvas.height/r.height);if(canvas.id==='matrixCanvas'){if(x<0||x>=300||y<0||y>=100)return -1;return Math.floor(y/10)*30+Math.floor(x/10);}let i=Math.floor(x/(canvas.id==='tailOverview'?3:10));return i>=0&&i<150?300+i:-1;}
function paintAt(index){if(index<0)return;if(px.tool==='eyedropper'){$('pixelColor').value='#'+px.cells[index];return;}if(px.tool==='pan')return;let color=px.tool==='eraser'?'000000':$('pixelColor').value.slice(1).toLowerCase();if(px.tool==='fill'){let old=px.cells[index];if(old===color)return;let q=[index],seen=new Set();while(q.length){let i=q.pop();if(seen.has(i)||px.cells[i]!==old)continue;seen.add(i);px.cells[i]=color;if(i<300){let x=i%30;if(x>0)q.push(i-1);if(x<29)q.push(i+1);if(i>=30)q.push(i-30);if(i<270)q.push(i+30);}else{if(i>300)q.push(i-1);if(i<449)q.push(i+1);}}return;}if(index<300){let x=index%30,y=Math.floor(index/30),radius=Math.floor((px.size-1)/2);for(let dy=-radius;dy<=radius;dy++)for(let dx=-radius;dx<=radius;dx++){let xx=x+dx,yy=y+dy;if(xx>=0&&xx<30&&yy>=0&&yy<10)px.cells[yy*30+xx]=color;}}else{for(let d=0;d<px.size;d++){let k=index+d;if(k<450)px.cells[k]=color;}}}
['matrixCanvas','tailDetail','tailOverview'].forEach(id=>{let canvas=$(id),drawing=false;canvas.addEventListener('pointerdown',e=>{if(px.tool==='pan')return;e.preventDefault();canvas.setPointerCapture(e.pointerId);drawing=true;pxSnapshot();paintAt(cellFor(canvas,e));pxChanged();});canvas.addEventListener('pointermove',e=>{if(!drawing||px.tool==='fill'||px.tool==='eyedropper')return;e.preventDefault();paintAt(cellFor(canvas,e));pxChanged();});function end(){if(!drawing)return;drawing=false;if(px.live&&online)sendPixels(true);}canvas.addEventListener('pointerup',end);canvas.addEventListener('pointercancel',end);});
document.querySelectorAll('[data-tool]').forEach(b=>b.onclick=()=>{px.tool=b.dataset.tool;document.querySelectorAll('[data-tool]').forEach(x=>x.classList.toggle('selected',x===b));$('pixelToolLabel').textContent=b.textContent+' · '+px.size+' px';$('matrixCanvas').style.touchAction=px.tool==='pan'?'auto':'none';$('tailDetail').style.touchAction=px.tool==='pan'?'auto':'none';});
$('brushSize').oninput=()=>{px.size=Number($('brushSize').value);$('pixelToolLabel').textContent=px.tool+' · '+px.size+' px';};$('pixelZoom').oninput=()=>{$('matrixCanvas').style.width=(Number($('pixelZoom').value)*100)+'%';};
$('pixelUndo').onclick=()=>{if(!px.undo.length)return;px.redo.push(px.cells.slice());px.cells=px.undo.pop();pxChanged();};$('pixelRedo').onclick=()=>{if(!px.redo.length)return;px.undo.push(px.cells.slice());px.cells=px.redo.pop();pxChanged();};$('pixelClear').onclick=()=>{pxSnapshot();px.cells.fill('000000');pxChanged();};
$('pixelTools').onclick=()=>{let box=el('div','stack'),intro=el('small','','Tail range tools select exact logical LEDs 0–149.');let row=el('div','row'),from=el('input'),to=el('input');from.type=to.type='number';from.min=to.min='0';from.max=to.max='149';from.value='0';to.value='149';from.setAttribute('aria-label','From Tail LED');to.setAttribute('aria-label','To Tail LED');row.append(from,to);function range(){let a=Number(from.value),b=Number(to.value);if(!Number.isInteger(a)||!Number.isInteger(b)||a<0||b>149||a>b)throw Error('Use a Tail range from 0 to 149');return[a,b];}box.append(intro,row,action('Fill range',()=>{try{let[a,b]=range();pxSnapshot();for(let i=a;i<=b;i++)px.cells[300+i]=$('pixelColor').value.slice(1);pxChanged();closeSheet();}catch(e){say(e.message,true);}}),action('Gradient',()=>{try{let[a,b]=range(),hex=$('pixelColor').value.slice(1);pxSnapshot();for(let i=a;i<=b;i++){let f=(i-a)/Math.max(1,b-a);px.cells[300+i]=[0,2,4].map(k=>Math.round(parseInt(hex.slice(k,k+2),16)*f).toString(16).padStart(2,'0')).join('');}pxChanged();closeSheet();}catch(e){say(e.message,true);}}));sheet('Drawing tools','Use the compact toolbar for strokes. These tools act on Tail detail.',box);};
async function sendPixels(live){if(px.inflight||!online)return;px.inflight=true;let generation=px.generation;$('pixelStatus').textContent='Sending';try{if(!px.session){let r=await api('/display/pixel/session',{});if(!Number.isInteger(r.session)||!r.session)throw Error('Invalid session ACK');px.session=r.session;px.rev=0;}let next=px.rev+1,session=px.session;let ack=await api('/display/pixel/frame',{session,revision:next,pixels:px.cells.join(''),live:!!live});if(ack.session!==session||ack.revision!==next||ack.applied!==true)throw Error('Frame ACK did not match');px.rev=next;px.dirty=px.generation!==generation;$('pixelStatus').textContent=px.dirty?'Draft':'Applied';$('pixelTransfer').textContent=px.dirty?'New edits need sending':'Applied revision '+next+' on wall';if(!px.dirty)say('Artwork applied on wall');}catch(e){px.session=0;px.rev=0;px.live=false;$('pixelLive').checked=false;$('pixelStatus').textContent='Unsent';$('pixelTransfer').textContent='Draft remains in browser';say(e.message,true);}finally{px.inflight=false;if(px.dirty&&px.live&&online)sendPixels(true);}}
$('pixelSend').onclick=()=>sendPixels(false);$('pixelLive').onchange=()=>{px.live=$('pixelLive').checked;if(px.live&&!online){px.live=false;$('pixelLive').checked=false;say('Wall offline; draft remains local',true);}};
$('pixelSave').onclick=()=>{let box=el('div','stack'),name=el('input');name.placeholder='Project name';name.maxLength=24;box.append(name,action('Save in browser',()=>{let n=name.value.trim();if(!/^[A-Za-z0-9_-]{1,24}$/.test(n)){say('Use 1–24 letters, digits, _ or -',true);return;}if(!saved[n]&&Object.keys(saved).length>=10){say('Ten project limit; export or replace one',true);return;}saved[n]=px.cells.slice();if(saveStore('ledwall-v5-pixels',saved)){say('Saved in this browser');closeSheet();}},true));sheet('Save artwork','The protected device filesystem is currently unavailable. Browser projects and export remain usable.',box);};
$('pixelProjects').onclick=()=>{let box=el('div','stack');Object.keys(saved).sort().forEach(name=>box.append(action('Open '+name,()=>{pxSnapshot();px.cells=saved[name].slice();pxChanged();closeSheet();})));if(!box.children.length)box.append(el('small','','No saved artwork yet.'));box.append(action('Export current artwork',()=>{let blob=new Blob([JSON.stringify({schemaVersion:1,width:30,height:10,tailLength:150,pixels:px.cells})],{type:'application/json'}),url=URL.createObjectURL(blob),a=el('a');a.href=url;a.download='led-wall-v5-art.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}),action('Import JSON',()=>{let f=el('input');f.type='file';f.accept='application/json';f.onchange=()=>{let file=f.files&&f.files[0];if(!file)return;file.text().then(t=>{let d=JSON.parse(t);if(d.schemaVersion!==1||d.width!==30||d.height!==10||d.tailLength!==150||!Array.isArray(d.pixels)||d.pixels.length!==450||!d.pixels.every(x=>/^[0-9a-f]{6}$/i.test(x)))throw Error('Invalid 450 pixel project');pxSnapshot();px.cells=d.pixels.map(x=>x.toLowerCase());pxChanged();closeSheet();}).catch(e=>say(e.message,true));};f.click();}));sheet('Projects','Browser storage can be cleared. Export important work.',box);};drawPixels();
function renderController(){let d=state&&state.ds4||{},mask=d.assignedMask||0,ready=!!d.ready,connected=!!d.players;let where=(mask&1?'P1':'')+(mask===3?' + ':'')+(mask&2?'P2':'');$('deviceControllers').textContent=d.supported?(connected?'Gamepad connected':'Bluetooth '+(ready?'ready':'starting')):'Phone input';$('controllerState').textContent=d.supported?(connected?'Connected':'Disconnected'):'Unavailable';$('controllerDetail').textContent=d.supported?(connected?'Assigned '+(where||'none')+(d.battery&&d.battery[0]?' · battery '+d.battery[0]+'%':'')+' · '+(d.inputEvents||0)+' inputs':ready?'Hold SHARE + PS until the light flashes':'Waiting for Wi-Fi before Bluetooth starts'):'Use phone P1/P2 controls';$('gameInputMode').textContent=connected?'DS4 '+(where||'unassigned')+' · phone on the other free slot':'Phone P1/P2 available';$('controllerPair').disabled=!d.supported||!ready;$('controllerP1').disabled=!connected;$('controllerP2').disabled=!connected;$('claimP1').disabled=!!(mask&1);$('claimP2').disabled=!!(mask&2);}
$('controllerPair').onclick=()=>command('/display/controller/pair',{},'Pairing enabled');$('controllerP1').onclick=()=>command('/display/controller/assign',{player:1},'Controller assigned P1');$('controllerP2').onclick=()=>command('/display/controller/assign',{player:2},'Controller assigned P2');
function renderState(){if(!state)return;let mode=state.currentMode||'idle',name=state.game&&state.game.active?state.game.name:state.activeAnimation||state.activeScene||mode;$('activeLabel').textContent=mode==='idle'?'Wall is ready':name.replaceAll('_',' ');$('activeDetail').textContent=mode==='idle'?'Choose a scene, game or creation':mode.toUpperCase()+' · device state';$('homeStop').disabled=mode==='idle';$('deviceVersion').textContent=state.fwVersion||'Unknown';$('deviceConnection').textContent=online?'Connected locally':'Offline';$('deviceBody').textContent='30×10 + '+(state.tail&&state.tail.enabled?state.tail.length+' Tail':'Tail unavailable');renderController();let b=state.brightness||{current:0,max:40};$('brightness').max=b.max;$('brightness').value=b.current;$('brightnessLabel').textContent='Current '+b.current+' / ceiling '+b.max+' (device scale)';$('rawState').textContent=JSON.stringify(state,null,2);renderGame();updateTimer();}
async function refresh(){try{let raw=await get('/state');state=normalizeState(raw);online=true;lastStateAt=performance.now();$('dot').classList.add('online');$('connectionText').textContent='Connected';renderState();}catch(e){online=false;$('dot').classList.remove('online');$('connectionText').textContent='Offline';$('deviceConnection').textContent='Offline';if(px.live){px.live=false;$('pixelLive').checked=false;$('pixelStatus').textContent='Unsent';}if(state)$('activeDetail').textContent='Connection lost · device state may have changed';}}
function connect(){try{ws=new WebSocket((location.protocol==='https:'?'wss://':'ws://')+location.hostname+':81');ws.onopen=()=>{wsDelay=1000;refresh();};ws.onmessage=()=>refresh();ws.onclose=()=>{setTimeout(connect,wsDelay);wsDelay=Math.min(wsDelay*2,8000);};ws.onerror=()=>ws.close();}catch(e){setTimeout(connect,wsDelay);}}
setInterval(refresh,4000);setInterval(updateTimer,250);connect();refresh();get('/display/pixel/projects').then(r=>{$('deviceStorage').textContent=r.available?'Device projects available':'Browser projects only';}).catch(()=>$('deviceStorage').textContent='Storage status unavailable');
})();
</script></body></html>
)lwui_v1";
#endif
}
#ifdef LED_WALL_ENABLE_DS4
const char* index_html() { return reinterpret_cast<const char*>(kIndexGzip); }
size_t index_html_len() { return sizeof(kIndexGzip); }
#else
const char* index_html() { return kIndexHtmlStore; }
size_t index_html_len() { return sizeof(kIndexHtmlStore) - 1; }
#endif
const char* index_content_type() { return "text/html; charset=utf-8"; }
bool is_index_route(const char* method, const char* uri) {
  return method != nullptr && uri != nullptr &&
         std::strcmp(method, "GET") == 0 && std::strcmp(uri, "/") == 0;
}
} }
