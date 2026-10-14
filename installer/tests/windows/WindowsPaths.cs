using System;
using System.IO;
using System.IO.Compression;
using Igobygod;
class WindowsPaths {
 static int Main(string[] args) {
  if(Environment.OSVersion.Platform!=PlatformID.Win32NT)throw new Exception("Run this regression on Windows .NET Framework.");
  Core.InitializeFileIO();
  if(Core.ExtendedIOPath(@"\\server\share\file")!=@"\\?\UNC\server\share\file")throw new Exception("UNC extended path conversion");
  if(Core.ExtendedIOPath(@"\\?\C:\file")!=@"\\?\C:\file")throw new Exception("Extended prefix was added twice");
  var root=Path.Combine(Path.GetTempPath(),"igobygod-path-test-"+Guid.NewGuid().ToString("N"));
  Directory.CreateDirectory(root);
  try {
   var zip=Path.Combine(root,"fixture.zip");
   var relative="sdk/8.0.425/DotnetTools/dotnet-watch/8.0.425-servicing.26421.11/tools/net8.0/any/BuildHost-net472/Microsoft.CodeAnalysis.Workspaces.MSBuild.BuildHost.exe";
   using(var z=ZipFile.Open(zip,ZipArchiveMode.Create))using(var w=new StreamWriter(z.CreateEntry(relative).Open()))w.Write("asset-free regression fixture");
   // Final and staging paths both exceed MAX_PATH, just as the SDK staging path did.
   var output=Path.Combine(root,new string('p',75),"dotnet");
   Directory.CreateDirectory(Path.GetDirectoryName(output));
   Console.WriteLine("Final test path length: "+Path.Combine(output,relative).Length);
   if(args.Length==2&&args[0]=="--sdk"){
    Core.Extract(args[1],output);
    if(!File.Exists(Core.ExtendedIOPath(Path.Combine(output,"dotnet.exe")))||!File.Exists(Core.ExtendedIOPath(Path.Combine(output,relative))))throw new Exception("SDK extraction incomplete");
    Console.WriteLine("PASS: full .NET SDK archive extracted at the long-path regression destination.");return 0;
   }
   Core.Extract(zip,output);
   var expected="\\\\?\\"+Path.Combine(output,relative).Replace('/','\\');
   if(File.ReadAllText(expected)!="asset-free regression fixture")throw new Exception("Long-path file contents differ");
   var bad=Path.Combine(root,"traversal.zip");using(var z=ZipFile.Open(bad,ZipArchiveMode.Create))using(var w=new StreamWriter(z.CreateEntry("../escape.txt").Open()))w.Write("bad");
   bool rejected=false;try{Core.Extract(bad,Path.Combine(root,"bad"));}catch(InvalidDataException){rejected=true;}
   if(!rejected||File.Exists(Path.Combine(root,"escape.txt")))throw new Exception("Traversal protection changed");
   Console.WriteLine("PASS: .NET Framework x64 extraction beyond MAX_PATH and traversal rejection.");return 0;
  }catch(Exception e){Console.WriteLine(e.ToString());return 1;}
  finally{try{Directory.Delete("\\\\?\\"+root,true);}catch(Exception e){Console.WriteLine("Fixture cleanup: "+e.Message);}}
 }
}
