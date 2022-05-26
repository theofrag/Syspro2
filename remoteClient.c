
#include <stdio.h>

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <dirent.h>

#include <netinet/in.h> /* internet sockets */
#include <netdb.h>  /* gethostbyaddr */
#include <unistd.h> /* fork */
#include <stdlib.h> /* exit */

#include <string.h>

#include <sys/stat.h>

#include <arpa/inet.h>

#include <fcntl.h>

#include <errno.h>

void perror_exit(char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

void create_dir_files(char* pathAndFile);

int main(void){

    char* addr = "127.0.0.1";
    int port = 12500;
    
    char* dirName = "./test";
    
    printf("directory: %s\n",dirName);

    // create socket
    int sock;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1 )
        perror_exit( "socket" );

    struct in_addr myaddress;
    // convert ipv4 form to binary
    inet_aton(addr,&myaddress);

    struct sockaddr_in server;
    struct sockaddr *serverptr = (struct sockaddr*)&server;

    server.sin_family = AF_INET;
    server.sin_addr = myaddress;
    server.sin_port = htons(port);

    // connect to server
    if (connect(sock, serverptr, sizeof(server)) < 0)
        perror_exit("connect");
    
    printf("Connecting to %s port %d\n", addr, port);

    // send dir name to server
    if (write(sock,dirName,strlen(dirName))<0)
        perror_exit("write");
    

    // create file
    // open file

    // read file data
    int time=0;
    FILE* sock_fp;
    
    int newFile = 1;

    if ((sock_fp = fdopen(sock,"r")) == NULL)
        perror_exit("fdopen");

        char fromfgets[256];
        while(fgets(fromfgets,256,sock_fp)!=NULL){
            
            if((newFile == 1) && (strcmp(fromfgets,"CONTERM\n")!=0) ){
                printf("Received: %s\n",fromfgets);
                newFile = 0;
                // create_dir_files(fromfgets);
            }

            if(strcmp(fromfgets,"CONTERM\n")==0){
                break;
            }
            if(strcmp(fromfgets,"ENDOFFILE\n")==0){
                newFile=1;
            }
            // printf("%s",fromfgets);

        }



    // close socket
    // close connection

    return 0;

}

void create_dir_files(char* pathAndFile){

    char* temp = malloc(strlen(pathAndFile)+1);

    char* t2 = malloc(strlen(pathAndFile)+1);

    int count=0;

    int first = 1;
    for(int i=0; i<strlen(pathAndFile); i++){

        if(pathAndFile[i] == '.'){
            if(first == 1){
                temp[i] = pathAndFile[i];
                first=0;
            }
            else
                count++;
        }
        else if(pathAndFile[i]== '/'){
            temp[i-count] = pathAndFile[i];

                if((mkdir(temp, 0777)<0 && (errno != EEXIST))){
                    perror("mkdir");
                }
          
        }
        else{

            temp[i-count] = pathAndFile[i];
        }

    }
            
        printf("%s\n",temp);
        if(open("./testa/a/b/filename",O_EXCL|O_RDWR,S_IRWXU)<0){

        
        perror("open");
        }
}