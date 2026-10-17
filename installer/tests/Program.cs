using System.Linq;
using System;
using System.IO;
using System.IO.Compression;
using Igobygod;
class Tests {
 static int Main(string[] args) {
  if(args.Length==3&&args[0]=="--inspect"){
   using(var file=File.OpenRead(args[1]))using(var pe=new System.Reflection.PortableExecutable.PEReader(file)){
    if(pe.PEHeaders.CoffHeader.Machine!=System.Reflection.PortableExecutable.Machine.Amd64)throw new Exception("Expected x64 executable");
    var metadata=System.Reflection.Metadata.PEReaderExtensions.GetMetadataReader(pe);
    var section=pe.GetSectionData(pe.PEHeaders.CorHeader.ResourcesDirectory.RelativeVirtualAddress);
    var found=new System.Collections.Generic.HashSet<string>();
    foreach(var handle in metadata.ManifestResources){var resource=metadata.GetManifestResource(handle);var name=metadata.GetString(resource.Name);if(!resource.Implementation.IsNil)continue;var length=section.GetReader((int)resource.Offset,4).ReadInt32();var bytes=section.GetContent((int)resource.Offset+4,length).ToArray();var expected=File.ReadAllBytes(Path.Combine(args[2],name));if(!System.Linq.Enumerable.SequenceEqual(bytes,expected))throw new Exception("Embedded resource mismatch: "+name);found.Add(name);Console.WriteLine(name+": "+length+" verified bytes");}
    foreach(var name in new[]{"payload.zip","tools.json","release.json","windows_worker.py"})if(!found.Contains(name))throw new Exception("Missing embedded resource: "+name);
   }Console.WriteLine("PASS: x64 executable and all embedded resources verified without loading Windows code.");return 0;
  }
  if(args.Length>0&&args[0]=="--echo"){Console.Write(args[1]);return args.Length>2?int.Parse(args[2]):0;}
  var temp=Path.Combine(Path.GetTempPath(),"igobygod-tests-"+Guid.NewGuid());Directory.CreateDirectory(temp);
  try {
   if(Core.Quote("a b\\")!="\"a b\\\\\"")throw new Exception("trailing slash quoting");
   if(Core.Quote("a\"b")!="\"a\\\"b\"")throw new Exception("embedded quote escaping");
   var empty=Path.Combine(temp,"empty install");Directory.CreateDirectory(empty);
   if(!Core.IsUsableFolder(empty,".owner","igobygod"))throw new Exception("empty folder selected through Browse must be accepted");
   File.WriteAllText(Path.Combine(empty,"unrelated.txt"),"keep");
   if(Core.IsUsableFolder(empty,".owner","igobygod"))throw new Exception("unrelated files must be rejected");
   File.WriteAllText(Path.Combine(empty,".owner"),"igobygod");
   if(!Core.IsUsableFolder(empty,".owner","igobygod"))throw new Exception("owned retry folder must be accepted");
   var game=Path.Combine(temp,"game");Directory.CreateDirectory(game);
   bool rejected=false;try{Core.CheckDestination(Path.Combine(game,"mod"),game);}catch(InvalidOperationException){rejected=true;}
   if(!rejected)throw new Exception("game folder overlap not rejected");
   rejected=false;try{Core.CheckDestination(game,game);}catch(InvalidOperationException){rejected=true;}
   if(!rejected)throw new Exception("existing game folder not rejected");
   Core.CheckDestination(Path.Combine(temp,"safe output"),game);
   var zip=Path.Combine(temp,"evil.zip");using(var z=ZipFile.Open(zip,ZipArchiveMode.Create)){using(var w=new StreamWriter(z.CreateEntry("../escape.txt").Open()))w.Write("bad");}
   rejected=false;try{Core.Extract(zip,Path.Combine(temp,"extracted"));}catch(InvalidDataException){rejected=true;}
   if(!rejected || File.Exists(Path.Combine(temp,"escape.txt")))throw new Exception("archive traversal");
   var valid=Path.Combine(temp,"valid.zip");using(var z=ZipFile.Open(valid,ZipArchiveMode.Create)){using(var w=new StreamWriter(z.CreateEntry("nested/data.txt").Open()))w.Write("verified");}
   Core.Extract(valid,Path.Combine(temp,"valid"));if(File.ReadAllText(Path.Combine(temp,"valid/nested/data.txt"))!="verified")throw new Exception("valid archive extraction");
   var childArgs=new[]{System.Reflection.Assembly.GetExecutingAssembly().Location,"--echo","path with spaces and \"quote\""};
   var echoed=Core.Run(Environment.GetEnvironmentVariable("DOTNET_HOST_PATH") ?? "dotnet",childArgs,new System.Collections.Generic.Dictionary<string,string>(),Path.Combine(temp,"process.log"));
   if(echoed.Trim()!=childArgs[2])throw new Exception("process argument corruption");
   rejected=false;try{Core.Run(Environment.GetEnvironmentVariable("DOTNET_HOST_PATH") ?? "dotnet",new[]{childArgs[0],"--echo","failure","9"},new System.Collections.Generic.Dictionary<string,string>(),Path.Combine(temp,"process.log"));}catch(InvalidOperationException){rejected=true;}
   if(!rejected)throw new Exception("child process failure ignored");
   rejected=false;try{Core.Run(Environment.GetEnvironmentVariable("DOTNET_HOST_PATH") ?? "dotnet",new[]{childArgs[0],"--echo","Setup stopped: generated animation failed verification","9"},new System.Collections.Generic.Dictionary<string,string>(),Path.Combine(temp,"detail.log"));}catch(InvalidOperationException e){rejected=e.Message=="Setup stopped: generated animation failed verification";}
   if(!rejected)throw new Exception("actionable builder failure hidden");
   Console.WriteLine("PASS: quoted paths, embedded quotes, output containment, existing folders, empty-folder selection, owned retries, ZIP traversal, valid extraction, process arguments and failure exit codes.");return 0;
  }finally{Directory.Delete(temp,true);}
 }
}
