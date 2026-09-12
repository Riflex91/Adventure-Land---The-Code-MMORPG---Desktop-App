#!/usr/bin/env python3
import json, sqlite3, sys, pathlib
src=pathlib.Path(sys.argv[1]); out=pathlib.Path(sys.argv[2])
data=json.loads(src.read_text(encoding='utf-8'))
if out.exists(): out.unlink()
con=sqlite3.connect(out)
con.execute('PRAGMA journal_mode=WAL'); con.execute('PRAGMA synchronous=OFF')
con.executescript('''
CREATE TABLE meta(key TEXT PRIMARY KEY, value TEXT NOT NULL);
CREATE TABLE entities(
  section TEXT NOT NULL,
  id TEXT NOT NULL,
  name TEXT NOT NULL,
  kind TEXT NOT NULL DEFAULT '',
  description TEXT NOT NULL DEFAULT '',
  raw_json TEXT NOT NULL,
  sort_name TEXT NOT NULL,
  PRIMARY KEY(section,id)
) WITHOUT ROWID;
CREATE INDEX idx_entities_section_sort ON entities(section, sort_name);
CREATE TABLE entity_fields(
  section TEXT NOT NULL,id TEXT NOT NULL,key TEXT NOT NULL,value TEXT NOT NULL,ord INTEGER NOT NULL,
  PRIMARY KEY(section,id,key)
) WITHOUT ROWID;
CREATE INDEX idx_fields_entity ON entity_fields(section,id,ord);
CREATE VIRTUAL TABLE entities_fts USING fts5(section UNINDEXED,id UNINDEXED,name,kind,description,body, tokenize='unicode61 remove_diacritics 2');
''')
con.execute('INSERT INTO meta VALUES (?,?)',('schema_version','1'))
con.execute('INSERT INTO meta VALUES (?,?)',('source','Adventure Land Reference OS V11.1.3 curated seed'))
for k,v in data.get('meta',{}).items():
    con.execute('INSERT OR REPLACE INTO meta VALUES (?,?)',(f'seed.{k}',json.dumps(v,ensure_ascii=False) if not isinstance(v,str) else v))

def flatten_text(v,out):
    if v is None: return
    if isinstance(v,(str,int,float,bool)): out.append(str(v)); return
    if isinstance(v,list):
        for x in v: flatten_text(x,out)
    elif isinstance(v,dict):
        for k,x in v.items(): out.append(str(k)); flatten_text(x,out)

def name_of(d,id):
    return d.get('officialName') or (d.get('names') or {}).get('de') or (d.get('names') or {}).get('en') or d.get('name') or d.get('title') or id

def desc_of(d):
    return (d.get('descriptions') or {}).get('de') or (d.get('descriptions') or {}).get('en') or d.get('officialDescription') or d.get('explanation') or d.get('description') or ''

def scalar(v):
    if isinstance(v,bool): return 'YES' if v else 'NO'
    if isinstance(v,(str,int,float)) and not isinstance(v,bool): return str(v)
    return None

sections=['items','monsters','skills','classes','maps']
preferred=['type','category','damage_type','level','hp','mp','attack','armor','resistance','frequency','speed','range','xp','gold','g','crit','evasion','reflection','apiercing','rpiercing','output','cooldown']
for section in sections:
    vals=data.get(section,{})
    if isinstance(vals,list): vals={x.get('id'):x for x in vals if isinstance(x,dict) and x.get('id')}
    for id,d in vals.items():
        if not isinstance(d,dict): continue
        name=name_of(d,id); desc=desc_of(d)
        kind=str(d.get('type') or d.get('category') or d.get('damage_type') or '')
        parts=[]; flatten_text(d,parts); body=' '.join(parts)
        raw=json.dumps(d,ensure_ascii=False,separators=(',',':'))
        con.execute('INSERT INTO entities(section,id,name,kind,description,raw_json,sort_name) VALUES (?,?,?,?,?,?,?)',(section,id,name,kind,desc,raw,name.casefold()))
        con.execute('INSERT INTO entities_fts(section,id,name,kind,description,body) VALUES (?,?,?,?,?,?)',(section,id,name,kind,desc,body))
        fields=[]
        seen=set()
        def add(k,v):
            vv=scalar(v)
            if vv is None or not vv or len(vv)>180 or k in seen: return
            seen.add(k); fields.append((k,vv))
        for k in preferred:
            if k in d: add(k,d[k])
        for group in ['stats','effects','trade']:
            obj=d.get(group)
            if isinstance(obj,dict):
                for k,v in obj.items(): add(group+'.'+k,v)
        for ord_,(k,v) in enumerate(fields):
            con.execute('INSERT INTO entity_fields(section,id,key,value,ord) VALUES (?,?,?,?,?)',(section,id,k,v,ord_))
con.commit(); con.execute('PRAGMA optimize')
print('entities',con.execute('select count(*) from entities').fetchone()[0])
print(dict(con.execute('select section,count(*) from entities group by section')))
con.close()
