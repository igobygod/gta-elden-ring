using SoulsFormats;
using System.Text;
using System.Text.Json;
Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);
// igobygod local-only regulation recipe. No game data is bundled.
var options=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
for(int i=0;i<args.Length;i++){
 if(args[i]=="--help"){Console.WriteLine("ERParams --input REGULATION --output NEW_REGULATION --paramdefs DIRECTORY [--compare INSTALLED_REGULATION] [--fingerprints HEADER]");return;}
 if(!args[i].StartsWith("--"))throw new ArgumentException("Expected a named option");
 var parts=args[i].Substring(2).Split('=',2);string value;
 if(parts.Length==2)value=parts[1];else{if(++i>=args.Length)throw new ArgumentException("Missing option value");value=args[i];}
 if(!new[]{"input","output","paramdefs","compare","fingerprints"}.Contains(parts[0]))throw new ArgumentException("Unknown option: "+parts[0]);
 if(!options.TryAdd(parts[0],value))throw new ArgumentException("Duplicate option: "+parts[0]);
}
string Required(string name)=>options.TryGetValue(name,out var value)?Path.GetFullPath(value):throw new ArgumentException("Missing --"+name);
var input=Required("input");var output=Required("output");var paramdefs=Required("paramdefs");
var header=options.TryGetValue("fingerprints",out var headerPath)?Path.GetFullPath(headerPath):Path.Combine(Path.GetDirectoryName(output)!,"expected_params.h");
var comparison=options.TryGetValue("compare",out var comparePath)?Path.GetFullPath(comparePath):null;
if(string.Equals(input,output,StringComparison.OrdinalIgnoreCase))throw new Exception("Input and output must differ; game files are never edited");
if(File.Exists(output)||File.Exists(header))throw new Exception("Output already exists; choose a new output directory");
var inputDirectory=Path.GetDirectoryName(input)!;
foreach(var path in new[]{output,header})if(path.StartsWith(inputDirectory+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase))throw new Exception("Generated files must be outside the source regulation directory");
foreach(var name in new[]{"combat-param-report.json","sword-absorption.json","verification.json"})if(File.Exists(Path.Combine(Path.GetDirectoryName(output)!,name)))throw new Exception("Generated report already exists; choose a new output directory");
if(!File.Exists(input))throw new FileNotFoundException("Input regulation was not found");
string Definition(string table){
 var names=table=="EquipParamWeapon"?new[]{"EquipParamWeapon.xml"}:table=="Bullet"?new[]{"BulletParam.xml","BULLET_PARAM_ST.xml"}:new[]{"AtkParam.xml","ATK_PARAM_ST.xml"};
 return names.Select(name=>Path.Combine(paramdefs,name)).FirstOrDefault(File.Exists)??throw new FileNotFoundException("Missing definition for "+table);
}
var b=SFUtil.DecryptERRegulation(input);
var reports=new List<object>();
var bulletSources=new Dictionary<int,int>{[98000000]=20007000,[98000100]=10030000,[98000101]=10030001,[98000102]=10030002,[98000103]=10030003,[98000200]=98000000,[98000300]=20009200,[98000301]=20009201};
var attackSources=new Dictionary<int,int>{[98000000]=10176000,[98000100]=10030000,[98000101]=10030001,[98000200]=98000000,[98000300]=5302300,[98000301]=5302301};
void CompareRow(PARAM.Row expected,PARAM.Row actual,string context){
 foreach(var cell in expected.Cells){var got=actual[cell.Def.InternalName].Value;
  bool same=cell.Value is byte[] bytes?bytes.SequenceEqual((byte[])got):Equals(cell.Value,got);
  if(!same)throw new Exception(context+" row "+expected.ID+" field "+cell.Def.InternalName);
 }
}
PARAM Load(string table) {
 var p=PARAM.Read(b.Files.Single(f=>f.Name.EndsWith("\\"+table+".param")).Bytes);
 var def=PARAMDEF.XmlDeserialize(Definition(table));
 if(!p.ApplyParamdefCarefully(def))throw new Exception("Definition mismatch: "+table);return p;
}
void Set(PARAM.Row r,string field,object value) {r[field].Value=Convert.ChangeType(value,r[field].Value.GetType());}
PARAM.Row Clone(PARAM p,int source,int id,string name) {var r=new PARAM.Row(id,name,p.AppliedParamdef);foreach(var c in p.Rows.Single(r=>r.ID==source).Cells)r[c.Def.InternalName].Value=c.Value is byte[] a?(byte[])a.Clone():c.Value;return r;}
void Store(string table,PARAM p){b.Files.Single(f=>f.Name.EndsWith("\\"+table+".param")).Bytes=p.Write();}
var bullets=Load("Bullet");var attacks=Load("AtkParam_Pc");
var bulletIds=new[]{98000000,98000100,98000101,98000102,98000103,98000200,98000300,98000301,98000400,98000500,98000600,98000700,98000701};var attackIds=new[]{98000000,98000100,98000101,98000200,98000300,98000301,98000400,98000500,98000600,98000700,98000701};
if(bullets.Rows.Any(r=>bulletIds.Contains(r.ID))||attacks.Rows.Any(r=>attackIds.Contains(r.ID)))throw new Exception("Custom row ID already exists; refuse collision");
var bullet=Clone(bullets,20007000,98000000,"Franklin native rifle (bolt visual adapter)");
foreach(var (key,value) in new Dictionary<string,object>{
 ["atkId_Bullet"]=98000000,["life"]=0.06f,["dist"]=120f,["initVellocity"]=2000f,["minVellocity"]=2000f,["maxVellocity"]=2000f,
 ["gravityInRange"]=0f,["gravityOutRange"]=0f,["accelInRange"]=0f,["accelOutRange"]=0f,["HitBulletID"]=-1,
 ["isPenetrateChr"]=0,["isPenetrateObj"]=0,["isPenetrateMap"]=0,["numShoot"]=1,["isEnableAutoHoming"]=0,["lockShootLimitAng"]=0,
})Set(bullet,key,value);
// A target=-1 spawn inherits the owner's Q target. App1.17.1 initial launch
// correction loads Bullet+0x91 even when ongoing homing is disabled. A zero
// lockShootLimitAng preserves our explicit reticle direction (RVA38F130).
// All five firearm rows clone this base; other/native rows stay untouched.
// Ruin Fragment supplies native hit geometry/flinch; independent damage is not proven.
// Custom PC additive flag below is a repair hypothesis pending controlled live proof.
var attack=Clone(attacks,10176000,98000000,"Franklin native rifle attack");
foreach(var (key,value) in new Dictionary<string,object>{["atkPhys"]=140,["atkPhysCorrection"]=100,["dmgLevel"]=1,["atkSuperArmor"]=12f,["atkAttribute"]=2,["atkMag"]=0,["atkFire"]=0,["atkThun"]=0,["atkDark"]=0})Set(attack,key,value);
bullets.Rows.Add(bullet);attacks.Rows.Add(attack);
// Retain the complete native Fire Pot flight -> explosion -> debris -> spark chain.
for(int i=0;i<4;i++){var r=Clone(bullets,10030000+i,98000100+i,"Franklin grenade native Fire Pot adapter "+i);if(i<3)Set(r,"HitBulletID",98000101+i);if(i<2)Set(r,"atkId_Bullet",98000100+i);bullets.Rows.Add(r);}
for(int i=0;i<2;i++)attacks.Rows.Add(Clone(attacks,10030000+i,98000100+i,"Franklin grenade native attack "+i));bullets.Rows.Sort((a,c)=>a.ID.CompareTo(c.ID));attacks.Rows.Sort((a,c)=>a.ID.CompareTo(c.ID));
// Source weapons.meta: WEAPON_PISTOL/Damage=26. This only adapts base power;
// ER defence/scaling applies and the native bolt collision/visual path remains.
var pistol=Clone(bullets,98000000,98000200,"Franklin pistol");Set(pistol,"atkId_Bullet",98000200);bullets.Rows.Add(pistol);
var pistolAtk=Clone(attacks,98000000,98000200,"Franklin pistol attack");Set(pistolAtk,"atkPhys",100);attacks.Rows.Add(pistolAtk);
// Complete native explosive greatbolt flight/impact chain for the launcher.
for(int i=0;i<2;i++){
 var launcher=Clone(bullets,20009200+i,98000300+i,"Franklin launcher native projectile "+i);
 Set(launcher,"atkId_Bullet",98000300+i);if(i==0){Set(launcher,"HitBulletID",98000301);Set(launcher,"isPenetrateObj",0);}
 bullets.Rows.Add(launcher);attacks.Rows.Add(Clone(attacks,5302300+i,98000300+i,"Franklin launcher attack "+i));
}
// Balanced powers: shotgun50perpellet (8 explicit native spawns), SMG80,sniper400.
foreach(var (id,damage,count,spread) in new[]{(98000400,50,1,0),(98000500,80,1,0),(98000600,400,1,0)}){
 var projectile=Clone(bullets,98000000,id,"GTA isolated firearm "+id);Set(projectile,"atkId_Bullet",id);Set(projectile,"numShoot",count);Set(projectile,"shootAngle",spread);if(id==98000600){Set(projectile,"dist",250f);Set(projectile,"life",0.125f);}
 bullets.Rows.Add(projectile);var damageRow=Clone(attacks,98000000,id,"GTA balanced additive firearm damage");Set(damageRow,"atkPhys",damage);attacks.Rows.Add(damageRow);bulletSources[id]=98000000;attackSources[id]=98000000;
}
// Original GTA AMMO_GRENADELAUNCHER:25m/s, GravityFactor1,30s lifetime.
// Keep one deterministic native trajectory so the low-arc reticle solver and
// collision projectile use identical motion, without greatbolt range switching.
var glFlight=bullets.Rows.Single(r=>r.ID==98000300);
foreach(var (f,v) in new Dictionary<string,float>{{"life",30f},{"dist",750f},{"initVellocity",25f},{"minVellocity",25f},{"maxVellocity",25f},{"accelInRange",0f},{"accelOutRange",0f},{"gravityInRange",9.81f},{"gravityOutRange",9.81f}})Set(glFlight,f,v);
// Fixed combat powers after additive damage was verified too high in live footage.
foreach(var id in new[]{98000100,98000101,98000300,98000301}){var r=attacks.Rows.Single(r=>r.ID==id);foreach(var field in new[]{"atkPhys","atkMag","atkFire","atkThun","atkDark"})Set(r,field,field=="atkFire"?(id==98000101?500:id==98000301?650:0):0);}
// Grenade flight is handled by bounded game-thread fuse physics; native row is
// harmless if accidentally spawned. Blast remains native player-owned child.
Set(bullets.Rows.Single(r=>r.ID==98000100),"atkId_Bullet",-1);Set(bullets.Rows.Single(r=>r.ID==98000100),"HitBulletID",-1);
// Original GTA AMMO_RPG: LaunchSpeed400m/s, LifeTime5s, GravityFactor0.02.
// ER Bullet definition documents downward gravity in m/s^2: factor*9.81.
for(int i=0;i<2;i++){
 var r=Clone(bullets,98000300+i,98000700+i,"Original RPG native collision adapter");Set(r,"atkId_Bullet",98000700+i);
 if(i==0){Set(r,"HitBulletID",98000701);foreach(var (f,v) in new Dictionary<string,float>{{"life",5f},{"dist",2000f},{"initVellocity",400f},{"minVellocity",400f},{"maxVellocity",400f},{"accelInRange",0f},{"accelOutRange",0f},{"gravityInRange",0.1962f},{"gravityOutRange",0.1962f}})Set(r,f,v);}
 bullets.Rows.Add(r);attacks.Rows.Add(Clone(attacks,98000300+i,98000700+i,"Balanced RPG additive damage"));bulletSources[98000700+i]=98000300+i;attackSources[98000700+i]=98000300+i;
}
bullets.Rows.Sort((a,c)=>a.ID.CompareTo(c.ID));attacks.Rows.Sort((a,c)=>a.ID.CompareTo(c.ID));
// Flight must stop at enemies, dynamic objects and map geometry. Area explosions
// retain native penetration flags so their radius can contact multiple targets.
void Require(PARAM p,int id,string field,int expected){if(Convert.ToInt32(p.Rows.Single(r=>r.ID==id)[field].Value)!=expected)throw new Exception("Invalid custom param "+id+" "+field);}
foreach(var id in new[]{98000000,98000200,98000100,98000300})foreach(var field in new[]{"isPenetrateChr","isPenetrateObj","isPenetrateMap"})Require(bullets,id,field,0);
foreach(var id in attackIds){Require(attacks,id,"opposeTarget",1);Require(attacks,id,"friendlyTarget",0);Require(attacks,id,"selfTarget",0);}
foreach(var id in new[]{98000000,98000200,98000400,98000500,98000600})Require(bullets,id,"lockShootLimitAng",0);
Require(bullets,98000300,"HitBulletID",98000301);Require(bullets,98000301,"HitBulletID",-1);
foreach(var id in new[]{98000000,98000200,98000101,98000300,98000301})Require(bullets,id,"atkId_Bullet",id);
// Independent explosion hit list: zero-power flight contact must not exclude
// the same target from its damaging child. Single child + isEndlessHit=0
// retains one hit per target; only isolated custom launcher children change.
foreach(var id in new[]{98000301,98000701}){
 var row=bullets.Rows.Single(r=>r.ID==id);Set(row,"isUseSharedHitList",0);
 // Four frames at20fps rather than a1–2frame .07s sphere. The native2s hit
 // record outlives this entire stationary blast; repeat-hit remains disabled.
 Set(row,"life",.20f);Require(bullets,id,"isEndlessHit",0);
 if(Convert.ToSingle(row["dmgHitRecordLifeTime"].Value)<.20f)throw new Exception("Blast hit record shorter than lifespan");
 Require(bullets,id,"HitBulletID",-1);
}
// Player-owned weapon bullets otherwise use equipped-weapon correction and ignore
// the NPC-only atkPhys/atkFire base fields. Enable additive bases explicitly.
foreach(var id in attackIds){var row=attacks.Rows.Single(r=>r.ID==id);Set(row,"isAddBaseAtk",1);foreach(var field in new[]{"atkPhysCorrection","atkMagCorrection","atkFireCorrection","atkThunCorrection","atkDarkCorrection"})Set(row,field,0);}
// Render imported original GTA flight meshes and child-position explosion FX.
// Only isolated custom rows suppress ER spell visuals; baseline rows stay untouched.
foreach(var id in bulletIds){var row=bullets.Rows.Single(r=>r.ID==id);Set(row,"sfxId_Bullet",-1);
 if(id>=98000100&&id<=98000103||id>=98000300&&id<=98000301||id>=98000700&&id<=98000701)foreach(var field in new[]{"sfxId_Hit","sfxId_Flick","sfxId_ForceErase"})Set(row,field,-1);
}
// V4.1 native blast keeps spherical Bullet geometry but uses the exact working
// custom RuinFragment physical attack template, at unchanged total650power.
// No arrow/bow attack context is used; direct HP writes remain forbidden.
foreach(var id in new[]{98000301,98000701}){
 var row=attacks.Rows.Single(r=>r.ID==id);var source=attacks.Rows.Single(r=>r.ID==98000000);
 foreach(var c in source.Cells)row[c.Def.InternalName].Value=c.Value is byte[] bytes?(byte[])bytes.Clone():c.Value;
 Set(row,"atkPhys",650);Set(row,"atkFire",0);attackSources[id]=98000000;
}
// Native sword equip uses the shipped ID so every animation/skill reference stays
// intact. Only its five stat requirements change; player stats are never written.
var equipment=Load("EquipParamWeapon");
var sword=equipment.Rows.Single(r=>r.ID==2140000);
var swordRequirements=new[]{"properStrength","properAgility","properMagic","properFaith","properLuck"};
foreach(var field in swordRequirements)Set(sword,field,0);
Store("Bullet",bullets);Store("AtkParam_Pc",attacks);Store("EquipParamWeapon",equipment);
Directory.CreateDirectory(Path.GetDirectoryName(output)!);SFUtil.EncryptERRegulation(output,b);
var result=SFUtil.DecryptERRegulation(output);
var original=SFUtil.DecryptERRegulation(input);
foreach(var (table,expected) in new[]{("Bullet",bullets),("AtkParam_Pc",attacks),("EquipParamWeapon",equipment)}){
 var actual=PARAM.Read(result.Files.Single(f=>f.Name.EndsWith("\\"+table+".param")).Bytes);actual.ApplyParamdef(expected.AppliedParamdef);
 var rows=actual.Rows.ToDictionary(r=>r.ID);
 foreach(var row in expected.Rows)CompareRow(row,rows[row.ID],"Roundtrip");
 var before=PARAM.Read(original.Files.Single(f=>f.Name.EndsWith("\\"+table+".param")).Bytes);before.ApplyParamdef(expected.AppliedParamdef);
 foreach(var row in before.Rows){
  if(table=="EquipParamWeapon"&&row.ID==2140000)foreach(var field in swordRequirements)Set(row,field,0);
  CompareRow(row,rows[row.ID],"Changed original");
 }
 if(table=="EquipParamWeapon")continue;
 var sources=table=="Bullet"?bulletSources:attackSources;
 foreach(var (id,source) in sources){
  var row=rows[id];var nativeTemplate=source==98000000?sources[source]:source;
  reports.Add(new {table,row=id,source,native_template=nativeTemplate,values=row.Cells.Where(c=>c.Value is not byte[]).ToDictionary(c=>c.Def.InternalName,c=>c.Value)});
 }
}
// Every unrelated binder entry stays byte identical. Equipment is compared cell
// by cell above, with exactly five permitted fields in exactly one original row.
foreach(var f in original.Files){if(f.Name.EndsWith("\\Bullet.param")||f.Name.EndsWith("\\AtkParam_Pc.param")||f.Name.EndsWith("\\EquipParamWeapon.param"))continue;if(!f.Bytes.SequenceEqual(result.Files.Single(r=>r.Name==f.Name).Bytes))throw new Exception("Unrelated param changed: "+f.Name);}
File.WriteAllText(Path.Combine(Path.GetDirectoryName(output)!,"combat-param-report.json"),JsonSerializer.Serialize(new {
 author="igobygod",runtime_verified=false,actual_enemy_hit_verified=false,original_combat_rows_unchanged=true,unrelated_tables_byte_identical=true,
 sword=new{id=2140000,name="Sword of Night and Flame",requirement_fields=swordRequirements,requirement_value=0,other_equipment_fields_unchanged=true,player_stats_unchanged=true},
 rifle=new{bullet=98000000,attack=98000000,base_power=140,visual="Native ER bolt",ballistics="2000 m/s, 0.06 s life, no gravity; native collision"},
 pistol=new{bullet=98000200,attack=98000200,base_power=100,damage_source="Assets/GTA-GunData/update_update.rpf/update_update.rpf_common_data_ai_weapons.meta/weapons.meta: WEAPON_PISTOL/Damage",visual="Native ER bolt",ballistics="Inherits custom rifle collision/ballistics; not a complete GTA pistol simulation"},
 grenade=new{bullets=new[]{98000100,98000101,98000102,98000103},attacks=new[]{98000100,98000101},adapter="Bounded 4s game-thread grenade flight; native stationary explosion/debris/spark with balanced500fire additive power"},
 rpg=new{bullets=new[]{98000700,98000701},attacks=new[]{98000700,98000701},speed_mps=400,life_seconds=5,gravity_mps2=0.1962,child_physical_power=650,source="Original AMMO_RPG; native ER collision adapter"},
 launcher=new{bullets=new[]{98000300,98000301},attacks=new[]{98000300,98000301},adapter="Original GTA25m/s,9.81m/s2 low arc; native collision and650physical additive explosion power"},
 muzzle_flash_included=false,native_flight_spell_sfx_suppressed=true,native_explosive_spell_sfx_suppressed=true,imported_explosion_rendering="Renderer uses original GTA FX at confirmed native child entity positions",detonation_audio_verified=false,rows=reports
},new JsonSerializerOptions{WriteIndented=true}));
Console.WriteLine("Verified local combat rows and sword requirements; original combat rows, unrelated equipment fields and tables preserved.");

// Generate weapon-socket metadata from this user's regulation, never ship it.
var absorptionId=Convert.ToInt32(sword["absorpParamId"].Value);
var absorptionBytes=original.Files.Single(f=>f.Name.EndsWith("\\WepAbsorpPosParam.param")).Bytes;
var absorptionParam=PARAM.Read(absorptionBytes);var absorptionRow=absorptionParam.Rows.Single(r=>r.ID==absorptionId);
var offsetField=typeof(PARAM.Row).GetField("DataOffset",System.Reflection.BindingFlags.NonPublic|System.Reflection.BindingFlags.Instance)!;
var absorptionOffset=checked((int)(long)offsetField.GetValue(absorptionRow)!);
if(absorptionId!=23||absorptionParam.ParamdefDataVersion!=2)throw new Exception("Unsupported sword socket schema");
var absorptionRaw=absorptionBytes.AsSpan(absorptionOffset,96).ToArray();
if(BitConverter.ToInt16(absorptionRaw,8)!=20)throw new Exception("Unexpected sword hand socket");
File.WriteAllText(Path.Combine(Path.GetDirectoryName(output)!,"sword-absorption.json"),JsonSerializer.Serialize(new{weapon=2140000,absorption=absorptionId,paramVersion=absorptionParam.ParamdefDataVersion,offset=absorptionOffset,size=96,raw=Convert.ToHexString(absorptionRaw)},new JsonSerializerOptions{WriteIndented=true}));

// Optional installed-build equivalence check compares decompressed binder entries.
// Comparing the encrypted files is insufficient: encryption may use different IVs.
bool? installedMatch=null;
if(comparison!=null){
 var reference=SFUtil.DecryptERRegulation(comparison);
 if(reference.Files.Count!=result.Files.Count)throw new Exception("Reference binder file count differs");
 foreach(var entry in reference.Files){
  var actual=result.Files.SingleOrDefault(f=>f.Name==entry.Name);
  if(actual==null||actual.ID!=entry.ID||!actual.Bytes.SequenceEqual(entry.Bytes))throw new Exception("Reference binder mismatch: "+Path.GetFileName(entry.Name.Replace('\\','/')));
 }
 installedMatch=true;
}
uint Fnv(byte[] bytes){uint h=2166136261;foreach(byte x in bytes)h=unchecked((h^x)*16777619);return h;}
var fingerprints=new List<string>();
foreach(var table in new[]{"AtkParam_Pc","Bullet"}){
 var currentBytes=result.Files.Single(f=>f.Name.EndsWith("\\"+table+".param")).Bytes;
 var currentParam=PARAM.Read(currentBytes);
 var def=table=="Bullet"?bullets.AppliedParamdef:attacks.AppliedParamdef;int size=def.GetRowSize();
 var dataOffset=typeof(PARAM.Row).GetField("DataOffset",System.Reflection.BindingFlags.NonPublic|System.Reflection.BindingFlags.Instance)!;
 foreach(var id in table=="Bullet"?bulletIds:attackIds){var row=currentParam.Rows.Single(r=>r.ID==id);
  int off=checked((int)(long)dataOffset.GetValue(row)!);
  fingerprints.Add("{"+(table=="Bullet"?10:8)+","+id+","+size+",0x"+Fnv(currentBytes.AsSpan(off,size).ToArray()).ToString("x8")+"u,0x00000000u}");
 }
}
Directory.CreateDirectory(Path.GetDirectoryName(header)!);
File.WriteAllText(header,"#pragma once\nstruct ExpectedParam {unsigned index;int id;unsigned size,repair_hash,shipped_hash;};\nstatic const ExpectedParam expected_params[]={"+string.Join(",",fingerprints)+"};\n");
File.WriteAllText(Path.Combine(Path.GetDirectoryName(output)!,"verification.json"),JsonSerializer.Serialize(new{author="igobygod",original_input_unchanged=true,roundtrip_fields_verified=true,unrelated_tables_byte_identical=true,installed_entries_identical=installedMatch,custom_bullets=bulletIds.Length,custom_attacks=attackIds.Length,gameplay_verified=false},new JsonSerializerOptions{WriteIndented=true}));
Console.WriteLine(installedMatch==true?"All decompressed entries match the installed regulation.":"Runtime row fingerprints generated; live gameplay not tested by this tool.");
