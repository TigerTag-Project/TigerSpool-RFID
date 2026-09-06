#pragma once
#include <Arduino.h>
#include <pgmspace.h>

// The Wi-Fi setup page, served to a phone that has joined the device's own
// access point.
//
// Everything is in this one string on purpose. The phone on that network has NO
// internet: it is joined to an access point that routes nowhere, so a web font,
// an icon set or a CDN would silently fail and leave a broken layout at the
// exact moment the user has no way to fix it. System font, inline SVG, no
// external request of any kind, no image file to serve.
//
// It commits to a dark ground rather than following the phone's theme, and
// offers a toggle. The device's own screen is near-black with a single amber
// accent; a portal that turns white on half the phones that open it is a second
// product. The accent is the exact value the LVGL screens use.
//
// Placeholders substituted at serve time:
//   %SSID%  the setup access point's own name, per-device
//   %FW%    firmware version, shown on every screen for bug reports
//   %LANG%  the language chosen on the device, so the page opens in it
static const char PORTAL_HTML[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>%SSID%</title><style>
:root{--bg:#0B0D10;--surf:#151921;--surf2:#1C222B;--line:#242B36;--fg:#F3F6F9;
--dim:#7F8A99;--acc:#F2C744;--ok:#48B96A;--err:#FF6B5E;--btnfg:#0B0D10}
.light{--bg:#F4F5F7;--surf:#FFF;--surf2:#EDEFF3;--line:#E0E3E9;--fg:#12151A;
--dim:#69717F;--acc:#B8860B;--ok:#1F7A43;--err:#C0392B;--btnfg:#FFF}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.5 -apple-system,
BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;-webkit-font-smoothing:antialiased;
letter-spacing:-.005em;padding:0 0 32px}
/* Two corners rather than a cluster. The language was already chosen on the
   device's own screen in step one, so this is a correction, not a step - it
   sits out of the way on the left while the theme, the only thing anyone
   actually reaches for here, keeps the thumb-side corner. */
.bar{display:flex;justify-content:space-between;align-items:center;gap:8px;padding:14px 20px 0}
.pad{padding:22px 20px 0;max-width:520px;margin:0 auto}
h1{font-size:32px;line-height:1.12;font-weight:600;letter-spacing:-.032em;margin:0 0 22px}
p.l{margin:0 0 24px;color:var(--dim)}
.tg{position:relative;width:64px;height:40px;border-radius:20px;padding:0;background:var(--surf);
border:1px solid var(--line);cursor:pointer;flex:0 0 64px}
.tg .k{position:absolute;top:4px;left:4px;width:30px;height:30px;border-radius:50%;
background:var(--acc);transition:transform .22s cubic-bezier(.32,.72,0,1);z-index:0}
.tg .ic{z-index:1}
.light .tg .k{transform:translateX(24px)}
.tg .ic{position:absolute;top:0;height:100%;width:30px;display:grid;place-items:center;
pointer-events:none;transition:color .22s}
.tg .ic svg{width:15px;height:15px;display:block;fill:none;stroke:currentColor;stroke-width:2;
stroke-linecap:round;stroke-linejoin:round}
.tg .s{left:4px;color:var(--btnfg)}.tg .m{right:4px;color:var(--dim)}
.light .tg .s{color:var(--dim)}.light .tg .m{color:#FFF}
.lw{position:relative}
.lb{font:inherit;font-size:13px;font-weight:500;padding:0 12px;min-height:40px;border-radius:10px;
background:var(--surf);color:var(--dim);border:1px solid var(--line);cursor:pointer;
display:flex;align-items:center;gap:8px}
.lb .c{display:flex;transition:transform .16s}
.lb[aria-expanded=true] .c{transform:rotate(180deg)}
.lm{position:absolute;top:46px;left:0;z-index:20;min-width:186px;background:var(--surf);
border:1px solid var(--line);border-radius:14px;padding:6px;box-shadow:0 8px 28px rgba(0,0,0,.45);
display:flex;flex-direction:column;gap:2px}
.lm[hidden]{display:none}
.lm button{font:inherit;font-size:14px;text-align:left;padding:0 12px;min-height:44px;border:0;
border-radius:9px;background:none;color:var(--fg);cursor:pointer;display:flex;align-items:center}
.lm button[aria-selected=true]{color:var(--acc);font-weight:500}
.nets{display:flex;flex-direction:column;gap:8px;margin-bottom:14px}
.n{display:flex;align-items:center;gap:14px;min-height:60px;padding:0 16px;background:var(--surf);
border:1px solid var(--line);border-radius:14px;cursor:pointer;width:100%;font:inherit;color:inherit;
text-align:left}
.ex input{padding-right:52px}
.n .nm{flex:1;font-size:16px;font-weight:500;overflow:hidden;white-space:nowrap;text-overflow:ellipsis}
.n .k{display:flex;color:var(--dim);opacity:.5}
/* An opened row is one card, not a row with a panel stuck under it: the border
   wraps both, and the button loses its own so the seam disappears. */
.it{border:1px solid var(--line);border-radius:14px;background:var(--surf);overflow:hidden;
transition:border-color .24s ease}
.it .n{border:0;background:none;border-radius:0}
.it.op{border-color:var(--acc)}
/* The panel grows from nothing rather than appearing.
   grid-template-rows 0fr -> 1fr is the one way to animate to a height nobody
   measured; max-height needs a guess, and a guess either clips the content or
   makes the easing lie about how far it has to travel.
   The inner div carries overflow:hidden and min-height:0 - without min-height a
   grid item refuses to shrink below its content and nothing moves at all. */
.exw{display:grid;grid-template-rows:0fr;transition:grid-template-rows .28s cubic-bezier(.32,.72,0,1)}
.exw.o{grid-template-rows:1fr}
.exw>div{overflow:hidden;min-height:0}
/* The content follows slightly behind the box, so it reads as being revealed by
   the opening rather than as a second thing that also appeared. */
.ex{padding:2px 16px 16px;opacity:0;transform:translateY(-5px);
transition:opacity .2s ease .07s,transform .2s cubic-bezier(.32,.72,0,1) .07s}
.exw.o .ex{opacity:1;transform:none}
.ex label{margin-top:4px}
.ex .b{margin-top:14px}
@media(prefers-reduced-motion:reduce){.exw,.ex,.it{transition:none}}
.sig{display:flex;color:var(--fg)}
.gh{width:100%;min-height:52px;border:1px solid var(--line);border-radius:14px;background:none;
color:var(--dim);font:inherit;font-size:15px;cursor:pointer}
label{display:block;font-size:13px;font-weight:500;color:var(--dim);margin:0 0 8px}
.fld{position:relative}
input{width:100%;min-height:56px;padding:0 52px 0 16px;font:inherit;font-size:17px;
background:var(--surf);color:var(--fg);border:1px solid var(--line);border-radius:14px}
input:focus{outline:none;border-color:var(--acc);box-shadow:0 0 0 3px rgba(242,199,68,.14)}
.eye{position:absolute;top:0;right:0;width:48px;height:56px;display:grid;place-items:center;
background:none;border:0;color:var(--dim);cursor:pointer}
.eye[aria-pressed=true]{color:var(--acc)}
.b{width:100%;min-height:56px;border:0;border-radius:14px;font:inherit;font-size:17px;
font-weight:600;cursor:pointer;background:var(--acc);color:var(--btnfg);margin-top:22px}
.b.s{background:none;border:1px solid var(--line);color:var(--fg);font-weight:500;margin-top:10px}
.al{display:flex;gap:12px;padding:15px 16px;border-radius:14px;font-size:14px;margin-bottom:22px;
background:rgba(255,107,94,.09);border:1px solid rgba(255,107,94,.26);color:var(--err)}
.al b{display:block;margin-bottom:3px}
.ct{text-align:center;padding:34px 0 12px}
.sp{width:56px;height:56px;margin:0 auto 26px;border-radius:50%;border:3px solid var(--line);
border-top-color:var(--acc);animation:s .85s linear infinite}
@keyframes s{to{transform:rotate(360deg)}}
@media(prefers-reduced-motion:reduce){.sp{animation:none}}
.tk{width:64px;height:64px;margin:0 auto 22px;border-radius:50%;background:rgba(72,185,106,.13);
border:1px solid rgba(72,185,106,.4);display:grid;place-items:center;color:var(--ok)}
dl{margin:30px 0 0;display:grid;grid-template-columns:auto 1fr;gap:10px 20px;font-size:13.5px;
text-align:left;border-top:1px solid var(--line);padding-top:22px}
dt{color:var(--dim);white-space:nowrap}
dd{margin:0;font-family:ui-monospace,monospace;font-size:13px;text-align:right}
.ft{margin-top:34px;font-size:12px;color:var(--dim);opacity:.6;text-align:center}
</style></head><body>
<div class="bar">
 <div class="lw"><button class="lb" id="lb" type="button" aria-expanded="false"></button>
  <div class="lm" id="lm" hidden></div></div>
 <button class="tg" id="th" type="button" role="switch" aria-checked="false" aria-label="Theme">
  <span class="k"></span><span class="ic s"></span><span class="ic m"></span></button>
</div>
<div class="pad" id="v"></div>
<script>
var FW="%FW%",AP="%SSID%";
var L={en:{net:"Choose a network",pw:"Password",join:"Join",other:"Other network",rescan:"Scan again",
show:"Show password",back:"Back",joining:"Joining",done:"Connected",close:"You can close this page.",
badT:"Wrong password",badB:"Check it and try again.",scanning:"Searching",name:"Network name",
kName:"Name",kAddr:"Address",kMac:"MAC"},
fr:{net:"Choisissez un réseau",pw:"Mot de passe",join:"Rejoindre",other:"Autre réseau",
rescan:"Relancer la recherche",show:"Afficher le mot de passe",back:"Retour",joining:"Connexion",
done:"Connecté",close:"Vous pouvez fermer cette page.",badT:"Mot de passe incorrect",
badB:"Vérifiez-le et réessayez.",scanning:"Recherche",name:"Nom du réseau",kName:"Nom",
kAddr:"Adresse",kMac:"MAC"},
de:{net:"Netzwerk wählen",pw:"Passwort",join:"Verbinden",other:"Anderes Netzwerk",rescan:"Erneut suchen",
show:"Passwort anzeigen",back:"Zurück",joining:"Verbinde",done:"Verbunden",
close:"Sie können diese Seite schließen.",badT:"Falsches Passwort",badB:"Bitte prüfen und erneut versuchen.",
scanning:"Suche",name:"Netzwerkname",kName:"Name",kAddr:"Adresse",kMac:"MAC"},
es:{net:"Elige una red",pw:"Contraseña",join:"Conectar",other:"Otra red",rescan:"Buscar de nuevo",
show:"Mostrar contraseña",back:"Atrás",joining:"Conectando",done:"Conectado",
close:"Puedes cerrar esta página.",badT:"Contraseña incorrecta",badB:"Compruébala e inténtalo otra vez.",
scanning:"Buscando",name:"Nombre de red",kName:"Nombre",kAddr:"Dirección",kMac:"MAC"},
it:{net:"Scegli una rete",pw:"Password",join:"Connetti",other:"Altra rete",rescan:"Cerca di nuovo",
show:"Mostra password",back:"Indietro",joining:"Connessione",done:"Connesso",
close:"Puoi chiudere questa pagina.",badT:"Password errata",badB:"Controllala e riprova.",
scanning:"Ricerca",name:"Nome rete",kName:"Nome",kAddr:"Indirizzo",kMac:"MAC"},
pl:{net:"Wybierz sieć",pw:"Hasło",join:"Połącz",other:"Inna sieć",rescan:"Szukaj ponownie",
show:"Pokaż hasło",back:"Wstecz",joining:"Łączenie",done:"Połączono",
close:"Możesz zamknąć tę stronę.",badT:"Błędne hasło",badB:"Sprawdź je i spróbuj ponownie.",
scanning:"Szukam",name:"Nazwa sieci",kName:"Nazwa",kAddr:"Adres",kMac:"MAC"},
pt:{net:"Escolha uma rede",pw:"Senha",join:"Conectar",other:"Outra rede",rescan:"Procurar de novo",
show:"Mostrar senha",back:"Voltar",joining:"Conectando",done:"Conectado",
close:"Você pode fechar esta página.",badT:"Senha incorreta",badB:"Confira e tente de novo.",
scanning:"Procurando",name:"Nome da rede",kName:"Nome",kAddr:"Endereço",kMac:"MAC"},
ptpt:{net:"Escolha uma rede",pw:"Palavra-passe",join:"Ligar",other:"Outra rede",rescan:"Procurar de novo",
show:"Mostrar palavra-passe",back:"Voltar",joining:"A ligar",done:"Ligado",
close:"Pode fechar esta página.",badT:"Palavra-passe incorreta",badB:"Verifique e tente de novo.",
scanning:"A procurar",name:"Nome da rede",kName:"Nome",kAddr:"Endereço",kMac:"MAC"}};
var N={en:"English",fr:"Français",de:"Deutsch",es:"Español",it:"Italiano",pl:"Polski",
pt:"Português (BR)",ptpt:"Português (PT)"};
var st={lang:"%LANG%",view:"scanning",pick:"",light:false,nets:[],info:null,open:-1,bad:false};
if(!L[st.lang])st.lang="en";
var V=document.getElementById("v"),LB=document.getElementById("lb"),LM=document.getElementById("lm");
function t(k){return L[st.lang][k]}
function e(s){return String(s).replace(/[&<>"]/g,function(c){
return {"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;"}[c]})}
var SUN='<svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="4.2"/><path d="M12 2.4v2.4M12 19.2v2.4'+
'M4.2 12H1.8M22.2 12h-2.4M6.5 6.5 4.8 4.8M19.2 19.2l-1.7-1.7M17.5 6.5l1.7-1.7M4.8 19.2l1.7-1.7"/></svg>';
var MOON='<svg viewBox="0 0 24 24"><path d="M20.4 14.6A8.6 8.6 0 0 1 9.4 3.6a8.8 8.8 0 1 0 11 11z"/></svg>';
var CH='<span class="c"><svg viewBox="0 0 24 24" width="12" height="12" fill="none" '+
'stroke="currentColor" stroke-width="2.4" stroke-linecap="round"><path d="M5 9l7 7 7-7"/></svg></span>';
var LOCK='<svg viewBox="0 0 24 24" width="16" height="16"><path d="M8.6 10.6V7.3a3.4 3.4 0 0 1 6.8 0v3.3"'+
' fill="none" stroke="currentColor" stroke-width="1.85" stroke-linecap="round"/>'+
'<rect x="5.2" y="10.4" width="13.6" height="10.4" rx="3.2" fill="currentColor"/></svg>';
var EYE='<svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" '+
'stroke-width="1.85" stroke-linecap="round" stroke-linejoin="round"><path d="M2.4 12s3.8-6.5 9.6-6.5'+
'S21.6 12 21.6 12s-3.8 6.5-9.6 6.5S2.4 12 2.4 12Z"/><circle cx="12" cy="12" r="2.9"/></svg>';
var EYEOFF='<svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" '+
'stroke-width="1.85" stroke-linecap="round" stroke-linejoin="round"><path d="M10.2 6.1a9.9 9.9 0 0 1 1.8-.15'+
'c5.8 0 9.6 6.05 9.6 6.05a17.6 17.6 0 0 1-2.75 3.4"/><path d="M6.35 7.7A17.4 17.4 0 0 0 2.4 12s3.8 6.5 9.6 6.5'+
'a9.5 9.5 0 0 0 3.35-.6"/><path d="M9.95 9.95a2.9 2.9 0 0 0 4.1 4.1"/><path d="M3.6 3.6 20.4 20.4"/></svg>';
var TICK='<svg viewBox="0 0 24 24" width="28" height="28" fill="none" stroke="currentColor" '+
'stroke-width="2.6" stroke-linecap="round" stroke-linejoin="round"><path d="M4.5 12.5 10 18 19.5 6.5"/></svg>';
var WARN='<svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" '+
'stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="flex:0 0 18px;margin-top:1px">'+
'<path d="M12 3.6 22 20.4H2z"/><path d="M12 10v4"/><path d="M12 17.2v.1"/></svg>';
/* Wi-Fi is arcs, not bars: bars are the GSM symbol. Lit arcs are green, the
   state colour the device's own screens use for "reachable".
   The level is the device's own arithmetic - icons::wifiLevelFromRssi, which
   the TigerScale uses too - shifted up by one because this drawing counts the
   dot as its first step. One signal, one number, three places. */
function sig(n){var o="var(--ok)",f="currentColor";
function a(d,l){return '<path d="'+d+'" fill="none" stroke="'+(l?o:f)+'" stroke-width="2.1" '+
'stroke-linecap="round" opacity="'+(l?1:.22)+'"/>'}
return '<span class="sig"><svg viewBox="0 0 24 24" width="19" height="19">'+
a("M2.8 8.9a14.6 14.6 0 0 1 18.4 0",n>=4)+a("M6.1 12.7a9.9 9.9 0 0 1 11.8 0",n>=3)+
a("M9.3 16.4a5 5 0 0 1 5.4 0",n>=2)+'<circle cx="12" cy="19.8" r="1.35" fill="'+(n>=1?o:f)+
'" opacity="'+(n>=1?1:.22)+'"/></svg></span>'}
function bars(r){r=r>-40?-40:(r<-100?-100:r);var l=Math.trunc((r+100)*3/60);return (l<0?0:(l>3?3:l))+1}
function paintL(){LB.innerHTML="<span>"+e(N[st.lang])+"</span>"+CH;LM.innerHTML="";
Object.keys(N).forEach(function(k){var b=document.createElement("button");b.type="button";
b.setAttribute("aria-selected",String(k===st.lang));b.textContent=N[k];
b.onclick=function(ev){ev.stopPropagation();st.lang=k;LM.hidden=true;
LB.setAttribute("aria-expanded","false");fetch("/api/lang?l="+k);render()};LM.appendChild(b)})}
LB.onclick=function(ev){ev.stopPropagation();var h=LM.hidden;LM.hidden=!h;
LB.setAttribute("aria-expanded",String(h))};
document.onclick=function(){LM.hidden=true;LB.setAttribute("aria-expanded","false")};
document.getElementById("th").onclick=function(){st.light=!st.light;render()};
function render(){document.body.classList.toggle("light",st.light);
var th=document.getElementById("th");th.setAttribute("aria-checked",String(st.light));
th.querySelector(".s").innerHTML=SUN;th.querySelector(".m").innerHTML=MOON;paintL();
var h="",i;
if(st.view=="scanning"){h+='<div class="ct"><div class="sp"></div><h1>'+e(t("scanning"))+"</h1></div>"}
else if(st.view=="list"){h+="<h1>"+e(t("net"))+"</h1><div class=nets>";
for(i=0;i<st.nets.length;i++){var op=st.open===i;
h+='<div class="it'+(op?" op":"")+'" id=it'+i+'><button class=n data-i='+i+">"+
sig(bars(st.nets[i].r))+'<span class=nm>'+e(st.nets[i].s)+"</span>"+
(st.nets[i].k?'<span class=k>'+LOCK+"</span>":"")+"</button>";
/* The password opens under the network it belongs to. The name stays on
   screen, so there is nothing to remember and nothing to go back from. */
if(op){h+='<div class=exw id=exw><div><div class=ex>';
if(st.bad)h+='<div class=al>'+WARN+"<span><b>"+e(t("badT"))+"</b><span>"+e(t("badB"))+
"</span></span></div>";
h+="<label for=pw>"+e(t("pw"))+'</label><div class=fld><input id=pw type=password '+
'autocomplete=current-password autocapitalize=off autocorrect=off spellcheck=false>'+
'<button class=eye id=rv type=button aria-pressed=false aria-label="'+e(t("show"))+'">'+EYE+
'</button></div><button class=b data-g=1>'+e(t("join"))+"</button></div></div></div>"}
h+="</div>"}
h+='</div><button class=gh data-o=1>'+e(t("other"))+'</button><button class="b s" data-r=1>'+
e(t("rescan"))+"</button>"}
else if(st.view=="other"){h+="<h1>"+e(t("other"))+"</h1><label for=ss>"+e(t("name"))+
'</label><input id=ss autocapitalize=off autocorrect=off spellcheck=false style="padding-right:16px">'+
'<div style="height:16px"></div><label for=pw>'+e(t("pw"))+'</label><div class=fld>'+
'<input id=pw type=password autocomplete=current-password><button class=eye id=rv type=button '+
'aria-pressed=false aria-label="'+e(t("show"))+'">'+EYE+'</button></div>'+
'<button class=b data-g=1>'+e(t("join"))+'</button><button class="b s" data-b=1>'+e(t("back"))+"</button>"}
else if(st.view=="joining"){h+='<div class=ct><div class="sp"></div><h1>'+e(t("joining"))+
'</h1><p class=l style="margin:0">'+e(st.pick)+"</p></div>"}
else if(st.view=="done"){var d=st.info||{};
h+='<div class=ct><div class=tk>'+TICK+"</div><h1>"+e(t("done"))+
'</h1><p class=l style="margin:8px 0 0">'+e(st.pick)+'</p><p class=l style="margin:18px 0 0;'+
'font-size:14px">'+e(t("close"))+"</p><dl><dt>"+e(t("kName"))+"</dt><dd>"+e(d.host||"")+
"</dd><dt>"+e(t("kAddr"))+"</dt><dd>"+e(d.ip||"")+"</dd><dt>"+e(t("kMac"))+"</dt><dd>"+
e(d.mac||"")+"</dd></dl></div>"}
h+='<div class=ft>TigerSpool '+e(FW)+"</div>";V.innerHTML=h}
V.onclick=function(ev){
var r=ev.target.closest("#rv");
if(r){var f=document.getElementById("pw");var sh=r.getAttribute("aria-pressed")=="true";
var p=f.selectionStart;f.type=sh?"password":"text";r.setAttribute("aria-pressed",String(!sh));
r.innerHTML=sh?EYE:EYEOFF;f.focus();try{f.setSelectionRange(p,p)}catch(x){}return}
var n=ev.target.closest("[data-i]");
if(n){var i=+n.dataset.i,k=st.nets[i];st.pick=k.s;
if(!k.k){join("");return}
st.bad=false;
if(st.open===i){closeRow(function(){st.open=-1;render()});return}
if(st.open>=0){closeRow(function(){st.open=i;render();openRow(i)});return}
st.open=i;render();openRow(i);
return}
if(ev.target.closest("[data-o]")){st.pick="";st.view="other";render();return}
if(ev.target.closest("[data-b]")){st.view="list";st.open=-1;st.bad=false;render();return}
if(ev.target.closest("[data-r]")){scan();return}
if(ev.target.closest("[data-g]")){var ss=document.getElementById("ss");
if(ss)st.pick=ss.value;join(document.getElementById("pw").value)}};
/* Focus is all that is needed: the browser scrolls a focused input into view
   itself when the keyboard opens. Doing it here as well put two scrolls in
   flight at once, which is what makes a page judder. */
function openRow(i){var w=document.getElementById("exw");
// The class has to land on a frame AFTER the element exists, or there is no
// starting value to animate from and it snaps open.
if(w)requestAnimationFrame(function(){requestAnimationFrame(function(){w.classList.add("o")})});
// After the panel has grown, so the field is at its final position when the
// keyboard arrives.
setTimeout(function(){var f=document.getElementById("pw");if(f)f.focus()},300)}

// Collapse before the row disappears, so closing is the opening run backwards
// rather than the panel blinking out.
function closeRow(after){var w=document.getElementById("exw");
if(!w){after();return}
w.classList.remove("o");setTimeout(after,260)}

function scan(){st.view="scanning";render();
fetch("/api/scan").then(function(r){return r.json()}).then(function(j){
st.nets=j.nets||[];st.view="list";render()}).catch(function(){st.view="list";render()})}
/* Join, then verify, then report - the device does not reboot. The access point
   stays up until the association is confirmed, so a wrong password is reported
   while the user is still looking at the field they typed it into. */
function join(pw){st.view="joining";render();
fetch("/api/join",{method:"POST",headers:{"Content-Type":"application/json"},
body:JSON.stringify({ssid:st.pick,pass:pw})}).then(function(r){return r.json()})
.then(function(j){if(j.ok){st.info=j;st.view="done";render()}
else{st.bad=true;st.view="list";render();if(st.open>=0)openRow(st.open)}})
.catch(function(){st.bad=true;st.view="list";render();if(st.open>=0)openRow(st.open)})}
scan();
</script></body></html>)HTML";
