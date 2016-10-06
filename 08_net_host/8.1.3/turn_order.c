#include <stdio.h>
#include <arpa/inet.h>
/* 联合类型的变量类型，用于测试字节序
*  	成员value的高低端字节可以由成员type按字节访问
 */
/*16位字节序转换的结构*/
typedef union{
	unsigned short int value;				/*16位short类型变量value*/
	unsigned char byte[2];					/*char类型数组，共16位*/
}to16;

/*32位字节序转换的结构*/
typedef union{
	unsigned long int value;				/*32位unsigned long类型变量*/
	unsigned char byte[4];					/*char类型数组，共32位*/
}to32;

#define BITS16 16									/*常量，16*/
#define BITS32 32									/*常量，32*/
/* 按照字节打印，begin为字节开始，
*  flag为BITS16表示16位，
*  flag为BITS32表示32位， 
*/
void showvalue(unsigned char *begin, int flag)
{
	int num = 0, i = 0;
	if(flag == BITS16){								/*一个16b的变量*/
		num = 2;	
	}else if(flag == BITS32){						/*一个32b的变量*/
		num = 4;	
	}
	
	for(i = 0; i< num; i++)							/*显示每个字节的值*/
	{
		printf("%x ",*(begin+i));	
	}
	printf("\n");
}

int main(int argc, char *argv[])
{	
	to16 v16_orig, v16_turn1,v16_turn2;			/*一个to16类型变量*/
	to32 v32_orig, v32_turn1,v32_turn2; 			/*一个to32类型变量*/
	
	v16_orig.value = 0xabcd;						/*赋值为0xabcd*/
	v16_turn1.value = htons(v16_orig.value);		/*第一次转换*/
	v16_turn2.value = ntohs(v16_turn1.value);		/*第二次转换*/
	
	v32_orig.value = 0x12345678;				/*赋值为0x12345678*/
	v32_turn1.value = htonl(v32_orig.value);	/*第一次转换*/
	v32_turn2.value = ntohl(v32_turn1.value);	/*第二次转换*/
	
	/*打印结果*/
	printf("16 host to network byte order change:\n");
	printf("\torig:\t");showvalue(v16_orig.byte, BITS16);
	/*16位数值的原始值*/
	printf("\t1 times:");showvalue(v16_turn1.byte, BITS16); 
	/*16位数值的第一次转换后的值*/
	printf("\t2 times:");showvalue(v16_turn2.byte, BITS16); 
	/*16位数值的第二次转换后的值*/

	printf("32 host to network byte order change:\n");
	printf("\torig:\t");showvalue(v32_orig.byte, BITS32);   
	/*32位数值的原始值*/
	printf("\t1 times:");showvalue(v32_turn1.byte, BITS32); 
	/*32位数值的第一次转换后的值*/
	printf("\t2 times:");showvalue(v32_turn2.byte, BITS32); 
	/*32位数值的第二次转换后的值*/

	return 0;	
}

