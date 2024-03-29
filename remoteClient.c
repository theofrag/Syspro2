
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

int create_dir_files(char* pathAndFile);

int main(int argc,char* argv[]){

    

    char addr[20];
    int port = 12500;
    
    char dirName[256];


    // control arguments
    if(argc != 7){
        printf("Give right number of arguments\n");
        return -1;
    }else{
        for(int i=1;i<argc;i=i+2){
            if(strcmp(argv[i],"-i")==0){
                strcpy(addr,argv[i+1]);
            }
            else if(strcmp(argv[i],"-p")==0){
                port = atoi(argv[i+1]);
            }
            else if(strcmp(argv[i],"-d")==0){
                strcpy(dirName,argv[i+1]);
            }
            else{
                printf("Give right arguments\n");
                return -1;
            }
        }
    }
    
    printf("Client's parameters are:\n");
    printf("ServerIP: %s\n",addr);
    printf("port: %d\n",port);
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
        int fd;
        int remained=0;
        int sofar=0;
        char* fname;
        while(fgets(fromfgets,256,sock_fp)!=NULL){


            if((newFile == 1) && (strcmp(fromfgets,"\n")==0) ){
                
                continue;
            }
            
            if((newFile == 1) && (strcmp(fromfgets,"CONTERM\n")!=0) ){
                newFile = 2;
                fd = create_dir_files(fromfgets);
                fname = malloc(sizeof(char)*strlen(fromfgets)+1);
                strcpy(fname,fromfgets);
                fname[strlen(fname)-1]='\0';
                continue;

            }
            else if(newFile == 2){
                newFile = 0;
                fromfgets[strlen(fromfgets)-1]='\0';
                remained = atoi(fromfgets);
                printf("%d\n",remained);

                continue;
            }

            else if(strcmp(fromfgets,"CONTERM\n")==0){
                break;
            }
            sofar+= strlen(fromfgets);

   

            if(sofar > remained){
                printf("Received: %s\n",fname);
                fromfgets[strlen(fromfgets)-1]='\0';
                free(fname);
                sofar=0;
                remained = 0;
                newFile = 1;
            }

            int sz;
            if((sz = write(fd,fromfgets,strlen(fromfgets)))<0){
                perror("write");
                exit(1);
            }

        }


    fclose(sock_fp);
    close(sock);

    return 0;

}

int create_dir_files(char* pathAndFile){

    char* temp = malloc(strlen(pathAndFile)+1);

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
            temp[i-count+1] = '\0';
            if((mkdir(temp, 0777)<0 && (errno != EEXIST))){
                perror("mkdir");
            }
            
          
        }
        else{
            first = 1;
            temp[i-count] = pathAndFile[i];
            temp[i-count+1] = '\0';
        }
        

    }

            
        int fd;
        temp[strlen(temp)-1]='\0';

        if( (fd = open(temp,O_CREAT|O_RDWR|O_EXCL ,0777))<0){

            if(errno == EEXIST){

                unlink(temp);
                if((fd = open(temp,O_CREAT|O_RDWR|O_EXCL ,0777))<0){
                    perror("open1");
                    exit(2);
                }
            }
        
        }

        free(temp);
        
        return fd;
}