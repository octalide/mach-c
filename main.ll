; ModuleID = '../mach/src/main.mach'
source_filename = "../mach/src/main.mach"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@str = private unnamed_addr constant [17 x i8] c"variadic sum ok\0A\00", align 1
@str.2 = private unnamed_addr constant [18 x i8] c"variadic zero ok\0A\00", align 1
@str.4 = private unnamed_addr constant [17 x i8] c"variadic avg ok\0A\00", align 1
@str.6 = private unnamed_addr constant [14 x i8] c"hello, world\0A\00", align 1
@str.7 = private unnamed_addr constant [47 x i8] c"time.now_nanos returned 0 (ok on unsupported)\0A\00", align 1
@str.8 = private unnamed_addr constant [48 x i8] c"time.now_millis returned 0 (ok on unsupported)\0A\00", align 1
@str.9 = private unnamed_addr constant [16 x i8] c"ascii/digit ok\0A\00", align 1
@str.10 = private unnamed_addr constant [6 x i8] c"12345\00", align 1
@str.11 = private unnamed_addr constant [5 x i8] c"1a2b\00", align 1
@str.12 = private unnamed_addr constant [10 x i8] c"parse ok\0A\00", align 1
@str.13 = private unnamed_addr constant [3 x i8] c"hi\00", align 1
@str.15 = private unnamed_addr constant [23 x i8] c"/tmp/mach_std_test.txt\00", align 1
@str.16 = private unnamed_addr constant [7 x i8] c"fs ok\0A\00", align 1

define noundef i32 @main() local_unnamed_addr {
entry:
  tail call void @print({ ptr, i64 } { ptr @str.6, i64 13 })
  tail call void @print({ ptr, i64 } { ptr @str, i64 16 })
  tail call void @print({ ptr, i64 } { ptr @str.2, i64 17 })
  tail call void @print({ ptr, i64 } { ptr @str.4, i64 16 })
  %0 = tail call i64 @now_nanos()
  %1 = tail call i64 @now_millis()
  %eq = icmp eq i64 %0, 0
  br i1 %eq, label %then, label %ifcont

then:                                             ; preds = %entry
  tail call void @print({ ptr, i64 } { ptr @str.7, i64 46 })
  br label %ifcont

ifcont:                                           ; preds = %entry, %then
  %eq1 = icmp eq i64 %1, 0
  br i1 %eq1, label %then3, label %ifcont5

then3:                                            ; preds = %ifcont
  tail call void @print({ ptr, i64 } { ptr @str.8, i64 47 })
  br label %ifcont5

ifcont5:                                          ; preds = %ifcont, %then3
  %2 = tail call i32 @digit_val(i8 65)
  %lt = icmp slt i32 %2, 10
  br i1 %lt, label %then7, label %ifcont9

then7:                                            ; preds = %ifcont5
  tail call void @print({ ptr, i64 } { ptr @str.9, i64 15 })
  br label %ifcont9

ifcont9:                                          ; preds = %ifcont5, %then7
  %3 = tail call i64 @parse_u64_dec({ ptr, i64 } { ptr @str.10, i64 5 })
  %4 = tail call i64 @parse_u64_hex({ ptr, i64 } { ptr @str.11, i64 4 })
  %gt = icmp ne i64 %3, 0
  %gt10 = icmp ne i64 %4, 0
  %and = and i1 %gt, %gt10
  br i1 %and, label %then12, label %ifcont14

then12:                                           ; preds = %ifcont9
  tail call void @print({ ptr, i64 } { ptr @str.12, i64 9 })
  br label %ifcont14

ifcont14:                                         ; preds = %ifcont9, %then12
  %5 = tail call i32 @fs_write_all({ ptr, i64 } { ptr @str.15, i64 22 }, { ptr, i64 } { ptr @str.13, i64 2 })
  %6 = tail call { ptr, i64 } @fs_read_all({ ptr, i64 } { ptr @str.15, i64 22 })
  %.fca.1.extract = extractvalue { ptr, i64 } %6, 1
  %eq15 = icmp eq i64 %.fca.1.extract, 2
  br i1 %eq15, label %then17, label %ifcont19

then17:                                           ; preds = %ifcont14
  tail call void @print({ ptr, i64 } { ptr @str.16, i64 6 })
  br label %ifcont19

ifcont19:                                         ; preds = %ifcont14, %then17
  ret i32 0
}

define void @test_variadics() local_unnamed_addr {
ifcont24:
  tail call void @print({ ptr, i64 } { ptr @str, i64 16 })
  tail call void @print({ ptr, i64 } { ptr @str.2, i64 17 })
  tail call void @print({ ptr, i64 } { ptr @str.4, i64 16 })
  ret void
}

; Function Attrs: nofree norecurse nosync nounwind memory(read, inaccessiblemem: none)
define double @avg_variadic(double %first, i64 %__mach_vararg_count, ptr nocapture readonly %__mach_vararg_data) local_unnamed_addr #0 {
entry:
  %lt8.not = icmp eq i64 %__mach_vararg_count, 0
  br i1 %lt8.not, label %loop.exit, label %loop.body.preheader

loop.body.preheader:                              ; preds = %entry
  %xtraiter = and i64 %__mach_vararg_count, 7
  %0 = icmp ult i64 %__mach_vararg_count, 8
  br i1 %0, label %loop.exit.loopexit.unr-lcssa, label %loop.body.preheader.new

loop.body.preheader.new:                          ; preds = %loop.body.preheader
  %unroll_iter = and i64 %__mach_vararg_count, -8
  br label %loop.body

loop.body:                                        ; preds = %loop.body, %loop.body.preheader.new
  %idx.010 = phi i64 [ 0, %loop.body.preheader.new ], [ %add2.7, %loop.body ]
  %total.09 = phi double [ %first, %loop.body.preheader.new ], [ %add.7, %loop.body ]
  %sunkaddr = mul i64 %idx.010, 8
  %sunkaddr29 = getelementptr i8, ptr %__mach_vararg_data, i64 %sunkaddr
  %mach_va_ptr = load ptr, ptr %sunkaddr29, align 8
  %deref = load double, ptr %mach_va_ptr, align 8
  %add = fadd double %total.09, %deref
  %sunkaddr30 = mul i64 %idx.010, 8
  %sunkaddr31 = getelementptr i8, ptr %__mach_vararg_data, i64 %sunkaddr30
  %sunkaddr32 = getelementptr i8, ptr %sunkaddr31, i64 8
  %mach_va_ptr.1 = load ptr, ptr %sunkaddr32, align 8
  %deref.1 = load double, ptr %mach_va_ptr.1, align 8
  %add.1 = fadd double %add, %deref.1
  %sunkaddr33 = mul i64 %idx.010, 8
  %sunkaddr34 = getelementptr i8, ptr %__mach_vararg_data, i64 %sunkaddr33
  %sunkaddr35 = getelementptr i8, ptr %sunkaddr34, i64 16
  %mach_va_ptr.2 = load ptr, ptr %sunkaddr35, align 8
  %deref.2 = load double, ptr %mach_va_ptr.2, align 8
  %add.2 = fadd double %add.1, %deref.2
  %sunkaddr36 = mul i64 %idx.010, 8
  %sunkaddr37 = getelementptr i8, ptr %__mach_vararg_data, i64 %sunkaddr36
  %sunkaddr38 = getelementptr i8, ptr %sunkaddr37, i64 24
  %mach_va_ptr.3 = load ptr, ptr %sunkaddr38, align 8
  %deref.3 = load double, ptr %mach_va_ptr.3, align 8
  %add.3 = fadd double %add.2, %deref.3
  %sunkaddr39 = mul i64 %idx.010, 8
  %sunkaddr40 = getelementptr i8, ptr %__mach_vararg_data, i64 %sunkaddr39
  %sunkaddr41 = getelementptr i8, ptr %sunkaddr40, i64 32
  %mach_va_ptr.4 = load ptr, ptr %sunkaddr41, align 8
  %deref.4 = load double, ptr %mach_va_ptr.4, align 8
  %add.4 = fadd double %add.3, %deref.4
  %sunkaddr42 = mul i64 %idx.010, 8
  %sunkaddr43 = getelementptr i8, ptr %__mach_vararg_data, i64 %sunkaddr42
  %sunkaddr44 = getelementptr i8, ptr %sunkaddr43, i64 40
  %mach_va_ptr.5 = load ptr, ptr %sunkaddr44, align 8
  %deref.5 = load double, ptr %mach_va_ptr.5, align 8
  %add.5 = fadd double %add.4, %deref.5
  %sunkaddr45 = mul i64 %idx.010, 8
  %sunkaddr46 = getelementptr i8, ptr %__mach_vararg_data, i64 %sunkaddr45
  %sunkaddr47 = getelementptr i8, ptr %sunkaddr46, i64 48
  %mach_va_ptr.6 = load ptr, ptr %sunkaddr47, align 8
  %deref.6 = load double, ptr %mach_va_ptr.6, align 8
  %add.6 = fadd double %add.5, %deref.6
  %sunkaddr48 = mul i64 %idx.010, 8
  %sunkaddr49 = getelementptr i8, ptr %__mach_vararg_data, i64 %sunkaddr48
  %sunkaddr50 = getelementptr i8, ptr %sunkaddr49, i64 56
  %mach_va_ptr.7 = load ptr, ptr %sunkaddr50, align 8
  %deref.7 = load double, ptr %mach_va_ptr.7, align 8
  %add.7 = fadd double %add.6, %deref.7
  %add2.7 = add nuw i64 %idx.010, 8
  %niter.ncmp.7 = icmp eq i64 %unroll_iter, %add2.7
  br i1 %niter.ncmp.7, label %loop.exit.loopexit.unr-lcssa, label %loop.body

loop.exit.loopexit.unr-lcssa:                     ; preds = %loop.body, %loop.body.preheader
  %add.lcssa.ph = phi double [ undef, %loop.body.preheader ], [ %add.7, %loop.body ]
  %idx.010.unr = phi i64 [ 0, %loop.body.preheader ], [ %add2.7, %loop.body ]
  %total.09.unr = phi double [ %first, %loop.body.preheader ], [ %add.7, %loop.body ]
  %lcmp.mod.not = icmp eq i64 %xtraiter, 0
  br i1 %lcmp.mod.not, label %loop.exit, label %loop.body.epil.preheader

loop.body.epil.preheader:                         ; preds = %loop.exit.loopexit.unr-lcssa
  %1 = shl i64 %idx.010.unr, 3
  %scevgep = getelementptr i8, ptr %__mach_vararg_data, i64 %1
  br label %loop.body.epil

loop.body.epil:                                   ; preds = %loop.body.epil.preheader, %loop.body.epil
  %total.09.epil = phi double [ %add.epil, %loop.body.epil ], [ %total.09.unr, %loop.body.epil.preheader ]
  %epil.iter = phi i64 [ %epil.iter.next, %loop.body.epil ], [ 0, %loop.body.epil.preheader ]
  %2 = shl nuw nsw i64 %epil.iter, 3
  %scevgep12 = getelementptr i8, ptr %scevgep, i64 %2
  %mach_va_ptr.epil = load ptr, ptr %scevgep12, align 8
  %deref.epil = load double, ptr %mach_va_ptr.epil, align 8
  %add.epil = fadd double %total.09.epil, %deref.epil
  %epil.iter.next = add i64 %epil.iter, 1
  %epil.iter.cmp.not = icmp eq i64 %xtraiter, %epil.iter.next
  br i1 %epil.iter.cmp.not, label %loop.exit, label %loop.body.epil, !llvm.loop !0

loop.exit:                                        ; preds = %loop.body.epil, %loop.exit.loopexit.unr-lcssa, %entry
  %total.0.lcssa = phi double [ %first, %entry ], [ %add.lcssa.ph, %loop.exit.loopexit.unr-lcssa ], [ %add.epil, %loop.body.epil ]
  %uitofp = uitofp i64 %__mach_vararg_count to double
  %add3 = fadd double %uitofp, 1.000000e+00
  %div = fdiv double %total.0.lcssa, %add3
  ret double %div
}

; Function Attrs: nofree norecurse nosync nounwind memory(read, inaccessiblemem: none)
define i64 @sum_variadic(i64 %base, i64 %__mach_vararg_count, ptr nocapture readonly %__mach_vararg_data) local_unnamed_addr #0 {
entry:
  %lt6.not = icmp eq i64 %__mach_vararg_count, 0
  br i1 %lt6.not, label %loop.exit, label %loop.body.preheader

loop.body.preheader:                              ; preds = %entry
  %min.iters.check = icmp ult i64 %__mach_vararg_count, 16
  br i1 %min.iters.check, label %loop.body.preheader14, label %vector.ph

vector.ph:                                        ; preds = %loop.body.preheader
  %n.vec = and i64 %__mach_vararg_count, -16
  %0 = insertelement <4 x i64> <i64 poison, i64 0, i64 0, i64 0>, i64 %base, i64 0
  br label %vector.body

vector.body:                                      ; preds = %vector.body, %vector.ph
  %index = phi i64 [ 0, %vector.ph ], [ %index.next, %vector.body ]
  %vec.phi = phi <4 x i64> [ %0, %vector.ph ], [ %49, %vector.body ]
  %vec.phi9 = phi <4 x i64> [ zeroinitializer, %vector.ph ], [ %50, %vector.body ]
  %vec.phi10 = phi <4 x i64> [ zeroinitializer, %vector.ph ], [ %51, %vector.body ]
  %vec.phi11 = phi <4 x i64> [ zeroinitializer, %vector.ph ], [ %52, %vector.body ]
  %sunkaddr = mul i64 %index, 8
  %sunkaddr56 = getelementptr i8, ptr %__mach_vararg_data, i64 %sunkaddr
  %1 = load ptr, ptr %sunkaddr56, align 8
  %sunkaddr57 = mul i64 %index, 8
  %sunkaddr58 = getelementptr i8, ptr %__mach_vararg_data, i64 %sunkaddr57
  %sunkaddr59 = getelementptr i8, ptr %sunkaddr58, i64 8
  %2 = load ptr, ptr %sunkaddr59, align 8
  %sunkaddr60 = mul i64 %index, 8
  %sunkaddr61 = getelementptr i8, ptr %__mach_vararg_data, i64 %sunkaddr60
  %sunkaddr62 = getelementptr i8, ptr %sunkaddr61, i64 16
  %3 = load ptr, ptr %sunkaddr62, align 8
  %sunkaddr63 = mul i64 %index, 8
  %sunkaddr64 = getelementptr i8, ptr %__mach_vararg_data, i64 %sunkaddr63
  %sunkaddr65 = getelementptr i8, ptr %sunkaddr64, i64 24
  %4 = load ptr, ptr %sunkaddr65, align 8
  %sunkaddr66 = mul i64 %index, 8
  %sunkaddr67 = getelementptr i8, ptr %__mach_vararg_data, i64 %sunkaddr66
  %sunkaddr68 = getelementptr i8, ptr %sunkaddr67, i64 32
  %5 = load ptr, ptr %sunkaddr68, align 8
  %sunkaddr69 = mul i64 %index, 8
  %sunkaddr70 = getelementptr i8, ptr %__mach_vararg_data, i64 %sunkaddr69
  %sunkaddr71 = getelementptr i8, ptr %sunkaddr70, i64 40
  %6 = load ptr, ptr %sunkaddr71, align 8
  %sunkaddr72 = mul i64 %index, 8
  %sunkaddr73 = getelementptr i8, ptr %__mach_vararg_data, i64 %sunkaddr72
  %sunkaddr74 = getelementptr i8, ptr %sunkaddr73, i64 48
  %7 = load ptr, ptr %sunkaddr74, align 8
  %sunkaddr75 = mul i64 %index, 8
  %sunkaddr76 = getelementptr i8, ptr %__mach_vararg_data, i64 %sunkaddr75
  %sunkaddr77 = getelementptr i8, ptr %sunkaddr76, i64 56
  %8 = load ptr, ptr %sunkaddr77, align 8
  %sunkaddr78 = mul i64 %index, 8
  %sunkaddr79 = getelementptr i8, ptr %__mach_vararg_data, i64 %sunkaddr78
  %sunkaddr80 = getelementptr i8, ptr %sunkaddr79, i64 64
  %9 = load ptr, ptr %sunkaddr80, align 8
  %sunkaddr81 = mul i64 %index, 8
  %sunkaddr82 = getelementptr i8, ptr %__mach_vararg_data, i64 %sunkaddr81
  %sunkaddr83 = getelementptr i8, ptr %sunkaddr82, i64 72
  %10 = load ptr, ptr %sunkaddr83, align 8
  %sunkaddr84 = mul i64 %index, 8
  %sunkaddr85 = getelementptr i8, ptr %__mach_vararg_data, i64 %sunkaddr84
  %sunkaddr86 = getelementptr i8, ptr %sunkaddr85, i64 80
  %11 = load ptr, ptr %sunkaddr86, align 8
  %sunkaddr87 = mul i64 %index, 8
  %sunkaddr88 = getelementptr i8, ptr %__mach_vararg_data, i64 %sunkaddr87
  %sunkaddr89 = getelementptr i8, ptr %sunkaddr88, i64 88
  %12 = load ptr, ptr %sunkaddr89, align 8
  %sunkaddr90 = mul i64 %index, 8
  %sunkaddr91 = getelementptr i8, ptr %__mach_vararg_data, i64 %sunkaddr90
  %sunkaddr92 = getelementptr i8, ptr %sunkaddr91, i64 96
  %13 = load ptr, ptr %sunkaddr92, align 8
  %sunkaddr93 = mul i64 %index, 8
  %sunkaddr94 = getelementptr i8, ptr %__mach_vararg_data, i64 %sunkaddr93
  %sunkaddr95 = getelementptr i8, ptr %sunkaddr94, i64 104
  %14 = load ptr, ptr %sunkaddr95, align 8
  %sunkaddr96 = mul i64 %index, 8
  %sunkaddr97 = getelementptr i8, ptr %__mach_vararg_data, i64 %sunkaddr96
  %sunkaddr98 = getelementptr i8, ptr %sunkaddr97, i64 112
  %15 = load ptr, ptr %sunkaddr98, align 8
  %sunkaddr99 = mul i64 %index, 8
  %sunkaddr100 = getelementptr i8, ptr %__mach_vararg_data, i64 %sunkaddr99
  %sunkaddr101 = getelementptr i8, ptr %sunkaddr100, i64 120
  %16 = load ptr, ptr %sunkaddr101, align 8
  %17 = load i64, ptr %1, align 8
  %18 = load i64, ptr %2, align 8
  %19 = load i64, ptr %3, align 8
  %20 = load i64, ptr %4, align 8
  %21 = insertelement <4 x i64> poison, i64 %17, i64 0
  %22 = insertelement <4 x i64> %21, i64 %18, i64 1
  %23 = insertelement <4 x i64> %22, i64 %19, i64 2
  %24 = insertelement <4 x i64> %23, i64 %20, i64 3
  %25 = load i64, ptr %5, align 8
  %26 = load i64, ptr %6, align 8
  %27 = load i64, ptr %7, align 8
  %28 = load i64, ptr %8, align 8
  %29 = insertelement <4 x i64> poison, i64 %25, i64 0
  %30 = insertelement <4 x i64> %29, i64 %26, i64 1
  %31 = insertelement <4 x i64> %30, i64 %27, i64 2
  %32 = insertelement <4 x i64> %31, i64 %28, i64 3
  %33 = load i64, ptr %9, align 8
  %34 = load i64, ptr %10, align 8
  %35 = load i64, ptr %11, align 8
  %36 = load i64, ptr %12, align 8
  %37 = insertelement <4 x i64> poison, i64 %33, i64 0
  %38 = insertelement <4 x i64> %37, i64 %34, i64 1
  %39 = insertelement <4 x i64> %38, i64 %35, i64 2
  %40 = insertelement <4 x i64> %39, i64 %36, i64 3
  %41 = load i64, ptr %13, align 8
  %42 = load i64, ptr %14, align 8
  %43 = load i64, ptr %15, align 8
  %44 = load i64, ptr %16, align 8
  %45 = insertelement <4 x i64> poison, i64 %41, i64 0
  %46 = insertelement <4 x i64> %45, i64 %42, i64 1
  %47 = insertelement <4 x i64> %46, i64 %43, i64 2
  %48 = insertelement <4 x i64> %47, i64 %44, i64 3
  %49 = add <4 x i64> %24, %vec.phi
  %50 = add <4 x i64> %32, %vec.phi9
  %51 = add <4 x i64> %40, %vec.phi10
  %52 = add <4 x i64> %48, %vec.phi11
  %index.next = add nuw i64 %index, 16
  %53 = icmp eq i64 %n.vec, %index.next
  br i1 %53, label %middle.block, label %vector.body, !llvm.loop !2

middle.block:                                     ; preds = %vector.body
  %bin.rdx = add <4 x i64> %50, %49
  %bin.rdx12 = add <4 x i64> %51, %bin.rdx
  %bin.rdx13 = add <4 x i64> %52, %bin.rdx12
  %rdx.shuf = shufflevector <4 x i64> %bin.rdx13, <4 x i64> poison, <4 x i32> <i32 2, i32 3, i32 poison, i32 poison>
  %bin.rdx53 = add <4 x i64> %bin.rdx13, %rdx.shuf
  %rdx.shuf54 = shufflevector <4 x i64> %bin.rdx53, <4 x i64> poison, <4 x i32> <i32 1, i32 poison, i32 poison, i32 poison>
  %bin.rdx55 = add <4 x i64> %bin.rdx53, %rdx.shuf54
  %54 = extractelement <4 x i64> %bin.rdx55, i32 0
  %cmp.n = icmp eq i64 %n.vec, %__mach_vararg_count
  br i1 %cmp.n, label %loop.exit, label %loop.body.preheader14

loop.body.preheader14:                            ; preds = %loop.body.preheader, %middle.block
  %idx.08.ph = phi i64 [ 0, %loop.body.preheader ], [ %n.vec, %middle.block ]
  %total.07.ph = phi i64 [ %base, %loop.body.preheader ], [ %54, %middle.block ]
  br label %loop.body

loop.body:                                        ; preds = %loop.body.preheader14, %loop.body
  %idx.08 = phi i64 [ %add2, %loop.body ], [ %idx.08.ph, %loop.body.preheader14 ]
  %total.07 = phi i64 [ %add, %loop.body ], [ %total.07.ph, %loop.body.preheader14 ]
  %55 = shl i64 %idx.08, 3
  %scevgep = getelementptr i8, ptr %__mach_vararg_data, i64 %55
  %mach_va_ptr = load ptr, ptr %scevgep, align 8
  %deref = load i64, ptr %mach_va_ptr, align 8
  %add = add i64 %deref, %total.07
  %add2 = add nuw i64 %idx.08, 1
  %exitcond.not = icmp eq i64 %__mach_vararg_count, %add2
  br i1 %exitcond.not, label %loop.exit, label %loop.body, !llvm.loop !5

loop.exit:                                        ; preds = %loop.body, %middle.block, %entry
  %total.0.lcssa = phi i64 [ %base, %entry ], [ %54, %middle.block ], [ %add, %loop.body ]
  ret i64 %total.0.lcssa
}

declare i64 @now_nanos() local_unnamed_addr

declare i64 @now_millis() local_unnamed_addr

declare i64 @parse_u64_dec({ ptr, i64 }) local_unnamed_addr

declare i64 @parse_u64_hex({ ptr, i64 }) local_unnamed_addr

declare i32 @digit_val(i8) local_unnamed_addr

declare { ptr, i64 } @fs_read_all({ ptr, i64 }) local_unnamed_addr

declare i32 @fs_write_all({ ptr, i64 }, { ptr, i64 }) local_unnamed_addr

declare void @print({ ptr, i64 }) local_unnamed_addr

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i64 @llvm.vector.reduce.add.v4i64(<4 x i64>) #1

attributes #0 = { nofree norecurse nosync nounwind memory(read, inaccessiblemem: none) }
attributes #1 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }

!0 = distinct !{!0, !1}
!1 = !{!"llvm.loop.unroll.disable"}
!2 = distinct !{!2, !3, !4}
!3 = !{!"llvm.loop.isvectorized", i32 1}
!4 = !{!"llvm.loop.unroll.runtime.disable"}
!5 = distinct !{!5, !4, !3}
