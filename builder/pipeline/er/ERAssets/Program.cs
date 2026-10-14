using SoulsFormats;
using System.Numerics;
using System.Security.Cryptography;
using System.Text;
Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);
if(args.Length<3||args[0]=="--help"){Console.WriteLine("ERAssets GAME_DIRECTORY OUTPUT_DIRECTORY RESOURCE_PATH [...]");return;}
var game=Path.GetFullPath(args[0]);var output=Path.GetFullPath(args[1]);
if(output.StartsWith(game+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase)||game.StartsWith(output+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase)||string.Equals(output,game,StringComparison.OrdinalIgnoreCase))throw new Exception("Output must be separate from the game directory");
var names=args.Skip(2).Select(x=>"/"+x.Replace('\\','/').TrimStart('/').ToLowerInvariant()).ToArray();
if(names.Any(x=>x.Contains(':')||x.Split('/').Contains("..")))throw new Exception("Invalid relative resource path");
if(Directory.Exists(output)&&Directory.EnumerateFileSystemEntries(output).Any())throw new Exception("Output must be a fresh or empty directory");
Directory.CreateDirectory(output);
var recovered=new HashSet<string>();
ulong Hash(string s) { ulong h=0; foreach(char c in s) h=unchecked(h*0x85+c); return h; }
var wanted=names.ToDictionary(Hash);
foreach(var name in new[]{"Data0","Data1","Data2","Data3"}) {
 var bhdPath=Path.Combine(game,name+".bhd");
 var cache=Path.Combine(output,name+".bhd");
 if(!File.Exists(cache)) {
  using var rsa=RSA.Create(); rsa.ImportFromPem(UXM.ArchiveKeys.EldenRingKeys[name]);
  var rp=rsa.ExportParameters(false); var modulus=new BigInteger(rp.Modulus!,true,true); var exponent=new BigInteger(rp.Exponent!,true,true);
  int block=rp.Modulus!.Length; var encrypted=File.ReadAllBytes(bhdPath); using var decrypted=File.Create(cache);
  for(int i=0;i<encrypted.Length;i+=block) {
   var value=BigInteger.ModPow(new BigInteger(encrypted.AsSpan(i,block),true,true),exponent,modulus).ToByteArray(true,true);
   var padded=new byte[block-1]; value.CopyTo(padded,padded.Length-value.Length); decrypted.Write(padded);
  }
 }
 using var input=File.OpenRead(cache); var bhd=BHD5.Read(input,BHD5.Game.EldenRing);
 using var data=File.OpenRead(Path.Combine(game,name+".bdt"));
 foreach(var bucket in bhd.Buckets) foreach(var entry in bucket) {
  if(!wanted.TryGetValue(entry.FileNameHash,out var path)) continue;
  var dest=Path.Combine(output,path.TrimStart('/')); Directory.CreateDirectory(Path.GetDirectoryName(dest)!);
  File.WriteAllBytes(dest,entry.ReadFile(data)); Console.WriteLine($"Extracted {name}: {path}"); recovered.Add(path);
 }
}

var missing=names.Where(x=>!recovered.Contains(x)).ToArray();
if(missing.Length>0)throw new Exception("Missing requested resources: "+string.Join(", ",missing));
Console.WriteLine("All requested files extracted from the selected installation.");
