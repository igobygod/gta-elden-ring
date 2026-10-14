using System;
using System.IO;
using System.IO.Compression;
using System.Text;
using System.Linq;
using System.Collections.Generic;
using System.Diagnostics;
using System.Net.Http;
using System.Security.Cryptography;
using System.Runtime.Serialization.Json;
namespace Igobygod {
 public class Artifact { public string id; public string url; public string hash; public string algorithm; public string file; }
 public class LaunchSettings { public string python; public string install; }
 public static class Core {
  public static string Quote(string s) {
   var b=new StringBuilder("\"");int slashes=0;
   foreach(char c in s){if(c=='\\'){slashes++;continue;} if(c=='"'){b.Append('\\',slashes*2+1);b.Append(c);}else{b.Append('\\',slashes);b.Append(c);}slashes=0;}
   b.Append('\\',slashes*2);return b.Append('"').ToString();
  }
  public static bool IsUsableFolder(string path,string marker,string value){if(File.Exists(path))return false;if(!Directory.Exists(path))return true;if(!Directory.EnumerateFileSystemEntries(path).Any())return true;var m=Path.Combine(path,marker);return File.Exists(m)&&File.ReadAllText(m)==value;}
  public static string Canon(string path){return Path.GetFullPath(path).TrimEnd(Path.DirectorySeparatorChar,Path.AltDirectorySeparatorChar);}
  public static bool Within(string path,string parent){return Canon(path).Equals(Canon(parent),StringComparison.OrdinalIgnoreCase)||Canon(path).StartsWith(Canon(parent)+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase);}
  public static void CheckDestination(string output,params string[] protectedPaths){
   if(string.IsNullOrWhiteSpace(output))throw new InvalidOperationException("Choose an installation folder.");
   foreach(var p in protectedPaths)if(Within(output,p)||Within(p,output))throw new InvalidOperationException("Choose a separate installation folder outside both games.");
   if(File.Exists(output)||Directory.Exists(output))throw new InvalidOperationException("That folder already exists. Choose a new folder.");
   CheckLinks(output);
  }
  public static void CheckLinks(string path){for(var p=new DirectoryInfo(Path.GetFullPath(path));p!=null;p=p.Parent)if(p.Exists&&(p.Attributes&FileAttributes.ReparsePoint)!=0)throw new InvalidOperationException("Choose an installation folder without linked folders in its path.");}
  public static string Hash(string path,string algorithm="sha256") {using(var h=algorithm=="sha512"?(HashAlgorithm)SHA512.Create():SHA256.Create())using(var s=File.OpenRead(path))return BitConverter.ToString(h.ComputeHash(s)).Replace("-","").ToLowerInvariant();}
  public static void Extract(string zip,string output) {
   if(Directory.Exists(output)||File.Exists(output))throw new IOException("Extraction destination already exists.");
   using(var z=ZipFile.OpenRead(zip)){
    foreach(var e in z.Entries){var name=e.FullName.Replace('\\','/');if(name.StartsWith("/")||name.Contains(":")||name.Split('/').Contains("..")||((e.ExternalAttributes>>16)&0xF000)==0xA000||!Within(Path.Combine(output,name),output))throw new InvalidDataException("Unsafe path in downloaded archive.");}
    var temp=output+".partial-"+Guid.NewGuid().ToString("N");Directory.CreateDirectory(temp);
    try{z.ExtractToDirectory(temp);Directory.Move(temp,output);}finally{if(Directory.Exists(temp))Directory.Delete(temp,true);}
   }
  }
  public static T ReadJson<T>(string path){using(var s=File.OpenRead(path))return (T)new DataContractJsonSerializer(typeof(T)).ReadObject(s);}
  public static void WriteJson<T>(string path,T value){using(var s=File.Create(path))new DataContractJsonSerializer(typeof(T)).WriteObject(s,value);}
  public static string Download(Artifact item,string cache,Action<string> progress){
   Directory.CreateDirectory(cache);var dest=Path.Combine(cache,item.file);
   if(File.Exists(dest)&&Hash(dest,item.algorithm)==item.hash)return dest;
   var partial=dest+".partial";
   try{
    using(var client=new HttpClient()){client.Timeout=TimeSpan.FromMinutes(30);client.DefaultRequestHeaders.UserAgent.ParseAdd("igobygod-setup/0.2");
     using(var response=client.GetAsync(item.url,HttpCompletionOption.ResponseHeadersRead).GetAwaiter().GetResult()){
      response.EnsureSuccessStatusCode();if(response.RequestMessage.RequestUri.Scheme!="https")throw new IOException("Download redirected to an insecure connection.");
      using(var source=response.Content.ReadAsStreamAsync().GetAwaiter().GetResult())using(var target=File.Create(partial))source.CopyTo(target);
     }
    }
    if(Hash(partial,item.algorithm)!=item.hash)throw new InvalidDataException("Download verification failed for "+item.id+". Run setup again to retry.");
    if(File.Exists(dest))File.Delete(dest);File.Move(partial,dest);return dest;
   }finally{if(File.Exists(partial))File.Delete(partial);}
  }
  public static string Run(string exe,IEnumerable<string> args,Dictionary<string,string> env,string log,bool visible=false,params int[] accepted){
   var info=new ProcessStartInfo(exe,string.Join(" ",args.Select(Quote))){UseShellExecute=false,CreateNoWindow=!visible,RedirectStandardOutput=true,RedirectStandardError=true};
   foreach(var kv in env)info.EnvironmentVariables[kv.Key]=kv.Value;
   var output=new StringBuilder();object gate=new object();
   using(var writer=new StreamWriter(log,true)){using(var p=new Process()){p.StartInfo=info;
    DataReceivedEventHandler handler=(s,e)=>{if(e.Data!=null)lock(gate){writer.WriteLine(e.Data);writer.Flush();output.AppendLine(e.Data);}};
    p.OutputDataReceived+=handler;p.ErrorDataReceived+=handler;p.Start();p.BeginOutputReadLine();p.BeginErrorReadLine();p.WaitForExit();
    if(p.ExitCode!=0&&!accepted.Contains(p.ExitCode))throw new InvalidOperationException(Path.GetFileName(exe)+" stopped (code "+p.ExitCode+"). Open the setup log for details.");
   }}return output.ToString();
  }
 }
}
