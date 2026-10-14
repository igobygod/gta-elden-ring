use sha2::{Digest,Sha256};
use std::io::Read;
extern "C" {fn proof_start();}
const SUPPORTED_EXE_SHA256:&str="1a3547101327f65d0c76da2f9190ac0aa66871ea42bae2aecc61e11a8b597891";
fn approved_process()->bool {
 let Ok(path)=std::env::current_exe() else{return false};
 let Ok(mut file)=std::fs::File::open(path) else{return false};
 let mut hash=Sha256::new();let mut chunk=[0u8;1024*1024];
 loop {match file.read(&mut chunk){Ok(0)=>break,Ok(n)=>hash.update(&chunk[..n]),Err(_)=>return false}}
 format!("{:x}",hash.finalize())==SUPPORTED_EXE_SHA256
}
#[no_mangle]
pub extern "system" fn DllMain(_:usize,reason:u32,_:usize)->i32 {
 if reason==1 {std::thread::spawn(||{if approved_process(){unsafe{proof_start()}}});}1
}
