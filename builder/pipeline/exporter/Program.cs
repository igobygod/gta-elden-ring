// igobygod: local asset extraction. Outputs stay on the game owner's computer.
using CodeWalker.GameFiles;
using System.Text.Json;
using System.Security.Cryptography;

string Arg(string name) { var i=Array.IndexOf(args,name); if(i<0 || i+1>=args.Length) throw new ArgumentException("Missing "+name); return args[i+1]; }
var gta=Path.GetFullPath(Arg("--gta")); var output=Path.GetFullPath(Arg("--output"));
if(!File.Exists(Path.Combine(gta,"GTA5.exe")))throw new ArgumentException("Select a GTA V Legacy directory containing GTA5.exe.");
Directory.CreateDirectory(output);
GTA5Keys.GenerateV2(File.ReadAllBytes(Path.Combine(gta,"GTA5.exe")),null);
GTA5Keys.LoadFromPath(gta,false,Convert.ToBase64String(GTA5Keys.PC_AES_KEY));
var records=new List<object>();
var weaponPrefixes=new[]{"w_pi_pistol","w_sb_smg","w_sg_pumpshotgun","w_ar_assaultrifle","w_sr_sniperrifle","w_lr_grenadelauncher","w_ex_grenade","w_lr_rpg","w_lr_40mm"};
var bodyPrefixes=new[]{"head_000","hair_000","uppr_000","lowr_000","feet_000","hand_000","teef_000","head_diff_000_a","hair_diff_000_a","uppr_diff_000_a","lowr_diff_000_a","feet_diff_000_a","hand_diff_000_a","teef_diff_000_a"};
var motionPrefixes=new[]{"move_p_m_one","move_weapon@rifle","move_weapon@pistol","move_strafe@generic","move_strafe@grenade","weapons@pistol@","weapons@rifle@","weapons@projectile@","weapons@heavy@","veh@std@ds@base","veh@std@ds@enter_exit"};
bool extrasOnly=args.Contains("--extras-only");
foreach(var archiveName in new[]{"x64v.rpf","x64e.rpf","x64c.rpf","x64b.rpf","x64d.rpf","x64/audio/sfx/WEAPONS_PLAYER.rpf","x64/audio/sfx/RESIDENT.rpf","x64/audio/sfx/SCRIPT.rpf","x64/audio/audio_rel.rpf"}) {
  var archivePath=Path.Combine(gta,archiveName); if(!File.Exists(archivePath))throw new FileNotFoundException("Required GTA archive missing",archiveName);
  var root=new RpfFile(archivePath,archiveName);root.ScanStructure(_=>{},s=>Console.Error.WriteLine(s));
  void Walk(RpfFile r) {
    foreach(var entry in r.AllEntries??new()) {
      if(entry is not RpfFileEntry e)continue;
      var n=e.NameLower; var p=e.Path.ToLowerInvariant(); string category=null;
      bool body=p.Contains("player_one")&&(n=="player_one.yft"||bodyPrefixes.Any(n.StartsWith));
      bool weapon=weaponPrefixes.Any(n.StartsWith);
      if((body||weapon)&&new[]{".ydd",".ydr",".yft",".ytd"}.Any(n.EndsWith))category="Assets/GTA-Franklin";
      if(n.EndsWith(".ycd")&&motionPrefixes.Any(n.StartsWith))category="Assets/GTA-Movement";
      if(n=="core.ypt")category="Build/gta-fx-source";
      if(extrasOnly && !(n.StartsWith("veh@std@ds@") || n.StartsWith("move_weapon@pistol")))category=null;
      if(new[]{"buffalo2.yft","buffalo2.ytd","vehshare.ytd","taxi.ytd","taxi+hi.ytd","vehicles_poltax_interior.ytd"}.Contains(n))category="PlayerRenderer/car-proof/stage/donor";
      if(new[]{"hud.gfx","hud.ytd","minimap.gfx","minimap.ytd"}.Contains(n))category="PlayerRenderer/polish-next/hud/donor";
      if(n=="font_lib_efigs_pc.gfx")category="PlayerRenderer/polish-next/death-ui/donor";
      if(n.EndsWith(".awc") && (archiveName.Contains("WEAPONS_PLAYER") || archiveName.Contains("RESIDENT") && new[]{"explosions.awc","weapons.awc","frontend.awc"}.Contains(n)))category="Build/gta-fx-source";
      if(archiveName.EndsWith("audio_rel.rpf") && n.EndsWith(".rel"))category="Build/gta-fx-source";
      if(archiveName.EndsWith("SCRIPT.rpf") && n=="offmission_wasted.awc")category="PlayerRenderer/polish-next/death-ui/donor";
      if(category==null)continue;
      var dir=category.EndsWith("car-proof/stage/donor") ? Path.Combine(output,category,n) : Path.Combine(output,category,archiveName.Replace('/','_'),n);Directory.CreateDirectory(dir);
      if(n.EndsWith(".gfx")) {
        var raw=r.ExtractFile(e)??throw new Exception("Could not extract "+n);
        var filename=Path.Combine(dir,n); File.WriteAllBytes(filename,raw);
        records.Add(new {archive=archiveName,source=e.Path,output=Path.GetRelativePath(output,filename),sha256=Convert.ToHexString(SHA256.HashData(raw)).ToLowerInvariant()});
        Console.WriteLine("Exported "+n);continue;
      }
      string xml=n.Split('.').Last() switch {
        "ydd"=>YddXml.GetXml(RpfFile.GetFile<YddFile>(e),dir),
        "ydr"=>YdrXml.GetXml(RpfFile.GetFile<YdrFile>(e),dir),
        "yft"=>YftXml.GetXml(RpfFile.GetFile<YftFile>(e),dir),
        "ytd"=>YtdXml.GetXml(RpfFile.GetFile<YtdFile>(e),dir),
        "ycd"=>YcdXml.GetXml(RpfFile.GetFile<YcdFile>(e)),
        "ypt"=>YptXml.GetXml(RpfFile.GetFile<YptFile>(e),dir),
        "awc"=>AwcXml.GetXml(RpfFile.GetFile<AwcFile>(e),dir),
        "rel"=>RelXml.GetXml(RpfFile.GetFile<RelFile>(e)),
        _=>throw new Exception("Unsupported selected file")};
      var dest=Path.Combine(dir,n+".xml");File.WriteAllText(dest,xml);
      records.Add(new {archive=archiveName,source=e.Path,output=Path.GetRelativePath(output,dest),sha256=Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(dest))).ToLowerInvariant()});
      Console.WriteLine("Exported "+n);
    }
    foreach(var c in r.Children??new())Walk(c);
  }
  Walk(root);
}
File.WriteAllText(Path.Combine(output,extrasOnly?"local-extraction-extras.json":"local-extraction.json"),JsonSerializer.Serialize(records,new JsonSerializerOptions{WriteIndented=true}));
