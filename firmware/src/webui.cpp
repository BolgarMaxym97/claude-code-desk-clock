// Settings page on the clock (http://claude-clock.local/): language, sleep hours, brightness, clock face, weather,
// alerts and the idle animation. A browser gets the same token as a computer by typing the code shown on the clock
// screen (POST /api/pair), keeps it in localStorage and calls the token-protected /api endpoints like claude-clock.
#include "webui.h"
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include "config.h"
#include "device.h"
#include "fetch.h"

static ESP8266WebServer *srv;

static const char PAGE[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Claude Clock</title>
<style>body{margin:0;background:#141210;color:#FFE8B0;font:16px -apple-system,system-ui,sans-serif}
main{max-width:480px;margin:0 auto;padding:20px 16px 40px}h1{font-size:22px;margin:0 0 16px;display:flex;justify-content:space-between;align-items:center}
h2{font-size:15px;color:#A8987C;margin:24px 0 8px;text-transform:uppercase;letter-spacing:.06em;display:flex;justify-content:space-between;gap:8px;align-items:baseline}
h2 .now{color:#F0A000;font-size:13px;text-transform:none;letter-spacing:0;font-weight:600}
.c{background:#1E1A16;border:1px solid #3A322A;border-radius:12px;padding:14px}
label{display:block;margin:10px 0 6px;color:#A8987C;font-size:14px}
input,select{box-sizing:border-box;width:100%;padding:10px;border-radius:10px;border:1px solid #3A322A;background:#141210;color:#FFE8B0;font:inherit}
input[type=range]{padding:0}input[type=checkbox]{width:auto}
.row{display:flex;gap:8px;align-items:center}.row>*{flex:1}
button{padding:10px 14px;border:1px solid #3A322A;border-radius:10px;background:#2A241E;color:#FFE8B0;font:600 15px inherit;cursor:pointer}
button.go{background:#D84000;border-color:#D84000;color:#fff}button.w{width:100%;margin-top:10px}
.seg{display:flex;gap:6px;flex-wrap:wrap}.seg label,.chip{margin:0;padding:8px 12px;border:1px solid #3A322A;border-radius:10px;color:#A8987C;cursor:pointer;font-size:14px}
.seg input,.chip input{display:none}.seg label:has(input:checked),.chip:has(input:checked){border-color:#D84000;color:#FFE8B0}
.m{color:#A8987C;font-size:13px;margin-top:6px}#msg{position:sticky;top:0;min-height:22px;padding:6px 0;background:#141210;z-index:2}
.lst{max-height:360px;overflow:auto;border:1px solid #3A322A;border-radius:10px;margin-top:10px}
.it{display:flex;align-items:center;gap:10px;padding:10px 12px;border-top:1px solid #2A241E;cursor:pointer}.it:first-child{border-top:0}
.it em{color:#A8987C;font-size:12px;font-style:normal;margin-left:auto}.it .pl{color:#D84000;font-weight:700}.it.on{background:#24180A}
.st{color:#584A36;font-size:18px;padding:0 2px}.st.on{color:#F0A000}
.ok{color:#50C020}.bad{color:#E84030}.hide{display:none}a{color:#F0A000}</style></head>
<body><main><h1>Claude Clock<span class="seg"><label><input type="radio" name="ul" value="en" onchange="ui(this.value)"><span>EN</span></label>
<label><input type="radio" name="ul" value="uk" onchange="ui(this.value)"><span>УКР</span></label></span></h1>
<div id="msg"></div>
<section id="pair" class="hide"><div class="c"><p data-t="pairIntro"></p><button class="go" onclick="pairStart()" data-t="showCode"></button>
<label data-t="code"></label><div class="row"><input id="code" maxlength="6" autocomplete="off" style="text-transform:uppercase">
<button onclick="pair()" data-t="connect"></button></div></div></section>
<section id="set" class="hide">
<h2><span data-t="anim"></span><span class="now" id="pinNow"></span></h2><div class="c">
<div class="seg"><label><input type="radio" name="mode" value="0" onchange="saveAnim()"><span data-t="mRandom"></span></label>
<label><input type="radio" name="mode" value="1" onchange="saveAnim()"><span id="mFav"></span></label></div>
<label data-t="groups"></label><div class="seg" id="groups"></div>
<label data-t="freq"></label><div class="seg"><label><input type="radio" name="freq" value="0" onchange="saveAnim()"><span data-t="calm"></span></label>
<label><input type="radio" name="freq" value="1" onchange="saveAnim()"><span data-t="normal"></span></label>
<label><input type="radio" name="freq" value="2" onchange="saveAnim()"><span data-t="lively"></span></label></div>
<label data-t="pick"></label><input id="q" oninput="list()" autocomplete="off">
<div class="lst" id="lst"></div><div class="m" data-t="listHint"></div>
<label data-t="keep"></label><div class="row"><select id="dur"><option value="0" data-t="always"></option><option value="m" data-t="minutes"></option>
<option value="h" data-t="hours"></option></select><input id="n" type="number" min="1" max="720" value="30"></div>
<button class="go w" onclick="pin()" id="pinBtn"></button>
<div class="row" style="margin-top:8px"><button onclick="playAll()" data-t="all"></button><button onclick="unpin()" data-t="unpin"></button></div>
<button class="w" onclick="reset()" data-t="reset"></button></div>
<h2 data-t="screen"></h2><div class="c"><label data-t="lang"></label><div class="seg">
<label><input type="radio" name="lang" value="en"><span>English</span></label><label><input type="radio" name="lang" value="uk"><span>Українська</span></label></div>
<label data-t="face"></label><div class="seg" id="faces"></div>
<label><span data-t="bDay"></span> <b id="bdv"></b>%</label><input id="bd" type="range" min="5" max="100" oninput="bdv.textContent=this.value">
<label><span data-t="bNight"></span> <b id="bnv"></b>%</label><input id="bn" type="range" min="5" max="100" oninput="bnv.textContent=this.value">
<label data-t="sleep"></label><div class="row"><select id="nf"></select><span style="flex:0">—</span><select id="nt"></select></div>
<label class="row" style="color:#FFE8B0"><input type="checkbox" id="never" style="flex:0"><span data-t="never"></span></label>
<button class="go w" onclick="saveScreen()" data-t="save"></button></div>
<h2 data-t="weather"></h2><div class="c"><label data-t="key"></label><input id="owm" maxlength="32" autocomplete="off">
<div class="m" id="owmNow"></div><label data-t="city"></label><div class="row"><input id="city" autocomplete="off"><button onclick="geo()" data-t="find"></button></div>
<div class="m" id="locNow"></div><button class="go w" onclick="saveWx()" data-t="save"></button></div>
<h2 data-t="alerts"></h2><div class="c"><select id="region"></select>
<label data-t="atok"></label><input id="atok" maxlength="64" autocomplete="off"><div class="m" id="atokNow"></div>
<div class="m" data-t="alertsHint"></div><button class="go w" onclick="saveAl()" data-t="save"></button></div>
<p class="m" id="foot"></p></section></main>
<script>
const R=['м. Київ','Київська область','Вінницька область','Волинська область','Дніпропетровська область','Донецька область','Житомирська область',
'Закарпатська область','Запорізька область','Івано-Франківська область','Кіровоградська область','Луганська область','Львівська область',
'Миколаївська область','Одеська область','Полтавська область','Рівненська область','Сумська область','Тернопільська область',
'Харківська область','Херсонська область','Хмельницька область','Черкаська область','Чернівецька область','Чернігівська область','Севастополь'];
const KINDS=[['daily',1],['weekend',2],['rare',4],['holiday',8],['event',16]];
const T={en:{pairIntro:'Connect this browser to the clock: press the button, the clock shows a code, type it here.',showCode:'Show code on the clock',
code:'Code from the clock screen',connect:'Connect',anim:'Animation',mRandom:'Random',fav:'Favourites only',groups:'Groups in the random pick',
daily:'Every day',weekend:'Weekend',rare:'Rare',holiday:'Holidays',event:'Events',freq:'How often the mascot does something',calm:'Calm',normal:'Normal',lively:'Lively',
pick:'Search',listHint:'Tap a row to play it now, the star adds it to favourites.',keep:'Keep the selected one playing',always:'always',minutes:'minutes',hours:'hours',
keepBtn:'Keep playing: ',all:'▶ All in turn',unpin:'Stop keeping',reset:'Reset animation settings',resetDone:'Animation settings are back to default',
screen:'Screen',lang:'Language',face:'Clock face',faces:['Classic','Minimal','Pixel','Segment','Flip','Terminal','Binary'],
bDay:'Brightness by day',bNight:'Brightness at night',sleep:'Sleep hours (night screen)',never:'Never sleep',save:'Save',weather:'Weather',
key:'OpenWeather key (free, openweathermap.org)',city:'City',find:'Find',alerts:'Air-raid alerts (Ukraine)',
atok:'alerts.in.ua token: drones / missiles, threat, partial alert',alertsHint:'Get a token at alerts.in.ua/api-request. Levels show while Claude Code runs on a computer; without it the clock shows only whether there is an alert. The data is informational: follow official sources.',
off:'off',saved:'Saved',err:'Error: ',keySet:'A key is set (hidden). Type a new one to replace it, or "off" to remove.',noKey:'No key: weather is off.',
tokSet:'A token is set (hidden). Type a new one to replace it, or "off" to remove.',noTok:'No token.',
loc:'Location: ',pinned:'kept: ',until:' until ',codeShown:'The code is on the clock screen.',wrong:'Wrong code',
locked:'Too many wrong codes: restart the clock',notFound:'City not found',playing:'▶ '},
uk:{pairIntro:'Підключи цей браузер до годинника: натисни кнопку, годинник покаже код, введи його тут.',showCode:'Показати код на годиннику',
code:'Код з екрана годинника',connect:'Підключити',anim:'Анімація',mRandom:'Випадкові',fav:'Тільки обрані',groups:'Групи у випадковому виборі',
daily:'Щодня',weekend:'Вихідні',rare:'Рідкісні',holiday:'Свята',event:'Події',freq:'Як часто маскот щось робить',calm:'Спокійно',normal:'Звичайно',lively:'Жваво',
pick:'Пошук',listHint:'Тап по рядку грає анімацію зараз, зірочка додає в обрані.',keep:'Тримати вибрану',always:'завжди',minutes:'хвилин',hours:'годин',
keepBtn:'Тримати: ',all:'▶ Усі по черзі',unpin:'Не тримати',reset:'Скинути налаштування анімації',resetDone:'Налаштування анімації скинуто',
screen:'Екран',lang:'Мова',face:'Циферблат',faces:['Класичний','Мінімальний','Піксельний','Сегментний','Перекидний','Термінал','Двійковий'],
bDay:'Яскравість вдень',bNight:'Яскравість вночі',sleep:'Нічні години (нічний екран)',never:'Ніколи не засинати',save:'Зберегти',weather:'Погода',
key:'Ключ OpenWeather (безкоштовний, openweathermap.org)',city:'Місто',find:'Знайти',alerts:'Повітряна тривога',
atok:'Токен alerts.in.ua: дрони / ракети, загроза, тривога частково',alertsHint:'Токен: alerts.in.ua/api-request. Рівні видно, поки на комп’ютері працює Claude Code; без нього годинник показує лише, чи є тривога. Дані довідкові: орієнтуйся на офіційні джерела.',
off:'вимкнено',saved:'Збережено',err:'Помилка: ',keySet:'Ключ задано (прихований). Введи новий, щоб замінити, або "off", щоб прибрати.',noKey:'Ключа немає: погода вимкнена.',
tokSet:'Токен задано (прихований). Введи новий, щоб замінити, або "off", щоб прибрати.',noTok:'Токена немає.',
loc:'Місце: ',pinned:'тримається: ',until:' до ',codeShown:'Код на екрані годинника.',wrong:'Невірний код',
locked:'Забагато невірних кодів: перезапусти годинник',notFound:'Місто не знайдено',playing:'▶ '}};
const st=(()=>{try{return localStorage}catch(e){const m={};return{getItem:k=>m[k]??null,setItem:(k,v)=>m[k]=v,removeItem:k=>delete m[k]}}})();
let L=st.getItem('ul')||((navigator.language||'').startsWith('uk')?'uk':'en'),tok=st.getItem('tok')||'',S={},scenes=[],A={fav:0,groups:31,freq:1,bits:[]},sel='';
const $=id=>document.getElementById(id),t=k=>T[L][k]||k;
function ui(l){L=l;st.setItem('ul',l);document.querySelectorAll('[data-t]').forEach(e=>e.textContent=t(e.dataset.t));
document.querySelector('[name=ul][value='+l+']').checked=true;if(S.fw)fill()}
function msg(s,ok){$('msg').textContent=s;$('msg').className=ok?'ok':'bad';if(ok)setTimeout(()=>{if($('msg').textContent==s)$('msg').textContent=''},3000)}
async function api(p,body){const r=await fetch(p,{method:body===undefined?'GET':'POST',headers:{'X-Clock-Token':tok},body:body===undefined?undefined:JSON.stringify(body)});
if(r.status==401){tok='';st.removeItem('tok');show();throw new Error('token')}
const j=await r.json().catch(()=>({}));if(!r.ok)throw new Error(j.error||r.status);return j}
async function pairStart(){try{const r=await fetch('/api/pair/start',{method:'POST'});msg(r.ok?t('codeShown'):t('locked'),r.ok);$('code').focus()}catch(e){msg(t('err')+e.message)}}
async function pair(){const r=await fetch('/api/pair',{method:'POST',body:JSON.stringify({code:$('code').value.trim().toUpperCase()})});
if(r.ok){tok=(await r.json()).token;st.setItem('tok',tok);show()}else msg(r.status==423?t('locked'):t('wrong'))}
function show(){$('pair').classList.toggle('hide',!!tok);$('set').classList.toggle('hide',!tok);if(tok)load()}
function opts(sel,list){sel.innerHTML='';list.forEach(([v,txt])=>{const o=document.createElement('option');o.value=v;o.textContent=txt;sel.appendChild(o)})}
function parseAnim(s){const a={fav:0,groups:31,freq:1,bits:[]};if(!s)return a;const p=s.split(',');a.fav=+p[0];a.groups=+p[1];a.freq=+p[2];
for(let i=0;i<(p[3]||'').length;i+=2){const b=parseInt(p[3].substr(i,2),16);for(let k=0;k<8;k++)if(b>>k&1)a.bits.push(i/2*8+k)}return a}
function encAnim(a){if(!a.fav&&a.groups==31&&a.freq==1&&!a.bits.length)return'';const by=[];a.bits.forEach(i=>{by[i>>3]=(by[i>>3]||0)|1<<(i&7)});
let h='';for(let i=0;i<by.length;i++)h+=((by[i]||0)+256).toString(16).slice(1);return(a.fav?1:0)+','+a.groups+','+a.freq+','+h}
async function load(){try{S=await api('/api/settings');scenes=await api('/api/scenes');A=parseAnim(S.anim);fill()}catch(e){if(e.message!='token')msg(t('err')+e.message)}}
const nm=n=>{const s=scenes.find(x=>x.n==n);return s?s.t:n};
function list(){const q=$('q').value.trim().toLowerCase(),order=KINDS.map(k=>k[0]);
const v=scenes.filter(s=>!q||s.t.toLowerCase().includes(q)||s.n.includes(q)).sort((a,b)=>order.indexOf(a.k)-order.indexOf(b.k));
$('lst').innerHTML='';v.forEach(s=>{const d=document.createElement('div');d.className='it'+(s.n==sel?' on':'');
const f=A.bits.includes(s.i);d.innerHTML='<span class="st'+(f?' on':'')+'">'+(f?'★':'☆')+'</span><span></span><em></em><span class="pl">▶</span>';
d.children[1].textContent=s.t;d.children[2].textContent=t(s.k);
d.children[0].onclick=e=>{e.stopPropagation();A.bits=f?A.bits.filter(x=>x!=s.i):A.bits.concat(s.i);saveAnim()};
d.onclick=()=>{sel=s.n;$('pinBtn').textContent=t('keepBtn')+s.t;list();api('/api/preview-scene?name='+encodeURIComponent(s.n),{}).then(()=>msg(t('playing')+s.t,1)).catch(e=>msg(t('err')+e.message))};
$('lst').appendChild(d)})}
function fill(){const g=$('groups');g.innerHTML='';KINDS.forEach(([k,b])=>{const c=scenes.filter(s=>s.k==k).length,l=document.createElement('label');l.className='chip';
l.innerHTML='<input type="checkbox"><span></span>';l.children[1].textContent=t(k)+' '+c;l.children[0].checked=!!(A.groups&b);
l.children[0].onchange=e=>{A.groups=e.target.checked?A.groups|b:A.groups&~b;saveAnim()};g.appendChild(l)});
document.querySelector('[name=mode][value="'+(A.fav?1:0)+'"]').checked=true;$('mFav').textContent=t('fav')+' ('+A.bits.length+')';
document.querySelector('[name=freq][value="'+A.freq+'"]').checked=true;
const p=(S.pin||'').split(',');if(p[0]&&!sel)sel=p[0];$('pinBtn').textContent=t('keepBtn')+(sel?nm(sel):'—');
$('pinNow').textContent=p[0]?t('pinned')+nm(p[0])+(+p[1]?t('until')+new Date(p[1]*1000).toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'}):''):'';list();
const fc=$('faces');fc.innerHTML='';t('faces').forEach((f,i)=>{const l=document.createElement('label');l.innerHTML='<input type="radio" name="face" value="'+i+'"><span></span>';
l.children[1].textContent=f;fc.appendChild(l)});document.querySelector('[name=face][value="'+S.face+'"]').checked=true;
const h=[...Array(24).keys()].map(i=>[i,(i<10?'0':'')+i+':00']);opts($('nf'),h);opts($('nt'),h);
const nn=(S.night||'23-7').split('-');$('never').checked=S.night=='off';$('nf').value=S.night=='off'?23:nn[0];$('nt').value=S.night=='off'?7:nn[1];
const b=(S.bright||'80/20').split('/');$('bd').value=b[0];$('bn').value=b[1];bdv.textContent=b[0];bnv.textContent=b[1];
document.querySelector('[name=lang][value='+S.lang+']').checked=true;
$('owmNow').textContent=S.owm?t('keySet'):t('noKey');$('locNow').textContent=S.loc?t('loc')+S.loc:'';$('atokNow').textContent=S.atok?t('tokSet'):t('noTok');
opts($('region'),[['',t('off')]].concat(R.map(r=>[r,r])));$('region').value=S.region||'';
$('foot').innerHTML='v'+S.fw+' · <a href="/status">status</a>'}
async function save(b,ok){try{await api('/api/settings',b);msg(ok||t('saved'),1);await load()}catch(e){if(e.message!='token')msg(t('err')+e.message)}}
function saveAnim(){A.fav=+document.querySelector('[name=mode]:checked').value;A.freq=+document.querySelector('[name=freq]:checked').value;save({anim:encAnim(A)})}
function pin(){if(!sel)return;const d=$('dur').value,n=Math.max(1,+$('n').value||1);
save({pin:sel+','+(d=='0'?0:Math.floor(Date.now()/1000)+n*(d=='m'?60:3600))})}
function unpin(){save({pin:''})}
function playAll(){api('/api/preview-scene?all=1',{}).then(()=>msg(t('all'),1)).catch(e=>msg(t('err')+e.message))}
function reset(){sel='';save({anim:'',pin:''},t('resetDone'))}
function saveScreen(){const lang=document.querySelector('[name=lang]:checked'),face=document.querySelector('[name=face]:checked');
save({lang:lang?lang.value:'en',face:face?face.value:'0',bright:$('bd').value+'/'+$('bn').value,night:$('never').checked?'off':$('nf').value+'-'+$('nt').value})}
async function geo(){try{const r=await api('/api/geocode?q='+encodeURIComponent($('city').value),{});$('city').value=r.name;$('city').dataset.loc=r.loc;
$('locNow').textContent=t('loc')+r.name+' ('+r.loc+')'}catch(e){msg(e.message=='404'||e.message=='city'?t('notFound'):t('err')+e.message)}}
function saveWx(){const b={},k=$('owm').value.trim();if(k)b.owm=k=='off'?'':k;if($('city').dataset.loc)b.loc=$('city').dataset.loc;save(b).then(()=>{$('owm').value=''})}
function saveAl(){const b={region:$('region').value},k=$('atok').value.trim();if(k)b.atok=k=='off'?'':k;save(b).then(()=>{$('atok').value=''})}
ui(L);show();
</script></body></html>)HTML";

static bool authorized() {
    if (srv->header("X-Clock-Token") == deviceToken()) return true;
    srv->send(401, "application/json", "{\"error\":\"token\"}");
    return false;
}

static void handlePage() {
    srv->sendHeader("Cache-Control", "no-store");
    srv->send_P(200, "text/html; charset=utf-8", PAGE);
}

// Current settings for the page (token only: the location is private; the weather key never leaves the clock).
static void handleGetSettings() {
    if (!authorized()) return;
    JsonDocument doc;
    doc["fw"] = FW_VERSION;
    doc["lang"] = deviceLang();
    doc["owm"] = *deviceOwm() != 0;
    doc["loc"] = deviceLoc();
    doc["region"] = deviceRegion();
    int a, b;
    deviceNight(a, b);
    char nb[8];
    snprintf(nb, sizeof nb, "%d-%d", a, b);
    doc["night"] = a == b ? "off" : nb;
    deviceBright(a, b);
    snprintf(nb, sizeof nb, "%d/%d", a, b);
    doc["bright"] = nb;
    doc["face"] = deviceFace();
    doc["pin"] = devicePin();
    doc["anim"] = deviceAnim();
    // the alerts.in.ua token: only "set or not" for the page; the computer that polls the service asks ?secret=1
    if (srv->hasArg("secret")) doc["atok"] = deviceAlertsToken();
    else doc["atok"] = *deviceAlertsToken() != 0;
    String out;
    serializeJson(doc, out);
    srv->send(200, "application/json", out);
}

// City name -> "lat,lon" with the clock's own OpenWeather key (plain HTTP; the page never sees the key).
static void handleGeocode() {
    if (!authorized()) return;
    String q = srv->arg("q");
    q.trim();
    if (!*deviceOwm()) { srv->send(409, "application/json", "{\"error\":\"key\"}"); return; }
    if (q.isEmpty() || q.length() > 60) { srv->send(400, "application/json", "{\"error\":\"city\"}"); return; }
    String enc;
    for (size_t i = 0; i < q.length(); i++) {  // percent-encode everything but letters and digits
        uint8_t c = uint8_t(q[i]);
        if (isalnum(c)) enc += char(c);
        else { char h[4]; snprintf(h, sizeof h, "%%%02X", c); enc += h; }
    }
    WiFiClient cl;
    HTTPClient http;
    http.setTimeout(3000);
    http.useHTTP10(true);
    String url = String("http://api.openweathermap.org/geo/1.0/direct?limit=1&q=") + enc + "&appid=" + deviceOwm();
    int code = http.begin(cl, url) ? http.GET() : -1;
    JsonDocument filter, doc;
    filter[0]["name"] = true;
    filter[0]["lat"] = true;
    filter[0]["lon"] = true;
    filter[0]["local_names"][deviceLang()] = true;
    if (code != 200 || deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter)) || !doc[0].is<JsonObject>()) {
        http.end();
        srv->send(404, "application/json", "{\"error\":\"city\"}");
        return;
    }
    http.end();
    JsonObject p = doc[0];
    char loc[32];
    snprintf(loc, sizeof loc, "%.2f,%.2f", p["lat"].as<double>(), p["lon"].as<double>());
    JsonDocument out;
    const char *local = p["local_names"][deviceLang()] | (const char *)nullptr;
    out["name"] = local ? local : (p["name"] | "");
    out["loc"] = loc;
    String body;
    serializeJson(out, body);
    srv->send(200, "application/json", body);
}

void webuiBegin(ESP8266WebServer &server) {
    srv = &server;
    server.on("/api/settings", HTTP_GET, handleGetSettings);
    server.on("/api/geocode", HTTP_POST, handleGeocode);
}

void webuiPage() { handlePage(); }
