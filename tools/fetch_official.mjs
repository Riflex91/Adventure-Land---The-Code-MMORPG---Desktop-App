#!/usr/bin/env node
import fs from 'node:fs';
import vm from 'node:vm';
import crypto from 'node:crypto';
import path from 'node:path';

const out = process.argv[2] || 'resources/official_snapshot.json';
const repo = 'kaansoral/adventureland_mongodb';
const files = [
  'design/items.js','design/skills.js','design/monsters.js','design/classes.js',
  'design/npcs.js','design/maps.js','design/recipes.js','design/upgrades.js','design/multipliers.js',
  'design/dimensions.js','design/sprites.js','design/drops.js','design/tokens.js','design/conditions.js',
  'design/quests.js','design/events.js','design/achievements.js','design/animations.js','design/cosmetics.js',
  'design/projectiles.js','design/titles.js','design/games.js','design/levels.js'
];
const vars = ['items','sets','skills','monsters','classes','npcs','maps','craft','dismantle','drops','tokens','conditions','quests','events','achievements','animations','cosmetics','projectiles','titles','games','levels','positions','dimensions','sprites','imagesets','tilesets','upgrades','compounds','multipliers'];

async function get(url, tries=4) {
  let last;
  for (let i=0;i<tries;i++) {
    try {
      const r = await fetch(url,{headers:{'user-agent':'AdventureLandReferenceOS-Native-Build/1.0','accept':'application/vnd.github+json'}});
      if (!r.ok) throw new Error(`${r.status} ${r.statusText}`);
      return await r.text();
    } catch (e) { last=e; await new Promise(r=>setTimeout(r,700*(i+1))); }
  }
  throw last;
}

let revision = {sha:'main',date:'',message:'',repository:repo,branch:'main'};
try {
  const meta = JSON.parse(await get(`https://api.github.com/repos/${repo}/commits/main`));
  revision = {sha:meta.sha||'main',date:meta.commit?.committer?.date||'',message:meta.commit?.message||'',repository:repo,branch:'main'};
} catch (e) { console.warn('revision lookup failed:',e.message); }

const context = vm.createContext({
  console:{log(){},warn(){},error(){}},
  module:{exports:{}}, exports:{},
  require(){return {};},
  Math, Date, JSON, Object, Array, Number, String, Boolean, RegExp,
  parseInt, parseFloat, isNaN, Infinity, NaN,
  min:Math.min,max:Math.max,round:Math.round,floor:Math.floor,ceil:Math.ceil
});
context.global=context; context.globalThis=context; context.window=context; context.self=context;
const sourceManifest=[]; const errors=[];
for (const file of files) {
  const ref = revision.sha && revision.sha!=='main' ? revision.sha : 'main';
  const url = `https://raw.githubusercontent.com/${repo}/${ref}/${file}`;
  try {
    const text = await get(url);
    sourceManifest.push({path:file,bytes:Buffer.byteLength(text),sha256:crypto.createHash('sha256').update(text).digest('hex')});
    try { vm.runInContext(text,context,{filename:file,timeout:4000}); }
    catch (e) { errors.push({file,error:String(e?.message||e)}); }
  } catch (e) { errors.push({file,error:`download: ${e.message}`}); }
}

const data={};
for (const key of vars) {
  const value = context[key];
  if (value !== undefined) {
    try { data[key]=JSON.parse(JSON.stringify(value,(k,v)=>typeof v==='function'?undefined:v)); }
    catch (e) { errors.push({file:key,error:`serialize: ${e.message}`}); data[key]={}; }
  } else data[key]={};
}

for (const req of ['items','skills','monsters','classes','maps','drops']) {
  const n = data[req] && typeof data[req]==='object' ? Object.keys(data[req]).length : 0;
  if (!n) throw new Error(`required section ${req} is empty`);
}
const counts={}; for (const [k,v] of Object.entries(data)) counts[k]=v&&typeof v==='object'?Object.keys(v).length:0;
const manifestHash=crypto.createHash('sha256').update(sourceManifest.map(x=>`${x.path}:${x.sha256}`).join('\n')).digest('hex');
const snapshot={
  meta:{schema_version:'2',source:'Adventure Land official GitHub',source_mode:'official-build',revision:revision.sha,revision_date:revision.date,revision_message:revision.message,repository:repo,branch:'main',manifest_sha256:manifestHash,built_at:new Date().toISOString(),source_errors:errors},
  data,
  sources:sourceManifest,
  errors
};
fs.mkdirSync(path.dirname(path.resolve(out)),{recursive:true});
fs.writeFileSync(out,JSON.stringify(snapshot));
console.log('revision',revision.sha);
console.log('counts',counts);
console.log('errors',errors.length);
console.log('snapshot_bytes',fs.statSync(out).size);
