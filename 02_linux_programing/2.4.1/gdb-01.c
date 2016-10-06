/*文件名：gdb-01.c*/
#include <stdio.h>/* 用于printf*/
#include <stdlib.h>/*用于malloc*/
/* 声明函数sum()为static int类型 */
static int sum(int value);
/* 用于控制输入输出的结构 */
struct inout{
	int value;
	int result;
};
int main(int argc, char *argv[]){
	
	struct inout *io = (struct inout*)malloc(sizeof(struct inout));													/*申请内存*/
	
	if(NULL == io)								/*判断是否成功*/
	{
		printf("申请内存失败\n");					/*打印失败信息*/
		return -1;								/*返回-1*/
	}
		
	if(argc !=2)								/*判断输入参数是否正确*/
	{
		printf("参数输入错误!\n");				/*打印失败信息*/
		return -1;								/*返回-1*/
	}	
	
	io->value = *argv[1]-'0';						/* 获得输入的参数 */
	io->result = sum(io->value);					/* 对value进行累加求和 													*/
	printf("你输入的值为：%d,计算结果为：%d\n",io->value,io->result);
	return 0;
}
/* 累加求和函数 */
static int sum(int value){
	int result = 0;
	int i = 0;
	
	for(i=0;i<=value;i++)							/* 循环计算累加值 */
		result += i;
	
	return result;								/*返回结果*/
}
