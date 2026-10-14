use std::{env,fs,path::{Path,PathBuf}};
const GENERATED:[&str;9]=[
 "PlayerRenderer/repair-next/fx/original_grenade_fx.h",
 "PlayerRenderer/polish-fix/rpg/fx/original_rpg_fx.h",
 "PlayerRenderer/polish-fix/rpg/fx/original_gl_fx.h",
 "PlayerRenderer/polish-fix/rpg/fx/original_grenade_typed.h",
 "PlayerRenderer/polish-v4.5/sword/sword_carry.h",
 "PlayerRenderer/polish-v4/radar/hud_atlas.h",
 "PlayerRenderer/polish-v4.3/camera-wheel/wheel_atlas.h",
 "PlayerRenderer/polish-v4.6/sword/sword_attack.h",
 "PlayerRenderer/polish-v4.6/Params/expected_params.h",
];
fn path(p:&Path)->String {p.to_string_lossy().replace('\\',"/")}
fn compiler()->cc::Build {
 let mut c=cc::Build::new();c.cpp(true).cpp_link_stdlib(None).flag("/std:c++17").flag("/EHsc")
 .define("WIN32_LEAN_AND_MEAN",None).define("NOMINMAX",None)
 .flag(&format!("/FI{}",path(&Path::new("src/portable_paths.h").canonicalize().unwrap())));
 c
}
fn main(){
 println!("cargo:rerun-if-env-changed=IGOBYGOD_GENERATED_ROOT");
 let generated=PathBuf::from(env::var_os("IGOBYGOD_GENERATED_ROOT").expect("Run local asset preparation first, then set IGOBYGOD_GENERATED_ROOT to its workspace output."));
 let output=PathBuf::from(env::var_os("OUT_DIR").unwrap()).join("local-generated");
 let source=Path::new("..").canonicalize().unwrap();
 let mut macros=Vec::new();
 for (i,relative) in GENERATED.iter().enumerate(){
  let input=generated.join(relative);let dest=output.join(relative);
  let text=fs::read_to_string(&input).unwrap_or_else(|_|panic!("Missing local game-derived header: {}",input.display()));
  fs::create_dir_all(dest.parent().unwrap()).unwrap();
  // Redirect generated headers' authored-code includes back to the source tree.
  let mut fixed=String::new();
  for line in text.lines(){
   if let Some(value)=line.trim().strip_prefix("#include \"").and_then(|s|s.strip_suffix('"')) {
    let rel=Path::new(relative).parent().unwrap().join(value);
    let candidate=source.join(&rel);
    if candidate.is_file()&&!GENERATED.iter().any(|g|source.join(g).canonicalize().ok()==candidate.canonicalize().ok()) {
     fixed.push_str(&format!("#include \"{}\"\n",path(&candidate.canonicalize().unwrap())));continue;
    }
   }
   fixed.push_str(line);fixed.push('\n');
  }
  fs::write(&dest,fixed).unwrap();println!("cargo:rerun-if-changed={}",input.display());
  macros.push((format!("IGOBYGOD_GENERATED_{i}"),format!("\"{}\"",path(&dest))));
 }
 let root="../Bridge/vendor/er-reference";
 let mut render=compiler();render.include(format!("{root}/src")).include(format!("{root}/include"))
  .include("../Source/vendor/hudhook/vendor/minhook/include")
  .file("src/renderer.cpp").file(format!("{root}/src/memutil.cpp"));
 for (k,v) in &macros{render.define(k.as_str(),Some(v.as_str()));}render.compile("proof");
 let native="../NativePort/combat-source/native";let mut combat=compiler();combat.include(native);
 for file in ["rifle.cpp","native_melee.cpp","shoulder_camera.cpp","native_ui_state.cpp","native_lock_marker.cpp","native_locomotion.cpp","native_ui_capture.cpp","player_status.cpp","cheat_console.cpp","arsenal_audio.cpp"]{combat.file(format!("src/{file}"));}
 for file in ["car-proof/vehicle_demo.cpp","car-proof/car_collision_probe.cpp"]{combat.file(file);}
 for file in ["input.cpp","gameplay_gate.cpp"]{combat.file(format!("{native}/{file}"));}
 for (k,v) in &macros{combat.define(k.as_str(),Some(v.as_str()));}combat.compile("rifle");
 let mh="../Source/vendor/hudhook/vendor/minhook/src";
 cc::Build::new().include(format!("{mh}/../include")).file(format!("{mh}/hook.c")).file(format!("{mh}/buffer.c")).file(format!("{mh}/trampoline.c")).file(format!("{mh}/hde/hde64.c")).compile("minhook_rifle");
 for lib in ["dinput8","dxguid","winmm","kernel32","user32","d3d12","dxgi","d3dcompiler"]{println!("cargo:rustc-link-lib={lib}");}
 // Watch source directories and vendored code; generated payloads remain local.
 for dir in ["src","car-proof","car-drive","polish-fix","polish-next","polish-v4","polish-v4.3","polish-v4.4","polish-v4.5","polish-v4.6","repair-next","../NativePort/combat-source/native","../Bridge/vendor/er-reference","../Source/vendor/hudhook/vendor/minhook"]{println!("cargo:rerun-if-changed={dir}");}
}
