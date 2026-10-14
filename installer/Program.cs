using System;
using System.IO;
using System.Drawing;
using System.Windows.Forms;
using System.Diagnostics;
using System.Threading.Tasks;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using Microsoft.Win32;
namespace Igobygod {
 static class Program {
  [STAThread] static void Main(string[] args){
   System.Net.ServicePointManager.SecurityProtocol=System.Net.SecurityProtocolType.Tls12;
   Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
   try{if(args.Length==2&&args[0]=="--play"){Engine.Play(args[1]);return;}Application.Run(new SetupForm());}
   catch(Exception e){MessageBox.Show(e.Message,"igobygod",MessageBoxButtons.OK,MessageBoxIcon.Error);}
  }
 }
 class SetupForm:Form {
  readonly TextBox gta=new TextBox(),er=new TextBox(),folder=new TextBox();
  readonly Button install=new Button(),play=new Button(),logs=new Button();
  readonly Label status=new Label();readonly ProgressBar progress=new ProgressBar();readonly CheckBox consent=new CheckBox();bool busy;volatile bool stop;
  public SetupForm(){
   Text="igobygod · GTA × Elden Ring";ClientSize=new Size(760,680);MinimumSize=new Size(776,719);StartPosition=FormStartPosition.CenterScreen;BackColor=Color.FromArgb(20,23,29);ForeColor=Color.White;Font=new Font("Segoe UI",10);AutoScaleMode=AutoScaleMode.Dpi;
   var title=new Label{Text="igobygod",Font=new Font("Segoe UI",28,FontStyle.Bold),Bounds=new Rectangle(30,20,690,60),ForeColor=Color.FromArgb(170,240,110)};Controls.Add(title);
   AddLabel("GTA × ELDEN RING",30,82,690,28,12);
   AddLabel("Choose your games. Install. Play.",30,116,690,28,15);
   AddLabel("Windows preview · Native Windows gameplay testing is still pending.",30,152,690,30,10);
   Row("GTA V Legacy folder",gta,201);Row("Elden Ring Game folder",er,277);Row("Install the mod here",folder,353);
   folder.Text=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"igobygod","GTA-EldenRing");
   consent.Text="Prepare the required tools automatically. Microsoft setup may ask for administrator permission. Internet and 35 GB free space are required.";consent.SetBounds(30,432,690,52);consent.Checked=false;Controls.Add(consent);
   var notice=new LinkLabel{Text="Read setup details and dependency licenses",Bounds=new Rectangle(30,486,690,24),LinkColor=Color.FromArgb(170,240,110)};notice.LinkClicked+=(s,e)=>ShowDetails();Controls.Add(notice);
   status.Text="Your game folders will be checked before anything is installed.";status.SetBounds(30,522,690,42);Controls.Add(status);
   progress.SetBounds(30,571,690,8);progress.Style=ProgressBarStyle.Continuous;Controls.Add(progress);
   StyleButton(install,"Install mod",30,604,210);StyleButton(play,"Play mod",256,604,210);StyleButton(logs,"Open setup folder",482,604,238);
   play.Enabled=false;logs.Enabled=false;install.Click+=async(s,e)=>await Install();play.Click+=(s,e)=>Process.Start(new ProcessStartInfo(Path.Combine(folder.Text,"igobygod.exe"),"--play "+Core.Quote(folder.Text)){UseShellExecute=true});logs.Click+=(s,e)=>{if(busy){stop=true;status.Text="Stopping after the current step finishes safely…";return;}if(Directory.Exists(folder.Text))Process.Start(new ProcessStartInfo(folder.Text){UseShellExecute=true});};
   FormClosing+=(s,e)=>{if(busy){e.Cancel=true;stop=true;status.Text="Stopping after the current step finishes safely…";}};
   Shown+=(s,e)=>{try{DetectGames();}catch(System.IO.IOException){}catch(UnauthorizedAccessException){}};
  }
  void AddLabel(string text,int x,int y,int w,int h,int size){Controls.Add(new Label{Text=text,Bounds=new Rectangle(x,y,w,h),Font=new Font("Segoe UI",size)});}
  void StyleButton(Button b,string text,int x,int y,int width){b.Text=text;b.SetBounds(x,y,width,42);b.FlatStyle=FlatStyle.Flat;b.BackColor=Color.FromArgb(170,240,110);b.ForeColor=Color.FromArgb(20,23,29);Controls.Add(b);}
  void Row(string label,TextBox box,int y){AddLabel(label,30,y,690,24,10);box.SetBounds(30,y+27,568,28);box.BackColor=Color.FromArgb(35,40,48);box.ForeColor=Color.White;box.BorderStyle=BorderStyle.FixedSingle;Controls.Add(box);var b=new Button();StyleButton(b,"Browse…",611,y+21,109);b.Height=34;b.Click+=(s,e)=>{using(var dialog=new FolderBrowserDialog{Description=label,ShowNewFolderButton=box==folder})if(dialog.ShowDialog(this)==DialogResult.OK)box.Text=dialog.SelectedPath;};}
  void ShowDetails(){MessageBox.Show("This preview rebuilds the mod using your own GTA V Legacy and Elden Ring installations. Game files stay untouched; the mod uses offline mode and a separate save.\n\nSetup downloads verified tools from Python/NuGet, Microsoft, Git for Windows, Eclipse Adoptium and Rust, then the conversion tools listed in the included source documentation. Their licenses apply. Microsoft C++ tools and the Visual C++ runtime may install system components; other tools stay in the mod setup folder.\n\nThe download is small; the first installation downloads several GB and can take a while. Windows gameplay and broad game-version compatibility remain unverified.\n\nKeep the complete installation folder: the shortcut needs its tools. To remove the local mod, close the game and delete this folder and its shortcut. Microsoft components can be managed in Windows Installed Apps.","Before installing",MessageBoxButtons.OK,MessageBoxIcon.Information);}
  async Task Install(){
   if(!consent.Checked){MessageBox.Show("Select the preparation checkbox to allow setup to download and prepare its required tools.","igobygod");return;}
   if(string.IsNullOrWhiteSpace(gta.Text)||string.IsNullOrWhiteSpace(er.Text)||string.IsNullOrWhiteSpace(folder.Text)){MessageBox.Show("Choose both game folders and an installation folder.","igobygod");return;}
   busy=true;stop=false;logs.Enabled=true;logs.Text="Stop after this step";install.Enabled=false;play.Enabled=false;foreach(Control c in Controls)if(c is Button && c!=install && c!=play && c!=logs)c.Enabled=false;gta.Enabled=er.Enabled=folder.Enabled=consent.Enabled=false;progress.Style=ProgressBarStyle.Marquee;
   string g=gta.Text,r=er.Text,d=folder.Text;
   try{var updates=new Progress<string>(message=>status.Text=message);await Task.Run(()=>new Engine(message=>{if(stop)throw new OperationCanceledException("Setup stopped at a safe point. Completed downloads are kept; reopen setup to continue.");((IProgress<string>)updates).Report(message);}).Install(g,r,d));play.Enabled=File.Exists(Path.Combine(d,"launch.json"));if(play.Enabled){progress.Style=ProgressBarStyle.Continuous;progress.Value=100;status.Text="Ready. Use Play mod here or the shortcut on your desktop.";install.Text="Installed";}}
   catch(Exception error){status.Text="Setup stopped. Your game files were left in place.";MessageBox.Show(error.Message,"Setup needs attention",MessageBoxButtons.OK,MessageBoxIcon.Warning);install.Enabled=true;}
   finally{busy=false;logs.Text="Open setup folder";progress.Style=ProgressBarStyle.Continuous;if(!play.Enabled){gta.Enabled=er.Enabled=folder.Enabled=consent.Enabled=true;foreach(Control c in Controls)if(c is Button && c!=install && c!=play && c!=logs)c.Enabled=true;}logs.Enabled=Directory.Exists(d);}
  }
  void DetectGames(){
   var libraries=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
   using(var key=Registry.CurrentUser.OpenSubKey(@"Software\Valve\Steam")){var steam=key==null?null:key.GetValue("SteamPath") as string;if(steam!=null){libraries.Add(steam);var vdf=Path.Combine(steam,"steamapps","libraryfolders.vdf");if(File.Exists(vdf))foreach(Match m in Regex.Matches(File.ReadAllText(vdf),"\"path\"\\s+\"([^\"]+)\""))libraries.Add(m.Groups[1].Value.Replace(@"\\",@"\"));}}
   foreach(var library in libraries){var common=Path.Combine(library,"steamapps","common");foreach(var name in new[]{"Grand Theft Auto V Legacy","Grand Theft Auto V"}){var p=Path.Combine(common,name);if(File.Exists(Path.Combine(p,"GTA5.exe")))gta.Text=p;}var game=Path.Combine(common,"ELDEN RING","Game");if(File.Exists(Path.Combine(game,"eldenring.exe")))er.Text=game;}
  }
 }
}
