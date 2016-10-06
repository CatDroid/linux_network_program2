/*动态加载库示例*/
#include <dlfcn.h>/*动态加载库库头*/
#include <stdio.h>
int main(void)
{
  char src[]="Hello Dymatic";					/*要计算的字符串*/
  int (*pStrLenFun)(char *str);					/*函数指针*/
  void *phandle = NULL;							/*库句柄*/
  char *perr = NULL;							/*错误信息指针*/
  phandle = dlopen("./libstr.so", RTLD_LAZY);	/*打开libstr.so动态链接库*/
  /*判断是否正确打开*/
  if(!phandle)									/*打开错误*/
  {
    printf("Failed Load library!\n");			/*打印库不能加载信息*/
  }
  perr = dlerror();								/*读取错误值*/	
  if(perr != NULL)								/*存在错误*/
  {
    printf("%s\n",perr);
    return 0;									/*正常返回*/
  }
  
  pStrLenFun = dlsym(phandle, "StrLen");		/*获得函数StrLen的地址*/
  perr = dlerror();								/*读取错误信息*/
  if(perr != NULL)								/*存在错误*/
  {
    printf("%s\n",perr);						/*打印错误函数获得的错误信息*/
    return 0;									/*返回*/
  }
  
  printf("the string length is: %d\n",pStrLenFun(src)); 													/*调用函数pStrLenFunc计算字符串的长度*/
  dlclose(phandle);					/*关闭动态加载库*/
  return 0;
}
