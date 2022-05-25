
remoteClient: remoteClient.o
	gcc remoteClient.o -o remoteClient 

dataServer: dataServer.o
	g++ dataServer.o -o dataServer -lpthread
