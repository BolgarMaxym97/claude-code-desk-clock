#include "portal.h"
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include "device.h"
#include "net.h"
#include "wifisetup.h"

static ESP8266WebServer *srv;

static const char PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Claude Clock · Wi-Fi</title>
<style>body{margin:0;background:#141210;color:#FFE8B0;font:16px -apple-system,system-ui,sans-serif}
main{max-width:420px;margin:0 auto;padding:24px 16px}h1{font-size:22px;margin:0 0 4px;display:flex;justify-content:space-between;align-items:center}
p{color:#A8987C;margin:0 0 20px}.lg button{padding:6px 10px;border:1px solid #3A322A;background:#1E1A16;color:#A8987C;font:600 13px inherit;border-radius:8px}
.lg button.on{border-color:#D84000;color:#FFE8B0}
.n{display:flex;justify-content:space-between;width:100%;padding:14px;margin:0 0 8px;border:1px solid #3A322A;
border-radius:10px;background:#1E1A16;color:inherit;font:inherit;text-align:left}.n.s{border-color:#D84000}
label{display:block;margin:16px 0 6px;color:#A8987C}input{box-sizing:border-box;width:100%;padding:12px;border-radius:10px;
border:1px solid #3A322A;background:#1E1A16;color:#FFE8B0;font:inherit}
button.go{width:100%;margin-top:20px;padding:14px;border:0;border-radius:10px;background:#D84000;color:#fff;font:600 17px inherit}
#st{margin-top:18px;min-height:24px}.ok{color:#50C020}.bad{color:#E84030}a{color:#F0A000}</style></head>
<body><main><h1>Claude Clock<span class="lg"><button type="button" id="l-en" onclick="setL('en',1)">EN</button>
<button type="button" id="l-uk" onclick="setL('uk',1)">УКР</button></span></h1><p data-t="intro"></p>
<div id="list"></div><a href="#" onclick="scan(1);return false" data-t="refresh"></a>
<form onsubmit="save();return false"><label data-t="net"></label><input id="s" maxlength="32" required autocomplete="off">
<label data-t="pass"></label><input id="p" type="password" maxlength="63" autocomplete="off">
<label style="display:flex;gap:8px;align-items:center;color:#A8987C"><input type="checkbox" style="width:auto"
onchange="p.type=this.checked?'text':'password'"><span data-t="show"></span></label>
<button class="go" data-t="go"></button></form><div id="st"></div></main>
<script>
const T={en:{intro:'Choose the Wi-Fi network for the clock',refresh:'Refresh list',net:'Network',pass:'Password',show:'show password',
go:'Connect',searching:'Looking for networks…',none:'No networks found: type the name yourself',
checking:'Checking… your phone may briefly drop off Claude-Clock, that is normal.',creds:'Network name 1–32 characters, password empty or 8–63 characters',
busy:'Already checking, please wait',ok:'Done! The clock joined "{s}". You can leave Claude-Clock now.',
r1:'Wrong password',r2:'Network not found',r3:'Could not connect',r4:'Connected, but the router gave no IP address (DHCP)',again:'Check the password and try again.'},
uk:{intro:'Обери мережу Wi-Fi для годинника',refresh:'Оновити список',net:'Мережа',pass:'Пароль',show:'показати пароль',
go:'Підключити',searching:'Шукаю мережі…',none:'Мереж не знайдено — впиши назву вручну',
checking:'Перевіряю… телефон може на мить відключитись від Claude-Clock — це нормально.',creds:'Назва мережі 1–32 символи, пароль порожній або 8–63 символи',
busy:'Вже перевіряю, зачекай',ok:'Готово! Годинник підключився до «{s}». Можна відключитись від Claude-Clock.',
r1:'Невірний пароль',r2:'Мережу не знайдено',r3:'Не вдалось підключитись',r4:'Підключився, але роутер не видав IP-адресу (DHCP)',again:'Перевір пароль і спробуй ще раз.'}};
let L='en';const $=id=>document.getElementById(id),s=$('s'),p=$('p'),st=$('st');
const t=k=>T[L][k];
function setL(l,send){L=l;document.documentElement.lang=l;$('l-en').className=l=='en'?'on':'';$('l-uk').className=l=='uk'?'on':'';
document.querySelectorAll('[data-t]').forEach(e=>e.textContent=t(e.dataset.t));if(!$('list').children.length)$('list').textContent=t('searching');
if(send)fetch('/setup/lang',{method:'POST',body:new URLSearchParams({lang:l})}).catch(()=>{})}
function esc(x){const d=document.createElement('div');d.textContent=x;return d.innerHTML}
async function scan(r){try{const j=await (await fetch('/setup/scan'+(r?'?refresh=1':''))).json();
if(j.lang&&j.lang!=L&&!r)setL(j.lang,0);
if(j.busy){setTimeout(scan,1500);return}
$('list').innerHTML='';if(!j.list.length)$('list').textContent=t('none');
j.list.forEach(n=>{const b=document.createElement('button');b.type='button';b.className='n';
b.innerHTML='<span>'+esc(n.ssid)+'</span><span>'+(n.lock?'🔒 ':'')+n.bars+'/4</span>';
b.onclick=()=>{s.value=n.ssid;document.querySelectorAll('.n').forEach(x=>x.classList.remove('s'));b.classList.add('s');p.focus()};
$('list').appendChild(b)})}catch(e){setTimeout(scan,2000)}}
async function save(){st.className='';st.textContent=t('checking');
const b=new URLSearchParams({ssid:s.value,pass:p.value});
try{const r=await fetch('/setup/save',{method:'POST',body:b});if(!r.ok){st.className='bad';st.textContent=t(r.status==409?'busy':'creds');return}}catch(e){}
poll()}
async function poll(){try{const j=await (await fetch('/setup/state')).json();
if(j.state=='running'){setTimeout(poll,1000);return}
if(j.state=='ok'){st.className='ok';st.textContent=t('ok').replace('{s}',j.ssid);return}
if(j.state=='fail'){st.className='bad';st.textContent=t('r'+(j.reason||3))+'. '+t('again');return}
}catch(e){}setTimeout(poll,1500)}
setL('en',0);scan(0);
</script></body></html>)HTML";

// Setup endpoints answer only phones on the setup access point (its WPA password is shown on the
// clock screen, so access needs physical presence); from the LAN they do not exist.
static bool fromAp() {
    if (netIsApClient(srv->client().remoteIP())) return true;
    srv->send(404, "text/plain", "not found");
    return false;
}

static void handlePage() {
    if (!fromAp()) return;
    srv->sendHeader("Cache-Control", "no-store");
    srv->send_P(200, "text/html; charset=utf-8", PAGE);
}

static void handleScan() {
    if (!fromAp()) return;
    if (srv->hasArg("refresh")) netScanRefresh();
    if (netScanBusy()) { srv->send(200, "application/json", String("{\"busy\":true,\"lang\":\"") + deviceLang() + "\"}"); return; }
    const ScanItem *items;
    int n = netScan(items);
    JsonDocument doc;
    doc["lang"] = deviceLang();
    JsonArray list = doc["list"].to<JsonArray>();
    for (int i = 0; i < n; i++) {
        JsonObject o = list.add<JsonObject>();
        o["ssid"] = items[i].ssid;
        int r = items[i].rssi;
        o["bars"] = r > -55 ? 4 : r > -65 ? 3 : r > -75 ? 2 : 1;
        o["lock"] = items[i].lock;
    }
    String out;
    serializeJson(doc, out);
    srv->send(200, "application/json", out);
}

static void handleSave() {
    if (!fromAp()) return;
    String ssid = srv->arg("ssid"), pass = srv->arg("pass");
    if (!validCreds(ssid.c_str(), pass.c_str())) {
        srv->send(400, "application/json", "{\"error\":\"creds\"}");  // the page shows the text in its language
        return;
    }
    if (netInfo().test == TEST_RUNNING) { srv->send(409, "application/json", "{\"error\":\"busy\"}"); return; }
    srv->send(200, "application/json", "{\"ok\":true}");
    netTryCreds(ssid, pass);
}

static void handleState() {
    if (!fromAp()) return;
    NetInfo n = netInfo();
    JsonDocument doc;
    const char *st = n.test == TEST_RUNNING ? "running" : n.test == TEST_FAIL ? "fail"
                   : (n.test == TEST_OK || n.test == TEST_OK_UNSAVED) ? "ok" : "idle";
    doc["state"] = st;
    doc["ssid"] = n.testSsid;
    doc["saved"] = n.test == TEST_OK;
    if (n.test == TEST_FAIL) doc["reason"] = n.testReason;  // 1 password, 2 not found, 3 other, 4 no IP (DHCP)
    String out;
    serializeJson(doc, out);
    srv->send(200, "application/json", out);
}

bool portalRedirect() {
    // Captive portal: any unknown URL / host from a phone on the AP -> the setup page.
    if (!netIsApClient(srv->client().remoteIP())) return false;
    srv->sendHeader("Location", "http://192.168.4.1/setup");
    srv->send(302, "text/plain", "");
    return true;
}

void portalBegin(ESP8266WebServer &server) {
    srv = &server;
    server.on("/setup", HTTP_GET, handlePage);
    server.on("/setup/scan", HTTP_GET, handleScan);
    server.on("/setup/save", HTTP_POST, handleSave);
    server.on("/setup/state", HTTP_GET, handleState);
    server.on("/setup/lang", HTTP_POST, []() {  // language picked on the setup page: clock screen follows
        if (!fromAp()) return;
        bool ok = deviceSetLang(srv->arg("lang"));
        srv->send(ok ? 200 : 400, "application/json", ok ? "{\"ok\":true}" : "{\"error\":\"lang\"}");
    });
    server.onNotFound([]() {
        if (!portalRedirect()) srv->send(404, "text/plain", "not found");
    });
}
