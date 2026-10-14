#pragma once

// Shared scalar math is stringified into HLSL and compiled directly by the
// portable regression check, so CPU checks exercise the production equations.
// The RGB lower bound allows one 8-bit step of capture quantization. Without
// it a dark channel rounding from 1/255 to zero makes a faint fade look opaque.
#define UI_COMPOSITE_MATH \
float uiChannelCoverage(float before,float after){ \
 return saturate(max((after-before-1.f/255.f)/max(1.f-before,.000001f),(before-after-1.f/255.f)/max(before,.000001f))); \
} \
float uiCoverage(float br,float bg,float bb,float ba,float ar,float ag,float ab,float aa){ \
 float rgb=max(max(uiChannelCoverage(br,ar),uiChannelCoverage(bg,ag)),uiChannelCoverage(bb,ab)); \
 float alpha=ba<.999f?saturate((aa-ba)/max(1.f-ba,.000001f)):0.f; \
 return max(rgb,alpha); \
} \
float uiPremul(float before,float after,float coverage){return min(coverage,max(0.f,after-before*(1.f-coverage)));} \
float uiCompose(float custom,float ui,float coverage,float customAlpha){return custom*(1.f-coverage)+ui*customAlpha;}

#define UI_COMPOSITE_STRING_INNER(...) #__VA_ARGS__
#define UI_COMPOSITE_STRING(...) UI_COMPOSITE_STRING_INNER(__VA_ARGS__)
#define UI_COMPOSITE_HLSL UI_COMPOSITE_STRING(UI_COMPOSITE_MATH)
