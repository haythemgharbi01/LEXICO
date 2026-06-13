; ModuleID = 'lexico'
source_filename = "lexico"

@.fmt.0 = private unnamed_addr constant [5 x i8] c"%lld\00", align 1
@.fmt.1 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@.fmt.2 = private unnamed_addr constant [5 x i8] c"done\00", align 1
@.fmt.3 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@.fmt.4 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare i32 @printf(ptr, ...)

declare i32 @scanf(ptr, ...)

declare i64 @lx_array_new(i64, i64)

declare i64 @lx_table_new(i64)

declare i64 @lx_matrix_new(i64, i64, i64)

declare i64 @lx_len(i64)

declare i64 @lx_rows(i64)

declare i64 @lx_cols(i64)

declare i64 @lx_strlen(ptr)

declare i64 @lx_str_at(ptr, i64)

declare ptr @lx_str_concat(ptr, ptr)

declare ptr @lx_str_minus(ptr, ptr)

declare ptr @lx_i64_to_string(i64)

declare ptr @lx_f64_to_string(double)

declare ptr @lx_char_to_string(i64)

declare ptr @lx_bool_to_string(i64)

declare i64 @lx_string_to_i64(ptr)

declare double @lx_string_to_f64(ptr)

declare i64 @lx_string_to_char(ptr)

declare ptr @lx_extract_from(ptr, ptr)

declare ptr @lx_extract_range(i64, i64, ptr)

declare i64 @lx_str_contains(ptr, ptr)

declare i64 @lx_str_contains_char(ptr, i64)

declare i64 @lx_str_position(ptr, ptr)

declare i64 @lx_str_position_char(ptr, i64)

declare i64 @lx_count(i64, i64, i64, double, i64, ptr)

declare i64 @lx_position(i64, i64, i64, double, i64, ptr)

declare void @lx_sort(i64, i64)

declare void @lx_set(i64, i64, i64, i64, double, i64, ptr)

declare void @lx_append(i64, i64, i64, i64, double, i64, ptr)

declare i64 @lx_get_tag(i64, i64)

declare i64 @lx_get_i(i64, i64)

declare double @lx_get_d(i64, i64)

declare i64 @lx_get_c(i64, i64)

declare ptr @lx_get_s(i64, i64)

declare void @lx_matrix_set(i64, i64, i64, i64, i64, double, i64, ptr)

declare i64 @lx_matrix_get_i(i64, i64, i64)

declare double @lx_matrix_get_d(i64, i64, i64)

declare i64 @lx_matrix_get_c(i64, i64, i64)

declare ptr @lx_matrix_get_s(i64, i64, i64)

declare void @lx_print_cell(i64, i64)

declare void @lx_print_array_ascii(i64, ptr)

declare void @lx_print_table_ascii(i64, ptr)

declare void @lx_print_matrix_ascii(i64, ptr)

declare i64 @lx_file_open(ptr, i64)

declare void @lx_file_close(i64)

declare void @lx_file_write(i64, ptr)

declare void @lx_file_write_line(i64, i64, ptr)

declare i64 @lx_file_read_all(i64)

declare ptr @lx_file_read_line(i64, i64)

declare i64 @lx_file_read_first(i64, i64)

declare i64 @lx_file_read_range(i64, i64, i64)

declare void @lx_file_clear(i64)

declare void @lx_file_clear_line(i64, i64)

declare void @lx_file_clear_range(i64, i64, i64)

declare void @lx_file_set_title(i64, ptr)

declare i64 @lx_file_line_count(i64)

declare void @lx_cleanup()

define i32 @main() {
entry:
  %i = alloca i64, align 8
  store i64 0, ptr %i, align 4
  br label %wh.cond.0

wh.cond.0:                                        ; preds = %wh.body.0, %entry
  %i1 = load i64, ptr %i, align 4
  %cmp = icmp slt i64 %i1, 5
  br i1 %cmp, label %wh.body.0, label %wh.after.0

wh.body.0:                                        ; preds = %wh.cond.0
  %i2 = load i64, ptr %i, align 4
  %0 = call i32 (ptr, ...) @printf(ptr @.fmt.0, i64 %i2)
  %1 = call i32 (ptr, ...) @printf(ptr @.fmt.1)
  %i3 = load i64, ptr %i, align 4
  %add = add i64 %i3, 1
  store i64 %add, ptr %i, align 4
  br label %wh.cond.0

wh.after.0:                                       ; preds = %wh.cond.0
  %2 = call i32 (ptr, ...) @printf(ptr @.fmt.3, ptr @.fmt.2)
  %3 = call i32 (ptr, ...) @printf(ptr @.fmt.4)
  ret i32 0
}
