
remoteClient: remoteClient.o
	gcc -g remoteClient.o -o remoteClient 

dataServer: dataServer.o
	g++ -g3 dataServer.o -o dataServer -lpthread

runServer:dataServer
	./dataServer -p 12500 -q 5 -b 512 -s 5

runClient: remoteClient
	./remoteClient -i 127.0.0.1 -p 12500 -d ./test