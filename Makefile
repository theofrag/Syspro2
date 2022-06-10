remoteClient: remoteClient.o
	gcc remoteClient.o -o remoteClient 

dataServer: dataServer.o
	g++ dataServer.o -o dataServer -lpthread

runServer:dataServer
	./dataServer -p 50000 -q 5 -b 256 -s 5

runClient: remoteClient
	./remoteClient -i 127.0.0.1 -p 50000 -d ./test