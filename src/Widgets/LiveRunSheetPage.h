#pragma once

// The live run sheet page (build 305), served at /runsheet by SheetCacheServer.
// Self-contained - venue networks are often offline, so nothing is fetched from
// anywhere but this client: it polls /runsheet.json once a second and counts
// down between polls on the phone's own clock, corrected to the client's.
// Kept under MSVC's 16 KB limit for one string literal.

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
@media (prefers-reduced-motion:reduce){*{transition:none!important}}
</style></head><body>
<header><div class="top"><span id="dot" title="Live"></span><h1 id="title">Run sheet</h1><span id="clock">--:--:--</span></div>
<nav id="tabs"></nav><div id="summary"></div></header>
<section id="air"></section>
<main id="list"></main>
<div id="empty" hidden>No rundown is open in the client.</div>
<script>
let data=null,offset=0,chosen=null,lastOk=0;
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
function render(){
 const rd=current();
 $('empty').hidden=!!rd;
 $('tabs').innerHTML=(data?data.rundowns:[]).map(r=>'<button class="'+(rd&&r.name===rd.name?'sel':'')+'" data-n="'+esc(r.name)+'">'+esc(r.name)+(r.onScreen?'<small>on screen</small>':'')+'</button>').join('');
 document.querySelectorAll('#tabs button').forEach(b=>b.onclick=()=>{chosen=b.dataset.n;render();});
 if(!rd){$('list').innerHTML='';$('air').innerHTML='';$('summary').textContent='';return;}
 $('title').textContent=rd.name;
 $('summary').textContent='Running time '+rd.total+(rd.hardOut?' · hard out '+rd.hardOut+' (start times back-timed)':' · start times from the top');
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
 const rd=current();if(!rd)return;
 document.querySelectorAll('.aircard').forEach(c=>{const it=rd.items.find(x=>String(x.n)===c.dataset.k);if(!it)return;
  const l=left(it),el=c.querySelector('.left');if(el&&l!=null)el.textContent=fmt(l);});
 document.querySelectorAll('.row.on').forEach((r,k)=>{const it=rd.items.filter(x=>x.state)[k];const el=r.querySelector('.cd');
  if(it&&el){const l=left(it);if(l!=null)el.textContent=fmt(l);}});}
poll();setInterval(tick,250);
</script></body></html>
)html";
}
