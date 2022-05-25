#include <stdio.h>

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <dirent.h>

#include <netinet/in.h> /* internet sockets */
#include <netdb.h>  /* gethostbyaddr */
#include <unistd.h> /* fork */
#include <stdlib.h> /* exit */

#include <pthread.h>
#include <unistd.h>

#include <string.h>

#include <map>

#define  MAX_CON 40
#define  POOL_SIZE 10
#define WORKERS 20
#define BLOCK_SIZE 512

#include <iostream>

using namespace std;


// from lectures
typedef struct{
    char* data[POOL_SIZE];
    int start;
    int end;
    int count;
}pool_t;

map <int, pthread_mutex_t*> workersMutexes;
map <int,int > mapNumbers;

pthread_mutex_t mtx;
pthread_cond_t cond_nonempty;
pthread_cond_t cond_nonfull;
pool_t pool;

void place(pool_t * pool, char* data) {

    pthread_mutex_lock(&mtx);
    while (pool->count >= POOL_SIZE) {
        pthread_cond_wait(&cond_nonfull, &mtx);
    }

    pool->end = (pool->end + 1) % POOL_SIZE;
    pool->data[pool->end] = new char[strlen(data)+1];

    strcpy(pool->data[pool->end],data);
    pool->count++;
    cout<<"[Thread: "<<pthread_self()<<"]: Adding file "<<pool->data[pool->end]<<" to the queue"<<endl;
    pthread_mutex_unlock(&mtx);

}


char* obtain(pool_t * pool) {
    
    pthread_mutex_lock(&mtx);
    while (pool->count <= 0) {
        pthread_cond_wait(&cond_nonempty, &mtx);
    }
    char* data;
    data = pool->data[pool->start];

    pool->start = (pool->start + 1) % POOL_SIZE;
    pool->count--;
    cout << "[Thread: "<<pthread_self()<<"]: Received task: < "<<data<<">"<<endl;
    pthread_mutex_unlock(&mtx);
    return data;
}


void initialize(pool_t * pool) {
    pool->start = 0;
    pool->end = -1;
    pool->count = 0;
}


void perror_exit(char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}


void* communication_thread(void* socket){

    char dirName[256] ;
    int comSocket = (long)socket;

    // read dirName from socket
    if(read(comSocket,dirName,256)<0)
        perror_exit("read");
    
    cout<<"[Thread: "<<pthread_self()<<"]: About to scan directory "<< dirName<<endl;

    // read number of files in folder
    int numberOfFiles = 0;
    // find <path/folder> -type f -printf "%p\n" | wc -l
    char command[256];
    snprintf(command,256+strlen(dirName),"find %s -type f -printf \"%%p\\n\" | wc -l ",dirName);
    
    // popen to invoke  find <path/folder> -type f -printf "%p\n" | wc -l
    FILE* pipe_fp;
    if( (pipe_fp =  popen(command,"r")) == NULL)
        perror_exit("popen");

    // tranfer data from find to socket

    char numstr[5];
    
    if (( fgets(numstr,sizeof(int)+1,pipe_fp))==NULL )
        perror_exit("fgets");
    ;
    // close pipe
    if( pclose(pipe_fp)<0)
        perror_exit("pclose");

    // write number to socket

    // if (write(comSocket,numstr,strlen(numstr)) < 0 )
    //     perror_exit("write");

    // add number to map
    int num = atoi(numstr); 
    mapNumbers.insert(pair<int,int>(comSocket,num));


    // retrieve file names from server using open and remove newline
    snprintf(command,256 + strlen(dirName),"find %s -type f -printf \"%%p\\n\"",dirName);
    if( (pipe_fp =  popen(command,"r")) == NULL)
        perror_exit("popen");
    
    char fname[256];
    while (( fgets(fname,256,pipe_fp)) != NULL ){

        fname[strlen(fname)-1] = '\0';
        char temp[300];
        snprintf(temp,300,"%s %d",fname,comSocket);
        place(&pool,temp);
        pthread_cond_signal(&cond_nonempty);
        // for(int i=0;i<100000000;i++);
    }

    // close pipe
    if( pclose(pipe_fp)<0)
        perror_exit("pclose");
  


}


void* worker_thread(void*){

    bool dead;
    while(1){
        dead = false;
        // consume name from queue
        char* consumed = obtain(&pool);
        pthread_cond_signal(&cond_nonfull);

        // retrieve filename and socketno
        char* filename;
        char* socketno;
        char* rest = NULL;
        
        filename = strtok_r(consumed," ",&rest);
        socketno = strtok_r(NULL," ",&rest);

        int sock = atoi(socketno);

        pthread_mutex_t * mx = workersMutexes.at(sock);

        // down socket mutex
        
        
        pthread_mutex_lock(mx);

        // send filename with '\n' to client
        
        if( write(sock,filename,strlen(filename))<0 )
            perror_exit("write1");
        if( write(sock,"\n",strlen("\n"))<0 )
            perror_exit("write2");

        // open file

        // read data of blocksize
        FILE* file_fp;
        if ((file_fp = fopen(filename,"r"))== NULL)
            perror_exit("fopen");
        
        // read from file and write it to socket
        // pthread_mutex_lock(&mtx);
        cout<<"[Thread: "<<pthread_self()<<"]: About to read file "<< filename<<endl;
        // pthread_mutex_unlock(&mtx);



        char fromfgets[BLOCK_SIZE];
        while(fgets(fromfgets,BLOCK_SIZE+1,file_fp)){
            write(sock,fromfgets,strlen(fromfgets));
        }
        write(sock,"\n",1);

        // reduce counter of filenames
        int n = mapNumbers.at(sock);

        n--;
        if(n==0){
            //do something
            char* terminate = "CONTERM\n";
            if( write(sock,terminate,strlen(terminate))<0 )
                perror_exit("write3");
            dead = true;

        }else{
            mapNumbers[sock] = n;
        }

        // up socket semaphore
        pthread_mutex_unlock(mx);
        
        // destroy semaphore
        if(dead == true){
            workersMutexes.erase(sock);
            mapNumbers.erase(sock);
            close(sock);    
        }
    }
}



int main(int argc, char** argv){

    int sock;
    int port = 12500;
    int workers = WORKERS;
    int queue_size;
    int block_size;
    int thread_pool_size;


    // control arguments
    if(argc != 9){
        cout<<"Give right number of arguments"<<endl;
        return -1;
    }else{
        for(int i=1;i<argc;i=i+2){
            if(strcmp(argv[i],"-p")){
                port = atoi(argv[i+1]);
            }else if(strcmp(argv[i],"-q")){
                queue_size = atoi(argv[i+1]);
            }else if(strcmp(argv[i],"-b")){
                block_size = atoi(argv[i+1]);
            }else if(strcmp(argv[i],"-s")){
                thread_pool_size = atoi(argv[i+1]);
            }else{
                cout<<"Give right arguments"<<endl;
                return -1;
            }
        }
    }




    initialize(&pool);

    // init mutexes and cond variables for consumer producer model
    pthread_mutex_init(&mtx, 0);
    pthread_cond_init(&cond_nonempty, 0);
    pthread_cond_init(&cond_nonfull, 0);


    // create worker threads
    for(int i=0;i<workers;i++){
        pthread_t newThread;
        pthread_create(&newThread,0,worker_thread,NULL);
    }

    // create a socket
    if((sock = socket(AF_INET,SOCK_STREAM,0)) == -1 )
        perror("Failed to create socket");
    
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);

    struct sockaddr_in* serverptr = &server;

    // bind socket to address
    if(bind(sock,(struct sockaddr *)serverptr,sizeof(server)))
        perror_exit("bind");

    // Listen for connections
    if(listen(sock,MAX_CON)<0)
        perror_exit("listen");

    // accept connections etc
    while(1){

        // accept
        int newSocket;
        if((newSocket = accept(sock,NULL,NULL))<0)
            perror_exit("accept");

        
        // create thread and assign to communication thread
        pthread_t newThread;

        // insert mutex to map
        pthread_mutex_t* mx = new pthread_mutex_t;
        pthread_mutex_init(mx,0);
        workersMutexes.insert(pair<int,pthread_mutex_t*>(newSocket,mx));

        pthread_create(&newThread,0,communication_thread,(void*)newSocket);
        
    }

}
