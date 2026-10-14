use std::{collections::HashSet,env,ffi::{c_void,CString},fs,os::windows::ffi::OsStrExt,path::{Path,PathBuf}};
#[link(name="kernel32")]
extern "system" {fn LoadLibraryW(name:*const u16)->*mut c_void;fn GetProcAddress(module:*mut c_void,name:*const i8)->*mut c_void;fn FreeLibrary(module:*mut c_void)->i32;}
type Decode=unsafe extern "system" fn(*const u8,isize,*mut u8,isize,i32,i32,i32,*mut c_void,isize,*mut c_void,*mut c_void,*mut c_void,isize,i32)->isize;
struct Job { input:PathBuf, output:PathBuf, size:usize }
fn source(job:&Job)->Result<Vec<u8>,Box<dyn std::error::Error>> {
 if job.output.exists(){return Err("Output already exists".into())}
 if job.size==0||job.size>1024*1024*1024{return Err("Invalid decoded size".into())}
 let src=fs::read(&job.input)?;
 if src.len()<76||&src[..4]!=b"DCX\0"||&src[40..44]!=b"KRAK"{return Err("Expected a KRAK DCX".into())}
 if u32::from_be_bytes(src[28..32].try_into()?) as usize!=job.size{return Err("DCX size differs from caller".into())}
 let compressed=u32::from_be_bytes(src[32..36].try_into()?)as usize;
 if src.len()<76+compressed||src.len()>76+compressed+15||src[76+compressed..].iter().any(|x|*x!=0){return Err("DCX compressed payload length mismatch".into())}
 Ok(src)
}
fn run()->Result<(),Box<dyn std::error::Error>> {
 let a:Vec<_>=env::args_os().collect();
 let (dll,jobs)=if a.len()==4 && a[1]=="--batch" {
  let list=fs::read_to_string(&a[3])?;let mut jobs=Vec::new();let mut outputs=HashSet::new();
  for line in list.lines() {
   if line.is_empty(){continue}
   let parts:Vec<_>=line.split('\t').collect();if parts.len()!=3{return Err("Expected INPUT<TAB>OUTPUT<TAB>SIZE for every batch line".into())}
   let output=PathBuf::from(parts[1]);
   if !outputs.insert(parts[1].replace('/',"\\").to_lowercase()){return Err("Duplicate output in batch".into())}
   jobs.push(Job{input:PathBuf::from(parts[0]),output,size:parts[2].parse()?});
  }
  if jobs.is_empty()||jobs.len()>10000{return Err("Invalid batch length".into())}
  (Path::new(&a[2]),jobs)
 } else if a.len()==5 {
  (Path::new(&a[1]),vec![Job{input:PathBuf::from(&a[2]),output:PathBuf::from(&a[3]),size:a[4].to_str().ok_or("Invalid size")?.parse()?}])
 } else {return Err("Use: igobygod-oodle-helper OODLE_DLL INPUT_DCX OUTPUT_RAW EXPECTED_SIZE; or --batch OODLE_DLL JOBS_TSV".into())};
 // Validate the entire batch before loading the library or writing any outputs.
 for job in &jobs {source(job)?;}
 let name:Vec<u16>=dll.as_os_str().encode_wide().chain(Some(0)).collect();
 unsafe{
  let module=LoadLibraryW(name.as_ptr());if module.is_null(){return Err("Cannot load selected game's Oodle DLL".into())}
  struct Library(*mut c_void);impl Drop for Library{fn drop(&mut self){unsafe{FreeLibrary(self.0);}}}let _library=Library(module);
  let symbol=CString::new("OodleLZ_Decompress")?;let ptr=GetProcAddress(module,symbol.as_ptr());if ptr.is_null(){return Err("Oodle decompressor export missing".into())}
  let decode:Decode=std::mem::transmute(ptr);
  for job in &jobs {
   let src=source(job)?;let compressed=u32::from_be_bytes(src[32..36].try_into()?)as usize;let mut out=vec![0u8;job.size+65536];
   let got=decode(src.as_ptr().add(76),compressed as isize,out.as_mut_ptr(),job.size as isize,1,0,0,std::ptr::null_mut(),0,std::ptr::null_mut(),std::ptr::null_mut(),std::ptr::null_mut(),0,3);
   if got!=job.size as isize{return Err(format!("Decoded length mismatch: {} != {}",got,job.size).into())}
   out.truncate(job.size);use std::io::Write;
   let mut file=fs::OpenOptions::new().write(true).create_new(true).open(&job.output)?;file.write_all(&out)?;
  }
 }
 println!("Decoded {} file(s) using the locally selected game's Oodle library",jobs.len());Ok(())
}
fn main(){if let Err(e)=run(){eprintln!("{}",e);std::process::exit(1)}}
