/* main.c */
#include <stdio.h>
/*需要包含的头文件*/
#include "add.h"
#include "sub.h"
int main(void)
{
	/*声明计算所用的变量 *a、b为整型，x、y为浮点型*/	
	int a = 10, b = 12;
	float x= 1.23456,y = 9.87654321;
	
	/*调用函数并将计算结果打印出来*/
	printf("int a+b IS:%d\n",add_int(a,b));		/*计算整型加*/
	printf("int a-b IS:%d\n",sub_int(a,b)); 		/*计算整型减*/
	printf("float x+y IS:%f\n",add_float(x,y)); 	/*计算浮点型加*/
	printf("float x-y IS:%f\n",sub_float(x,y));	/*计算浮点型减*/
	return 0;
}
