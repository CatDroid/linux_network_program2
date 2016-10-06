#生成cacu，“:”右边为目标
cacu:add_int.o add_float.o sub_int.o sub_float.o main.o
	gcc -o cacu add/add_int.o add/add_float.o \
			sub/sub_int.o sub/sub_float.o main.o 
#生成add_int.o的规则，将add_int.c编译成目标文件add_int.o
add_int.o:add/add_int.c add/add.h
	gcc -c -o add/add_int.o add/add_int.c
#生成add_float.o的规则
add_float.o:add/add_float.c add/add.h
	gcc -c -o add/add_float.o add/add_float.c
#生成sub_int.o的规则
sub_int.o:sub/sub_int.c sub/sub.h
	gcc -c -o sub/sub_int.o sub/sub_int.c
#生成sub_float.o的规则
sub_float.o:sub/sub_float.c sub/sub.h
	gcc -c -o sub/sub_float.o sub/sub_float.c
#生成main.o的规则
main.o:main.c add/add.h sub/sub.h
	gcc -c -o main.o main.c -Iadd -Isub
	
#清理的规则
clean:
	rm -f cacu add/add_int.o add/add_float.o \
			sub/sub_int.o sub/sub_float.o main.o
