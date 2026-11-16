#pragma once

// The live run sheet page (build 305), served at /runsheet by SheetCacheServer.
// Self-contained - venue networks are often offline, so nothing is fetched from
// anywhere but this client: it polls /runsheet.json once a second and counts
// down between polls on the phone's own clock, corrected to the client's.
// Kept under MSVC's 16 KB limit for one string literal.
//
// &output=1 (build 308) is the form a server plays on a channel: no links,
// no cursor.
//
// ?view=presenter (build 306) shows only what is on air, big, and what comes
// next with its note - for a confidence monitor or a tablet on the desk. Which
// item is "on air" and which is "next" is chosen by the client (primary, next).

namespace LiveRunSheetPage
{
    static const char* const HTML = R"html(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Run sheet</title>
<style>
:root{--bg:#101214;--panel:#181b1f;--line:#2a2f35;--text:#e8eaed;--dim:#8a9099;--air:#e5484d;--airbg:#3a1416;--pause:#d6a035;--accent:#4c9aff}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:15px/1.35 -apple-system,"Segoe UI",Roboto,Arial,sans-serif}
header{position:sticky;top:0;z-index:2;background:var(--bg);border-bottom:1px solid var(--line);padding:10px 14px}
.top{display:flex;align-items:center;gap:12px}
h1{font-size:17px;margin:0;flex:1}
#clock{font:600 22px/1 ui-monospace,Consolas,monospace;font-variant-numeric:tabular-nums}
#dot{width:10px;height:10px;border-radius:50%;background:#3fb950}
#dot.stale{background:var(--air)}
nav{display:flex;gap:6px;overflow-x:auto;margin-top:10px}
nav button{flex:none;background:var(--panel);color:var(--text);border:1px solid var(--line);border-radius:6px;padding:6px 10px;font-size:14px}
nav button.sel{border-color:var(--accent);color:#fff}
nav small{color:var(--dim);margin-left:4px}
#summary{color:var(--dim);font-size:13px;margin-top:8px}
#air{padding:0 14px}
.aircard{margin-top:10px;background:var(--airbg);border-left:4px solid var(--air);border-radius:6px;padding:10px 12px}
.aircard.paused{border-left-color:var(--pause);background:#33280f}
.aircard .name{font-weight:600}
.aircard .left{font:700 34px/1.1 ui-monospace,Consolas,monospace;font-variant-numeric:tabular-nums;margin-top:4px}
.aircard .ends{color:var(--dim);font-size:13px}
main{padding:6px 14px 40px}
.row{display:flex;gap:10px;padding:9px 0;border-bottom:1px solid var(--line)}
.row .n{width:28px;color:var(--dim);font-variant-numeric:tabular-nums;text-align:right;flex:none}
.row .body{flex:1;min-width:0}
.row .label{font-weight:600;overflow-wrap:anywhere}
.row .meta{color:var(--dim);font-size:13px;margin-top:2px}
.row .note{font-size:13px;font-style:italic;color:#c9cdd3;margin-top:3px;white-space:pre-wrap}
.row .tail{flex:none;text-align:right;font-variant-numeric:tabular-nums}
.row .start{color:var(--dim);font-size:13px}
.row.on{background:linear-gradient(90deg,var(--airbg),transparent 70%);border-left:4px solid var(--air);padding-left:6px}
.row.on.paused{border-left-color:var(--pause)}
.row.off{opacity:.45}
.badge{display:inline-block;font-size:11px;font-weight:700;letter-spacing:.04em;border-radius:3px;padding:1px 5px;margin-left:6px;background:var(--air);color:#fff;vertical-align:middle}
.badge.paused{background:var(--pause);color:#111}
.cd{font:700 18px ui-monospace,Consolas,monospace}
#empty{color:var(--dim);padding:30px 14px}
#mode{color:var(--accent);font-size:13px;text-decoration:none;flex:none}
#pv{display:none}
body.presenter nav,body.presenter #summary,body.presenter main,body.presenter #air{display:none}
body.presenter #pv{display:flex;flex-direction:column;gap:2vh;padding:2vh 5vw 4vh}
.pvl{font-size:clamp(13px,2vw,26px);letter-spacing:.14em;font-weight:700;color:var(--dim)}
.pvl.air{color:var(--air)}
.pvt{font-size:clamp(26px,min(9vw,9vh),104px);font-weight:700;line-height:1.05;overflow-wrap:anywhere}
.pvc{font:800 clamp(64px,min(26vw,27vh),280px)/1 ui-monospace,Consolas,monospace;font-variant-numeric:tabular-nums}
.pvc.warn{color:var(--pause)}
.pvc.urgent{color:var(--air)}
.pve{font-size:clamp(15px,min(2.6vw,4vh),38px);color:var(--dim)}
.pvo{font-size:clamp(14px,2.2vw,30px);color:#c9cdd3}
.pvn .pvt{font-size:clamp(22px,min(4.6vw,7vh),74px);color:#cfd3d8}
.pvnote{font-style:italic;font-size:clamp(17px,min(3.2vw,5vh),48px);color:#f0c674;white-space:pre-wrap}
#hard{font-size:clamp(14px,2.2vw,30px);color:var(--dim)}
#hard.over{color:var(--air)}
body.output{overflow:hidden;cursor:none}
body.output #mode{display:none}
body.output header{border-bottom:0}
body.presenter.lost #pv{opacity:.3}
body.presenter.lost #title::after{content:" - no answer from the client";color:var(--air)}
@media (prefers-reduced-motion:reduce){*{transition:none!important}}
</style></head><body>
<header><div class="top"><span id="dot" title="Live"></span><h1 id="title">Run sheet</h1><a id="mode" href="?view=presenter">Presenter view</a><span id="clock">--:--:--</span></div>
<nav id="tabs"></nav><div id="summary"></div></header>
<section id="pv"></section>
<section id="air"></section>
<main id="list"></main>
<div id="empty" hidden>No rundown is open in the client.</div>
<script>
let data=null,offset=0,chosen=null,lastOk=0;
const qs=new URLSearchParams(location.search),presenter=qs.get('view')==='presenter',output=qs.get('output')==='1';
const $=id=>document.getElementById(id);
const esc=s=>String(s==null?'':s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
function fmt(sec){sec=Math.max(0,Math.round(sec));const h=Math.floor(sec/3600),m=Math.floor(sec%3600/60),s=sec%60;
 return (h?h+':'+String(m).padStart(2,'0'):m)+':'+String(s).padStart(2,'0');}
function clockOf(ms){const d=new Date(ms);return [d.getHours(),d.getMinutes(),d.getSeconds()].map(v=>String(v).padStart(2,'0')).join(':');}
function left(it){const nowMs=Date.now()+offset;
 if(it.endsAt!=null)return Math.max(0,(it.endsAt-nowMs)/1000);
 return it.remaining!=null?it.remaining:null;}
async function poll(){
 try{const t0=Date.now();const r=await fetch('/runsheet.json',{cache:'no-store'});const j=await r.json();
  const t1=Date.now();offset=j.now-(t0+t1)/2;data=j;lastOk=t1;render();}
 catch(e){}
 setTimeout(poll,1000);}
function current(){if(!data||!data.rundowns.length)return null;
 let r=data.rundowns.find(x=>x.name===chosen);if(!r)r=data.rundowns.find(x=>x.onScreen)||data.rundowns[0];return r;}
function hardOutLeft(rd){if(!rd||!rd.hardOut)return null;const p=rd.hardOut.split(':').map(Number);
 const now=new Date(Date.now()+offset),t=new Date(now);t.setHours(p[0],p[1]||0,p[2]||0,0);
 let d=(t-now)/1000;if(d<-43200)d+=86400;if(d>43200)d-=86400;return d;}
function renderPresenter(rd){
 const pr=rd.items.find(i=>i.n===rd.primary),nx=rd.items.find(i=>i.n===rd.next);
 const others=rd.items.filter(i=>i.state&&(!pr||i.n!==pr.n));
 let h='<div id="hard"></div>';
 if(pr){const l=left(pr);
  h+='<div><div class="pvl air">'+(pr.state==='paused'?'PAUSED':'ON AIR')+'</div><div class="pvt">'+esc(pr.label)+'</div>'+
   (l!=null?'<div class="pvc" id="pvc">'+fmt(l)+'</div><div class="pve" id="pve">'+(pr.state==='paused'?'paused':'ends '+clockOf(Date.now()+offset+l*1000))+'</div>':'<div class="pve">on air, no end set</div>')+
   (others.length?'<div class="pvo">Also on air: '+others.map(o=>esc(o.label)).join(' \u00b7 ')+'</div>':'')+'</div>';}
 else h+='<div><div class="pvl">ON AIR</div><div class="pvt" style="color:var(--dim)">Nothing on air</div></div>';
 h+='<div class="pvn"><div class="pvl">NEXT</div>'+(nx?'<div class="pvt">'+esc(nx.label)+'</div>'+(nx.length?'<div class="pve">'+esc(nx.length)+'</div>':'')+
  (nx.note?'<div class="pvnote">'+esc(nx.note)+'</div>':''):'<div class="pvt" style="color:var(--dim)">End of the rundown</div>')+'</div>';
 $('pv').innerHTML=h;tickPresenter(rd);}
function tickPresenter(rd){
 const hd=$('hard');if(hd){const s=hardOutLeft(rd);hd.className=s!=null&&s<0?'over':'';
  hd.textContent=s==null?'':(s>=0?'Show ends in '+fmt(s)+' (hard out '+rd.hardOut+')':'Over the hard out by '+fmt(-s));}
 const pr=rd.items.find(i=>i.n===rd.primary),c=$('pvc');if(!pr||!c)return;const l=left(pr);if(l==null)return;
 c.textContent=fmt(l);c.className='pvc'+(pr.state==='paused'?'':(l<=10?' urgent':(l<=30?' warn':'')));
 const e=$('pve');if(e&&pr.state!=='paused')e.textContent='ends '+clockOf(Date.now()+offset+l*1000);}
function loudText(){const l=(data&&data.loudness)||[];if(!l.length)return'';
 return' · '+l.map(x=>x.name+' '+(x.est?'~':'')+(x.s||'\u2013')+' LUFS'+(x.i?' (I '+(x.est?'~':'')+x.i+')':'')).join(', ');}
function render(){
 const rd=current();
 $('empty').hidden=!!rd;
 $('tabs').innerHTML=(data?data.rundowns:[]).map(r=>'<button class="'+(rd&&r.name===rd.name?'sel':'')+'" data-n="'+esc(r.name)+'">'+esc(r.name)+(r.onScreen?'<small>on screen</small>':'')+'</button>').join('');
 document.querySelectorAll('#tabs button').forEach(b=>b.onclick=()=>{chosen=b.dataset.n;render();});
 if(!rd){$('list').innerHTML='';$('air').innerHTML='';$('summary').textContent='';return;}
 $('title').textContent=rd.name;
 if(presenter){renderPresenter(rd);return;}
 $('summary').textContent='Running time '+rd.total+(rd.hardOut?' · hard out '+rd.hardOut+' (start times back-timed)':' · start times from the top')+loudText();
 const on=rd.items.filter(i=>i.state);
 $('air').innerHTML=on.map(i=>{const l=left(i);
  return '<div class="aircard '+(i.state==='paused'?'paused':'')+'" data-k="'+i.n+'"><div class="name">'+esc(i.label)+
  ' <span class="badge '+(i.state==='paused'?'paused':'')+'">'+(i.state==='paused'?'PAUSED':'ON AIR')+'</span></div>'+
  (l!=null?'<div class="left">'+fmt(l)+'</div><div class="ends">'+(i.state==='paused'?'paused':'ends '+clockOf(Date.now()+offset+l*1000))+(i.where?' · '+esc(i.where):'')+'</div>':
  '<div class="ends">on air, no end set'+(i.where?' · '+esc(i.where):'')+'</div>')+'</div>';}).join('');
 $('list').innerHTML=rd.items.map(i=>{const l=left(i);const cls='row'+(i.state?' on':'')+(i.state==='paused'?' paused':'')+(i.disabled?' off':'');
  return '<div class="'+cls+'"><div class="n">'+i.n+'</div><div class="body" style="padding-left:'+(i.depth*14)+'px">'+
  '<div class="label">'+esc(i.label)+(i.state?' <span class="badge '+(i.state==='paused'?'paused':'')+'">'+(i.state==='paused'?'PAUSED':'ON AIR')+'</span>':'')+'</div>'+
  '<div class="meta">'+[i.type,i.where,i.length].filter(Boolean).map(esc).join(' · ')+(i.disabled?' · disabled':'')+'</div>'+
  (i.note?'<div class="note">'+esc(i.note)+'</div>':'')+'</div><div class="tail">'+
  (i.state&&l!=null?'<div class="cd">'+fmt(l)+'</div>':'')+(i.start?'<div class="start">'+esc(i.start)+'</div>':'')+'</div></div>';}).join('');}
function tick(){
 $('clock').textContent=clockOf(Date.now()+offset);
 $('dot').className=(Date.now()-lastOk>3500)?'stale':'';
 $('dot').title=(Date.now()-lastOk>3500)?'No answer from the client':'Live';
 document.body.classList.toggle('lost',Date.now()-lastOk>3500);
 const rd=current();if(!rd)return;
 if(presenter){tickPresenter(rd);return;}
 document.querySelectorAll('.aircard').forEach(c=>{const it=rd.items.find(x=>String(x.n)===c.dataset.k);if(!it)return;
  const l=left(it),el=c.querySelector('.left');if(el&&l!=null)el.textContent=fmt(l);});
 document.querySelectorAll('.row.on').forEach((r,k)=>{const it=rd.items.filter(x=>x.state)[k];const el=r.querySelector('.cd');
  if(it&&el){const l=left(it);if(l!=null)el.textContent=fmt(l);}});}
document.body.classList.toggle('presenter',presenter);document.body.classList.toggle('output',output);
$('mode').textContent=presenter?'Full list':'Presenter view';$('mode').href=presenter?'?':'?view=presenter';
poll();setInterval(tick,250);
</script></body></html>
)html";
}
