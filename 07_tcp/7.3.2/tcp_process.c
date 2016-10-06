#include <stdio.h>
#include <string.h>
/*客户端的处理过程*/
void process_conn_client(int s)
{
	ssize_t size = 0;
	char buffer[1024];							/*数据的缓冲区*/
	
	for(;;){									/*循环处理过程*/
		/*从标准输入中读取数据放到缓冲区buffer中*/
		size = read(0, buffer, 1024);
		if(size > 0){							/*读到数据*/
			write(s, buffer, size);				/*发送给服务器*/
			size = read(s, buffer, 1024);		/*从服务器读取数据*/
			write(1, buffer, size);				/*写到标准输出*/
		}
	}	
}
/*服务器对客户端的处理*/
void process_conn_server(int s)
{
	ssize_t size = 0;
	char buffer[1024];							/*数据的缓冲区*/
	
	for(;;){									/*循环处理过程*/		
		size = read(s, buffer, 1024);			/*从套接字中读取数据放到													缓冲区buffer中*/
		if(size == 0){							/*没有数据*/
			return;	
		}
		
		/*构建响应字符，为接收到客户端字节的数量*/
		sprintf(buffer, "%d bytes altogether\n", size);
		write(s, buffer, strlen(buffer)+1);/*发给客户端*/
	}	
}