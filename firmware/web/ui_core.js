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
