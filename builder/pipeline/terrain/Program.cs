// igobygod: reads locally extracted original map data; no game payload is bundled.
using SoulsFormats;
using System.Text;
using System.Text.Json;
using System.Xml.Linq;
Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);
if(args.Length!=3)throw new ArgumentException("TerrainAssets MODE WORKSPACE OUTPUT");
var mode=args[0];var root=Path.GetFullPath(args[1]);var output=Path.GetFullPath(args[2]);
Directory.CreateDirectory(output);
if(mode=="select") {
 var binder=BND4.Read(Path.Combine(output,"masks.raw"));
 var member=binder.Files.Single(f=>f.Name.Replace('\\','/').EndsWith("/MENU_MapTile_M00.mtmsk",StringComparison.OrdinalIgnoreCase));
 var masks=XDocument.Parse(Encoding.UTF8.GetString(member.Bytes)).Root!.Elements().ToDictionary(x=>int.Parse(x.Attribute("id")!.Value),x=>uint.Parse(x.Attribute("mask")!.Value));
 var source=Path.Combine(root,"Assets/ER/menu");
 using var tiles=new BXF4Reader(Path.Combine(source,"71_maptile.tpfbhd"),Path.Combine(source,"71_maptile.tpfbdt"));
 var compressed=Path.Combine(output,"compressed");Directory.CreateDirectory(compressed);var report=new List<object>();
 for(int x=7;x<=18;x++)for(int y=7;y<=18;y++) {
  uint mask=masks.GetValueOrDefault(x*100+y);var name=$"MENU_MapTile_M00_L0_{x:00}_{y:00}_{mask:x8}.tpf.dcx";
  var file=tiles.Files.SingleOrDefault(f=>f.Name.EndsWith(name,StringComparison.OrdinalIgnoreCase))??throw new Exception("Missing exact map mask: "+name);
  File.WriteAllBytes(Path.Combine(compressed,name),tiles.ReadFile(file));report.Add(new{x,y,mask,source=file.Name,file=name});
 }
 File.WriteAllText(Path.Combine(output,"tiles.json"),JsonSerializer.Serialize(report,new JsonSerializerOptions{WriteIndented=true}));
 Console.WriteLine("Selected all 144 exact-mask Limgrave tiles.");
} else if(mode=="headerize") {
 var dds=Path.Combine(output,"dds");Directory.CreateDirectory(dds);int count=0;
 foreach(var file in Directory.GetFiles(Path.Combine(output,"compressed"),"*.raw").Order()) {
  var textures=TPF.Read(file).Textures;if(textures.Count!=1)throw new Exception("Unexpected map tile texture count");
  var texture=textures[0];File.WriteAllBytes(Path.Combine(dds,texture.Name+".dds"),texture.Headerize());count++;
 }
 if(count!=144)throw new Exception("Missing decoded map tiles");Console.WriteLine("Headerized 144 original map textures.");
} else throw new ArgumentException("Unknown mode");
