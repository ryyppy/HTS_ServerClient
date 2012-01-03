all: myserver myclient

myserver:  myserver.o myutil.o 
	gcc -Wall -o myserver myserver.o myutil.o -lldap -DLDAP_DEPRECATED

myserver.o: myserver.c  
	gcc -Wall -o myserver.o  -c myserver.c

myclient: myclient.o myutil.o
	gcc -Wall -o myclient myclient.o myutil.o

myclient.o: myclient.c
	gcc -Wall -o myclient.o -c myclient.c

myutil.o: myutil.c
	gcc -Wall -o myutil.o -c myutil.c

clean: 
	rm -f myserver
	rm -f myclient
	rm -f myclient.o
	rm -f myserver.o
	rm -f myutil.o
