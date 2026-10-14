#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

// The shader and this check compile the same scalar compositing expressions.
static float max(float a,float b){return std::max(a,b);}
static float min(float a,float b){return std::min(a,b);}
static float saturate(float x){return std::clamp(x,0.f,1.f);}
#include "ui-composite.h"
UI_COMPOSITE_MATH

struct Pixel {float r,g,b,a;};
static Pixel recover(Pixel before,Pixel after){
 float a=uiCoverage(before.r,before.g,before.b,before.a,after.r,after.g,after.b,after.a);
 return {uiPremul(before.r,after.r,a),uiPremul(before.g,after.g,a),uiPremul(before.b,after.b,a),a};
}
static Pixel overCustom(Pixel custom,Pixel ui){
 return {uiCompose(custom.r,ui.r,ui.a,custom.a),uiCompose(custom.g,ui.g,ui.a,custom.a),uiCompose(custom.b,ui.b,ui.a,custom.a),custom.a};
}
static Pixel over(Pixel a,Pixel b){return {a.r+b.r*(1-a.a),a.g+b.g*(1-a.a),a.b+b.b*(1-a.a),1};}
static int failures=0;
static void check(const char*name,Pixel got,Pixel want,float tolerance=.00001f){
 const float d=std::max({std::fabs(got.r-want.r),std::fabs(got.g-want.g),std::fabs(got.b-want.b),std::fabs(got.a-want.a)});
 if(d>tolerance){++failures;std::printf("FAIL %s: got %.6f %.6f %.6f %.6f, want %.6f %.6f %.6f %.6f\n",name,got.r,got.g,got.b,got.a,want.r,want.g,want.b,want.a);}
}
int main(){
 const Pixel character{.8f,.4f,.2f,1};
 check("faint backdrop keeps body",overCustom(character,recover({.4f,.6f,.2f,1},{.38f,.57f,.19f,1})),{.76f,.38f,.19f,1},.01f);
 check("half black backdrop darkens body",overCustom(character,recover({.4f,.6f,.2f,1},{.2f,.3f,.1f,1})),{.4f,.2f,.1f,1},.01f);
 check("one quantized dark channel cannot make a faint gradient opaque",overCustom(character,recover({.4f,.6f,1.f/255,1},{.38f,.57f,0,1})),{.76f,.38f,.19f,1},.01f);
 check("transparent target black backdrop includes alpha-only change",overCustom(character,recover({0,0,0,0},{0,0,0,.5f})),{.4f,.2f,.1f,1});
 check("opaque white text covers body",overCustom(character,recover({.4f,.6f,.2f,1},{1,1,1,1})),{1,1,1,1},.01f);
 check("white glyph antialiasing preserves half body",overCustom(character,recover({.4f,.6f,.2f,1},{.7f,.8f,.6f,1})),{.9f,.7f,.6f,1},.01f);
 check("black glyph covers body",overCustom(character,recover({.4f,.6f,.2f,1},{0,0,0,1})),{0,0,0,1},.01f);
 check("colored bar exact with alpha",overCustom(character,recover({0,0,0,0},{.025f,.35f,.05f,.5f})),{.425f,.55f,.15f,1});
 check("no UI is identity",overCustom(character,recover({.4f,.6f,.2f,1},{.4f,.6f,.2f,1})),character);
 check("existing target contribution is removed",recover({.1f,.2f,.3f,.4f},{.45f,.3f,.2f,.7f}),{.4f,.2f,.05f,.5f});

 // Native frame already contains UI over grass: only the newly drawn custom
 // fragment receives the UI transformation, avoiding a second fullscreen fade.
 const Pixel halfShade{0,0,0,.5f}, nativeFrame{.2f,.3f,.1f,1};
 check("no custom coverage leaves native frame intact",over(overCustom({0,0,0,0},halfShade),nativeFrame),nativeFrame);
 check("translucent car under backdrop",over(overCustom({.4f,.2f,.1f,.5f},halfShade),nativeFrame),{.3f,.25f,.1f,1});
 check("additive flash stays under glyph",overCustom({.4f,.2f,.1f,0},{1,1,1,1}),{0,0,0,0});
 check("additive flash below gradient is attenuated",overCustom({.4f,.2f,.1f,0},halfShade),{.2f,.1f,.05f,0});
 check("shadow cannot darken native text",over(overCustom({0,0,0,.5f},{1,1,1,1}),{1,1,1,1}),{1,1,1,1});
 std::printf("%s: native UI coverage/compositing regression checks\n",failures?"FAILED":"PASS");
 return failures?EXIT_FAILURE:EXIT_SUCCESS;
}
