#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
int main(int argc, char *argv[])
{
	struct hostent *ht=NULL;
	
	char host[]="www.sina.com.cn";				/*查询sina的主机域名*/
	char host1[]="www.sohu.com";				/*查询sohu的主机域名*/
	char str[30];
	struct hostent *ht1=NULL, *ht2=NULL;
	
	ht1 = gethostbyname(host);					/*查询"www.sina.com.cn"*/
	ht2 = gethostbyname(host1);					/*查询"www.sohu.com"*/
	int j = 0; 	
	for(j = 0;j<2;j++){
		if(j == 0)
			ht = ht1;								/*sina*/
		else
			ht =ht2;								/*sohu*/
		
		if(ht){
			int i = 0;
			printf("get the host:%s addr\n",host);			/*原始域名*/
			printf("name:%s\n",ht->h_name);				/*名称*/
			
			/*协议族AF_INET为IPv4或者AF_INET6为IPv6*/
			printf("type:%s\n",ht->h_addrtype==AF_INET?"AF_INET":"AF_INET6"); 		
			
			printf("legnth:%d\n",ht->h_length);			/*IP地址的长度*/
			/*打印IP地址*/
			for(i=0;;i++){
				if(ht->h_addr_list[i] != NULL){	/*不是IP地址数组的结尾*/
					printf("IP:%s\n",inet_ntop(ht->h_addrtype,ht->h_addr_list[i],str,30));							/*打印IP地址*/
				}	else{							/*达到结尾*/
					break;							/*退出for循环*/
				}
			}
			
			/*打印域名地址*/
			for(i=0;;i++){/*循环*/
				if(ht->h_aliases[i] != NULL){		/*没有到达域名数组的结尾*/
					printf("alias %d:%s\n",i,ht->h_aliases[i]);															/*打印域名*/
				}	else{							/*结尾*/
					break;							/*退出循环*/
				}
			}
		}
	}
	return 0;
}
