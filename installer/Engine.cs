using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Reflection;
using System.Diagnostics;
using System.Collections.Generic;
using Microsoft.Win32;
namespace Igobygod {
 public class ReleaseInfo { public string elden_ring_sha256; }
 public class Engine {
  public const string Marker="igobygod-windows-setup-v1";
  readonly Action<string> progress;readonly Dictionary<string,string> env=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
  string root,log,host,source,tools,python;
  Artifact[] artifacts;
  public Engine(Action<string> progress){this.progress=progress;}
  public static void Resource(string name,string destination){using(var s=Assembly.GetExecutingAssembly().GetManifestResourceStream(name))using(var w=File.Create(destination)){if(s==null)throw new IOException("Setup is incomplete. Download it again.");s.CopyTo(w);}}
  string Fetch(string id){var a=artifacts.First(x=>x.id==id);return Core.Download(a,Path.Combine(root,"Downloads"),progress);}
  string Portable(string id){progress("Preparing "+id+"…");string archive=Fetch(id),target=Path.Combine(host,id);if(!Directory.Exists(target))Core.Extract(archive,target);return target;}
  string Run(string exe,params string[] args){return Core.Run(exe,args,env,log);}
  void ExternalInstaller(string exe,string args){
   var start=new ProcessStartInfo(exe,args){UseShellExecute=true,Verb="runas"};
   using(var p=Process.Start(start)){p.WaitForExit();if(p.ExitCode==3010)throw new InvalidOperationException("Microsoft needs a Windows restart. Restart, then open setup again and choose the same installation folder.");if(p.ExitCode!=0&&p.ExitCode!=1638)throw new InvalidOperationException("Microsoft setup stopped (code "+p.ExitCode+"). You can retry without losing the completed downloads.");}
  }
  string FindVisualStudio(){
   string finder=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86),"Microsoft Visual Studio","Installer","vswhere.exe");
   if(!File.Exists(finder))return null;
   var result=Run(finder,"-latest","-products","*","-requires","Microsoft.VisualStudio.Component.VC.Tools.x86.x64","Microsoft.VisualStudio.Component.VC.Llvm.Clang","-property","installationPath").Trim();
   return Directory.Exists(result)?result:null;
  }
  void Compiler(){
   var vs=FindVisualStudio();
   if(vs==null){progress("Preparing Microsoft C++ tools. Allow the Windows permission prompt to continue.");ExternalInstaller(Fetch("buildtools"),"--passive --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Llvm.Clang --includeRecommended");vs=FindVisualStudio();}
   if(vs==null)throw new InvalidOperationException("Microsoft C++ tools were not installed. Run setup again and allow the Microsoft installer to finish.");
   string clang=Path.Combine(vs,"VC","Tools","Llvm","x64","bin","clang-cl.exe");
   if(!File.Exists(clang))clang=Path.Combine(vs,"VC","Tools","Llvm","bin","clang-cl.exe");
   if(!File.Exists(clang))throw new IOException("The Microsoft Clang compiler is missing. Repair the Microsoft C++ tools and retry.");
   // The only shell operation runs the trusted vendor batch file. Game paths never enter this command.
   string bat=Path.Combine(vs,"VC","Auxiliary","Build","vcvars64.bat");
   if(bat.IndexOfAny(new[]{'"','\r','\n'})>=0)throw new IOException("Unsupported compiler path.");
   var psi=new ProcessStartInfo(Environment.GetEnvironmentVariable("COMSPEC")??"cmd.exe","/d /s /c \"\""+bat+"\" >nul && set\""){UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true};
   using(var p=Process.Start(psi)){string text=p.StandardOutput.ReadToEnd();string error=p.StandardError.ReadToEnd();p.WaitForExit();File.AppendAllText(log,error);if(p.ExitCode!=0)throw new IOException("Microsoft C++ tools could not be initialized.");foreach(var line in text.Split('\n')){int i=line.IndexOf('=');if(i>0)env[line.Substring(0,i)]=line.Substring(i+1).TrimEnd('\r');}}
   env["CC"]=clang;env["CXX"]=clang;
  }
  public void Install(string gta,string er,string destination){
   if(Environment.OSVersion.Platform!=PlatformID.Win32NT||!Environment.Is64BitOperatingSystem)throw new InvalidOperationException("This installer needs 64-bit Windows 10 or 11.");
   if(!File.Exists(Path.Combine(er,"eldenring.exe"))&&File.Exists(Path.Combine(er,"Game","eldenring.exe")))er=Path.Combine(er,"Game");
   gta=Path.GetFullPath(gta);er=Path.GetFullPath(er);root=Path.GetFullPath(destination);
   Core.CheckLinks(root);
   foreach(var game in new[]{gta,er})if(Core.Within(root,game)||Core.Within(game,root))throw new InvalidOperationException("Choose an installation folder separate from both games.");
   if(Process.GetProcessesByName("eldenring").Length>0)throw new InvalidOperationException("Save and close Elden Ring, then try again.");
   foreach(var file in new[]{"GTA5.exe","common.rpf","x64b.rpf","x64c.rpf","x64e.rpf","x64v.rpf"})if(!File.Exists(Path.Combine(gta,file)))throw new InvalidOperationException("Select the GTA V Legacy folder containing GTA5.exe and its game archives.");
   foreach(var file in new[]{"eldenring.exe","regulation.bin","Data0.bhd","Data0.bdt","Data1.bhd","Data1.bdt","Data2.bhd","Data2.bdt","Data3.bhd","Data3.bdt"})if(!File.Exists(Path.Combine(er,file)))throw new InvalidOperationException("Select the Elden Ring Game folder containing eldenring.exe.");
   using(var s=Assembly.GetExecutingAssembly().GetManifestResourceStream("release.json")){var spec=(ReleaseInfo)new System.Runtime.Serialization.Json.DataContractJsonSerializer(typeof(ReleaseInfo)).ReadObject(s);progress("Checking your game version…");if(Core.Hash(Path.Combine(er,"eldenring.exe"))!=spec.elden_ring_sha256)throw new InvalidOperationException("This Elden Ring version is not supported by this preview. Nothing was installed. Please send igobygod your game version.");}
   if(!Core.IsUsableFolder(root,".igobygod-setup",Marker))throw new InvalidOperationException("That folder contains other files. Choose a new empty folder for the mod.");
   Directory.CreateDirectory(root);File.WriteAllText(Path.Combine(root,".igobygod-setup"),Marker);
   log=Path.Combine(root,"setup.log");host=Path.Combine(root,"RuntimeTools");tools=Path.Combine(root,"BuildTools");Directory.CreateDirectory(host);
   if(File.Exists(Path.Combine(root,"Mod","install.local.json"))){python=Path.Combine(host,"python","tools","python.exe");if(!File.Exists(python))throw new IOException("The installed tool folder is incomplete. Choose a new installation folder.");Finish();return;}
   var drive=new DriveInfo(Path.GetPathRoot(root));if(drive.AvailableFreeSpace<35L*1024*1024*1024)throw new IOException("Choose a drive with at least 35 GB of free space for the first installation.");
   Resource("tools.json",Path.Combine(root,"tools.json"));artifacts=Core.ReadJson<Artifact[]>(Path.Combine(root,"tools.json"));
   var sourceZip=Path.Combine(root,"source.zip");Resource("payload.zip",sourceZip);source=Path.Combine(root,"Builder-"+Core.Hash(sourceZip).Substring(0,12));if(!Directory.Exists(source))Core.Extract(sourceZip,source);
   source=Directory.GetDirectories(source).Single();
   Compiler(); // Capture MSVC environment first; add private tools afterwards.
   var py=Portable("python");python=Path.Combine(py,"tools","python.exe");
   var git=Portable("git");var dotnet=Portable("dotnet");var java=Directory.GetDirectories(Portable("java")).Single();
   env["PATH"]=string.Join(";",new[]{Path.GetDirectoryName(python),Path.Combine(git,"cmd"),dotnet,Path.Combine(java,"bin"),Path.Combine(host,"cargo","bin"),env.ContainsKey("PATH")?env["PATH"]:Environment.GetEnvironmentVariable("PATH")});
   env["DOTNET_ROOT"]=dotnet;env["DOTNET_CLI_TELEMETRY_OPTOUT"]="1";env["DOTNET_SKIP_FIRST_TIME_EXPERIENCE"]="1";
   env["CARGO_HOME"]=Path.Combine(host,"cargo");env["RUSTUP_HOME"]=Path.Combine(host,"rustup");env["PYTHONUTF8"]="1";env["PYTHONDONTWRITEBYTECODE"]="1";
   progress("Preparing the mod compiler…");
   var rustup=Path.Combine(host,"cargo","bin","rustup.exe");
   if(!File.Exists(rustup))Run(Fetch("rust"),"-y","--no-modify-path","--profile","minimal","--default-host","x86_64-pc-windows-msvc","--default-toolchain","1.99.0");
   else Run(rustup,"toolchain","install","1.99.0","--profile","minimal");
   env["RUSTUP_TOOLCHAIN"]="1.99.0";
   env["GIT_CONFIG_COUNT"]="1";env["GIT_CONFIG_KEY_0"]="core.longpaths";env["GIT_CONFIG_VALUE_0"]="true";
   progress("Preparing Microsoft game components…");ExternalInstaller(Fetch("vcredist"),"/install /passive /norestart");
   var worker=Path.Combine(source,"windows_worker.py");
   progress("Preparing the conversion tools…");Run(python,worker,Path.Combine(source,"bootstrap.py"),"--output",tools);
   progress("Building Franklin, weapons, animations and the mod. This can take a while…");
   Run(python,worker,Path.Combine(source,"setup.py"),"--build","--gta",gta,"--elden-ring",er,"--tool-root",tools,"--output",Path.Combine(root,"Mod"),"--java",Path.Combine(java,"bin","java.exe"));
   if(!File.Exists(Path.Combine(root,"Mod","install.local.json")))throw new IOException("The mod did not finish installing. See the setup log.");
   Finish();
  }
  void Finish(){
   progress("Creating your Play Mod shortcut…");
   string manager=Path.Combine(root,"igobygod.exe");if(!string.Equals(Assembly.GetExecutingAssembly().Location,manager,StringComparison.OrdinalIgnoreCase))File.Copy(Assembly.GetExecutingAssembly().Location,manager,true);
   Core.WriteJson(Path.Combine(root,"launch.json"),new LaunchSettings{python=python,install=Path.Combine(root,"Mod")});
   Resource("windows_worker.py",Path.Combine(root,"windows_worker.py"));
   Shortcut(manager,root);
   progress("Installed. Your Play igobygod Mod shortcut is ready.");
  }
  public static void Shortcut(string manager,string root){
   var type=Type.GetTypeFromProgID("WScript.Shell");dynamic shell=Activator.CreateInstance(type);
   dynamic link=shell.CreateShortcut(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory),"Play igobygod Mod.lnk"));
   link.TargetPath=manager;link.Arguments="--play "+Core.Quote(root);link.WorkingDirectory=root;link.Description="Play the igobygod GTA × Elden Ring mod offline";link.Save();
   System.Runtime.InteropServices.Marshal.FinalReleaseComObject(link);System.Runtime.InteropServices.Marshal.FinalReleaseComObject(shell);
  }
  public static void Play(string root){var config=Core.ReadJson<LaunchSettings>(Path.Combine(root,"launch.json"));Core.Run(config.python,new[]{Path.Combine(root,"windows_worker.py"),Path.Combine(config.install,"setup.py"),"--launch",config.install},new Dictionary<string,string>{{"PYTHONUTF8","1"}},Path.Combine(root,"launch.log"));}
 }
}
