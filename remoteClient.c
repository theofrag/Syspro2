
#include <stdio.h>

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h> /* internet sockets */
#include <netdb.h>  /* gethostbyaddr */
#include <unistd.h> /* fork */
#include <stdlib.h> /* exit */

#include <string.h>

#include <arpa/inet.h>

#include <fcntl.h>

void perror_exit(char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}


int main(void){

    char* addr = "127.0.0.1";
    int port = 12500;
    
    char* dirName = "./test";
    
    printf("directory: %s\n",dirName);

    // create socket
    int sock;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1 )
        perror_exit( "socket" );

    // looup server's address and connect there
    struct in_addr myaddress;
    inet_aton(addr,&myaddress);

    struct sockaddr_in server;
    struct sockaddr *serverptr = (struct sockaddr*)&server;

    server.sin_family = AF_INET;
    server.sin_addr = myaddress;
    server.sin_port = htons(port);

    if (connect(sock, serverptr, sizeof(server)) < 0)
        perror_exit("connect");
    
    printf("Connecting to %s port %d\n", addr, port);

    // send dir name

    if (write(sock,dirName,strlen(dirName))<0)
        perror_exit("write");
    

    // create file
    // open file

    // read file data
    int time=0;
    FILE* sock_fp;
    if ((sock_fp = fdopen(sock,"r")) == NULL)
        perror_exit("fdopen");

        char fromfgets[20];
        while(fgets(fromfgets,21,sock_fp)!=NULL){
            

            if(strcmp(fromfgets,"CONTERM\n")==0){
                break;
            }
            printf("%s",fromfgets);

        }


    // write data to the file
    
    // increase counter
    
    // if counter == filenumber break from while(1)

    // close socket
    // close connection

    return 0;

}


// FILE* sock_fp;
//         if ((sock_fp = fdopen(sock,"r+")) == NULL)
//             perror_exit("fdopen");