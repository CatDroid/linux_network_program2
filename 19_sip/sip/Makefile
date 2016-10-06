CC = gcc

TARGET = sip_ether.o sip_skbuff.o sip_arp.o sip_ip.o sip_icmp.o sip_udp.o sip.o 
CFLAGS = -g
#sip_arp.o
sip:clean $(TARGET)
	gcc -o sip $(TARGET) 
clean:
	rm -rf *.o sip *.o
run:sip
	./sip
