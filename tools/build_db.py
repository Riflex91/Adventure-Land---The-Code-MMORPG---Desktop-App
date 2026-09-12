#!/usr/bin/env python3
import json, sqlite3, sys, pathlib
src=pathlib.Path(sys.argv[1]); out=pathlib.Path(sys.argv[2])
snapshot=json.loads(src.read_text(encoding='utf-8'))
data=snapshot.get('data',snapshot)
if out.exists(): out.unlink()
out.parent.mkdir(parents=True,exist_ok=True)
con=sqlite3.connect(out)
con.execute('PRAGMA journal_mode=OFF'); con.execute('PRAGMA synchronous=OFF'); con.execute('PRAGMA temp_store=MEMORY')
con.executescript('''
CREATE TABLE meta(key TEXT PRIMARY KEY, value TEXT NOT NULL);
CREATE TABLE entities(
  section TEXT NOT NULL,id TEXT NOT NULL,name TEXT NOT NULL,kind TEXT NOT NULL DEFAULT '',
  description TEXT NOT NULL DEFAULT '',sort_name TEXT NOT NULL,raw_json TEXT NOT NULL,
  PRIMARY KEY(section,id)
) WITHOUT ROWID;
CREATE INDEX idx_entities_section_sort ON entities(section,sort_name);
CREATE TABLE entity_fields(
  section TEXT NOT NULL,id TEXT NOT NULL,ord INTEGER NOT NULL,key TEXT NOT NULL,value TEXT NOT NULL,
  PRIMARY KEY(section,id,ord)
) WITHOUT ROWID;
CREATE INDEX idx_fields_entity ON entity_fields(section,id,ord);
CREATE TABLE section_blobs(section TEXT PRIMARY KEY,json TEXT NOT NULL) WITHOUT ROWID;
CREATE VIRTUAL TABLE entities_fts USING fts5(section UNINDEXED,id UNINDEXED,name,kind,description,body,tokenize='unicode61 remove_diacritics 2');
''')
for k,v in snapshot.get('meta',{}).items():
    con.execute('INSERT OR REPLACE INTO meta VALUES (?,?)',(str(k),v if isinstance(v,str) else json.dumps(v,ensure_ascii=False,separators=(',',':'))))
con.execute('INSERT OR REPLACE INTO meta VALUES (?,?)',('schema_version','2'))

ENTITY_SECTIONS={'items','monsters','skills','classes','maps','npcs','craft','sets','tokens','dismantle','conditions','quests','events','achievements','projectiles','cosmetics','titles','games'}

def name_of(d,id):
    return d.get('name') or d.get('title') or d.get('officialName') or id

def desc_of(d):
    return d.get('explanation') or d.get('description') or d.get('officialDescription') or ''

def kind_of(d):
    return d.get('type') or d.get('kind') or d.get('role') or d.get('damage_type') or ''

def scalar(v):
    if v is None:return 'null'
    if isinstance(v,bool):return 'true' if v else 'false'
    if isinstance(v,(str,int,float)):return str(v)
    return json.dumps(v,ensure_ascii=False,separators=(',',':'))

def flatten(v,prefix='',depth=0,out=None):
    if out is None:out=[]
    if depth>4 or len(out)>300:return out
    if isinstance(v,dict):
        for k,x in v.items():
            p=f'{prefix}.{k}' if prefix else str(k)
            if isinstance(x,(dict,list)):flatten(x,p,depth+1,out)
            else:out.append((p,scalar(x)))
    elif isinstance(v,list):
        for i,x in enumerate(v[:80]):
            p=f'{prefix}[{i}]'
            if isinstance(x,(dict,list)):flatten(x,p,depth+1,out)
            else:out.append((p,scalar(x)))
    return out

for section,obj in data.items():
    con.execute('INSERT OR REPLACE INTO section_blobs VALUES (?,?)',(section,json.dumps(obj,ensure_ascii=False,separators=(',',':'))))
    if section not in ENTITY_SECTIONS or not isinstance(obj,dict):continue
    for id,d in obj.items():
        if not isinstance(d,(dict,list)):continue
        if isinstance(d,dict):name=name_of(d,id);desc=desc_of(d);kind=str(kind_of(d) or '')
        else:name=id;desc='';kind=''
        raw=json.dumps(d,ensure_ascii=False,separators=(',',':'))
        fs=flatten(d)
        body=' '.join([id,name,kind,desc]+[f'{k} {v}' for k,v in fs])
        con.execute('INSERT INTO entities VALUES (?,?,?,?,?,?,?)',(section,id,name,kind,desc,name.casefold(),raw))
        con.execute('INSERT INTO entities_fts VALUES (?,?,?,?,?,?)',(section,id,name,kind,desc,body))
        for ord_,(k,v) in enumerate(fs): con.execute('INSERT INTO entity_fields VALUES (?,?,?,?,?)',(section,id,ord_,k,v[:2000]))

con.execute('INSERT OR REPLACE INTO meta VALUES (?,?)',('entity_count',str(con.execute('SELECT count(*) FROM entities').fetchone()[0])))
con.commit(); con.execute('PRAGMA optimize'); con.execute('VACUUM');
print('entities',con.execute('select count(*) from entities').fetchone()[0])
print(dict(con.execute('select section,count(*) from entities group by section')))
print('db_bytes',out.stat().st_size)
con.close()
