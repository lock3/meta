; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc -verify-machineinstrs -mcpu=pwr9 -ppc-vsr-nums-as-vr \
; RUN:   -mtriple=powerpc64le-unknown-unknown -ppc-asm-full-reg-names < %s | \
; RUN:   FileCheck %s
; Function Attrs: nounwind readnone speculatable
declare <4 x float> @llvm.fmuladd.v4f32(<4 x float>, <4 x float>, <4 x float>) #0

; Function Attrs: nounwind readnone speculatable
declare { i32, i1 } @llvm.usub.with.overflow.i32(i32, i32) #0

define void @draw_llvm_vs_variant0(<4 x float> %x) {
; CHECK-LABEL: draw_llvm_vs_variant0:
; CHECK:       # %bb.0: # %entry
; CHECK-NEXT:    lfd f0, 0(r3)
; CHECK-NEXT:    xxpermdi v3, f0, f0, 2
; CHECK-NEXT:    vmrglh v3, v3, v3
; CHECK-NEXT:    vextsh2w v3, v3
; CHECK-NEXT:    xvcvsxwsp vs0, v3
; CHECK-NEXT:    xxspltw vs0, vs0, 2
; CHECK-NEXT:    xvmaddasp vs0, v2, v2
; CHECK-NEXT:    stxvx vs0, 0, r3
; CHECK-NEXT:    blr
entry:
  %.size = load i32, i32* undef
  %0 = call { i32, i1 } @llvm.usub.with.overflow.i32(i32 %.size, i32 7)
  %1 = extractvalue { i32, i1 } %0, 0
  %2 = call { i32, i1 } @llvm.usub.with.overflow.i32(i32 %1, i32 0)
  %3 = extractvalue { i32, i1 } %2, 0
  %4 = select i1 false, i32 0, i32 %3
  %5 = xor i1 false, true
  %6 = sext i1 %5 to i32
  %7 = load <4 x i16>, <4 x i16>* undef, align 2
  %8 = extractelement <4 x i16> %7, i32 0
  %9 = sext i16 %8 to i32
  %10 = insertelement <4 x i32> undef, i32 %9, i32 0
  %11 = extractelement <4 x i16> %7, i32 1
  %12 = sext i16 %11 to i32
  %13 = insertelement <4 x i32> %10, i32 %12, i32 1
  %14 = extractelement <4 x i16> %7, i32 2
  %15 = sext i16 %14 to i32
  %16 = insertelement <4 x i32> %13, i32 %15, i32 2
  %17 = extractelement <4 x i16> %7, i32 3
  %18 = sext i16 %17 to i32
  %19 = insertelement <4 x i32> %16, i32 %18, i32 3
  %20 = sitofp <4 x i32> %19 to <4 x float>
  %21 = insertelement <4 x i32> undef, i32 %6, i32 0
  %22 = shufflevector <4 x i32> %21, <4 x i32> undef, <4 x i32> zeroinitializer
  %23 = bitcast <4 x float> %20 to <4 x i32>
  %24 = and <4 x i32> %23, %22
  %25 = bitcast <4 x i32> %24 to <4 x float>
  %26 = shufflevector <4 x float> %25, <4 x float> undef, <4 x i32> <i32 1, i32 1, i32 1, i32 1>
  %27 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %x, <4 x float> %x, <4 x float> %26)
  store <4 x float> %27, <4 x float>* undef
  ret void
}
