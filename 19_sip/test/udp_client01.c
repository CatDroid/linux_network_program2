#include <sys/types.h>
#include <sys/socket.h>	/*socket()/bind()*/
#include <linux/in.h>	/*struct sockaddr_in*/
#include <string.h>		/*memset()*/
#include <stdio.h>

#define PORT_SERV 8888
#define BUFF_LEN 256

int main(int argc, char *argv[])
{
	int s;
	struct sockaddr_in addr_serv;
	char buff[BUFF_LEN] = "UDP TEST";
	int len = sizeof(addr_serv);
	
	s = socket(AF_INET, SOCK_DGRAM, 0);
	
	memset(&addr_serv, 0, sizeof(addr_serv));
	addr_serv.sin_family = AF_INET;
	addr_serv.sin_addr.s_addr = inet_addr("172.16.12.250");
	addr_serv.sin_port = htons(PORT_SERV);
	for(;;){
		sendto(s, buff, strlen(buff), 0, (struct sockaddr*)&addr_serv, len);
		//sleep(5);
	}
		
	close(s);
	return 0;	
}

