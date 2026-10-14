#include "../../src/native_animation_policy.h"
#include <cassert>
#include <cstdio>
int main(){using namespace native_animation_policy;assert(base(42034000)==34000);assert(base(42020110)==20110);assert(base(42022100)==22100);assert(base(42027110)==27110);assert(base(202030)==202030);assert(base(202140)==202140);assert(base(-1)==-1);assert(control_action(42020110));assert(control_action(42000000));assert(!control_action(42034000));assert(!control_action(202030));assert(!control_action(202140));assert(!control_action(-1));puts("PASS verified million-bank IDs; special202xxx stays blocked");}
