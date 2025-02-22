@.str = private unnamed_addr constant [15 x i8] c"Hello, world!\0A\00", align 1

define i32 @main(i32 noundef %0, ptr noundef %1) #0 {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  %5 = alloca ptr, align 8
  %6 = alloca i32, align 4
  %7 = alloca i32, align 4
  %8 = alloca i8, align 1
  %9 = alloca i8, align 1
  store i32 0, ptr %3, align 4
  store i32 %0, ptr %4, align 4
  store ptr %1, ptr %5, align 8
  %10 = load i32, ptr %4, align 4
  %11 = add nsw i32 %10, 42
  store i32 %11, ptr %6, align 4
  %12 = load i32, ptr %6, align 4
  %13 = mul nsw i32 %12, 2
  store i32 %13, ptr %7, align 4
  %14 = load i32, ptr %7, align 4
  %15 = icmp slt i32 %14, 16
  br i1 %15, label %16, label %17

16:                                               ; preds = %2
  store i8 1, ptr %8, align 1
  br label %18

17:                                               ; preds = %2
  store i8 0, ptr %8, align 1
  br label %18

18:                                               ; preds = %17, %16
  %19 = load i8, ptr %8, align 1
  %20 = trunc i8 %19 to i1
  %21 = zext i1 %20 to i8
  store i8 %21, ptr %9, align 1
  %22 = call i32 (ptr, ...) @printf(ptr noundef @.str)
  ret i32 0
}

declare i32 @printf(ptr noundef, ...) #1

