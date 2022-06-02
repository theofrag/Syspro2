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
#include <vector>

#define  MAX_CON 40
#define  POOL_SIZE 10
#define WORKERS 20
#define BLOCK_SIZE 512

#include <iostream>

using namespace std;

vector<char*> dirContents(char* path);

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
    if(read(comSocket,dirName,256)<0){

        //ISO C++ forbids converting a string constant to ‘char*’
        char error[] = "read";
        perror_exit(error);
    }
 
    cout<<"[Thread: "<<pthread_self()<<"]: About to scan directory "<< dirName<<endl;


    // take filenames and their paths
    vector<char*> contents = dirContents(dirName);
    

    // add total number to map
    int num = contents.size(); 
    mapNumbers.insert(pair<int,int>(comSocket,num));


    // add file namesto queue

    for(int i=0;i<contents.size();i++){
        
        char temp[300];
        snprintf(temp,300,"%s %d",contents[i],comSocket);
        place(&pool,temp);
        pthread_cond_signal(&cond_nonempty);
    }
      
}


void* worker_thread(void* blockSize){

    bool dead;
    int blocksiz = (long)blockSize;

    while(1){
        dead = false;

        // consume name from queue
        char* consumed = obtain(&pool);
        pthread_cond_signal(&cond_nonfull);

        // retrieve filename and socketno
        char* filename;
        char* socketno;
        char* rest = NULL;
        
        // strtok_r is thread safe
        filename = strtok_r(consumed," ",&rest);
        socketno = strtok_r(NULL," ",&rest);

        int sock = atoi(socketno);

        pthread_mutex_t * mx = workersMutexes.at(sock);

        // down socket mutex so no writers can write simultaneously in the socket
        pthread_mutex_lock(mx);

        // send filename with '\n' to client   
        if( write(sock,filename,strlen(filename))<0 ){
            char error[] = "write";
            perror_exit(error);
        }
        if( write(sock,"\n",strlen("\n"))<0 ){
            char error[] = "write";
            perror_exit(error);
        
        }

        // open file
        FILE* file_fp;
        if ((file_fp = fopen(filename,"r"))== NULL){
            char error[] = "fopen";
            perror_exit(error);
        
        }
        

        cout<<"[Thread: "<<pthread_self()<<"]: About to read file "<< filename<<endl;


        // read data of blocksize
        // read from file and write it to socket
        char fromfgets[BLOCK_SIZE];
        while(fgets(fromfgets,BLOCK_SIZE+1,file_fp)){
            write(sock,fromfgets,strlen(fromfgets));
        }


        write(sock,"\n",1);
        write(sock,"ENDOFFILE\n",10);

        // reduce counter of filenames
        int n = mapNumbers.at(sock);
        n--;

        if(n==0){
            
            // write CONTERM so client know that all files have been send
            char terminate[] = "CONTERM\n";

            if( write(sock,terminate,strlen(terminate))<0 ){
                char error[] = "write";
                perror_exit(error);
            }

            dead = true;

        }else{
            mapNumbers[sock] = n;
        }

        // up socket semaphore
        pthread_mutex_unlock(mx);
        
        // destroy semaphore if all files sent
        if(dead == true){

            // erase data from map
            workersMutexes.erase(sock);
            mapNumbers.erase(sock);

            //close socket from server side
            close(sock);    
        }
    }
}


int block_size;
int main(int argc, char** argv){

    int sock;
    int port = 12500;
    int workers = WORKERS;
    int queue_size;
    
    int thread_pool_size;


    // control arguments
    if(argc != 9){
        cout<<"Give right number of arguments"<<endl;
        return -1;
    }else{
        for(int i=1;i<argc;i=i+2){
            if(strcmp(argv[i],"-p")==0){
                port = atoi(argv[i+1]);
            }else if(strcmp(argv[i],"-q")==0){
                queue_size = atoi(argv[i+1]);
            }else if(strcmp(argv[i],"-b")==0){
                block_size = atoi(argv[i+1]);
            }else if(strcmp(argv[i],"-s")==0){
                thread_pool_size = atoi(argv[i+1]);
            }else{
                cout<<"Give right arguments"<<endl;
                return -1;
            }
        }
    }

    // print starting
    cout<<"Server's parameters are:"<<endl;
    cout<<"port: "<<port<<endl;
    cout<<"thread_pool_size: "<<thread_pool_size<<endl;
    cout<<"queue_size: "<<queue_size<<endl;
    cout<<"block_size: "<<block_size<<endl;



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
    if((sock = socket(AF_INET,SOCK_STREAM,0)) == -1 ){
        char error[] = "Failed to create socket";
        perror_exit(error);   
    }
    
    struct sockaddr_in server,client;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);

    struct sockaddr_in* serverptr = &server;

    struct sockaddr *clientptr=(struct sockaddr *)&client;

    // bind socket to address
    if(bind(sock,(struct sockaddr *)serverptr,sizeof(server))){
        char error[] = "bind";
        perror_exit(error); 
    }

    // Listen for connections
    if(listen(sock,MAX_CON)<0){
        char error[] = "listen";
        perror_exit(error); 
    }

    cout<<"Server was successfully initialized..."<<endl;
    cout<<"Listening for connections to port "<<port<<endl;

    // accept connections etc
    while(1){   
        
        socklen_t clientlen=sizeof(client);

        // accept
        int newSocket;
        if((newSocket = accept(sock,clientptr,&clientlen))<0){
            char error[] = "accept";
            perror_exit(error); 
        }

        // take client name
        struct hostent *rem;
        if ((rem = gethostbyaddr((char *) &client.sin_addr.s_addr, sizeof(client.sin_addr.s_addr), client.sin_family)) == NULL){
            char error[] = "gethostbyaddr";
            perror_exit(error);
        }
        cout <<"Accepted connection from "<< rem->h_name<<endl;

        // insert mutex to map
        pthread_mutex_t* mx = new pthread_mutex_t;
        pthread_mutex_init(mx,0);
        workersMutexes.insert(pair<int,pthread_mutex_t*>(newSocket,mx));

        // create thread and assign to communication thread
        pthread_t newThread;
        pthread_create(&newThread,0,communication_thread,(void*)newSocket);

        
    }

}


// helper functions

vector<char*> dirContents(char* path){

    vector<char*> contents;

    DIR * d = opendir(path);

    if(d==NULL){
        perror("opendir");
        exit(2);
    }

    struct dirent * dir; // for the directory entries

    errno = 0;  // set errno to 0 before

    

    while ((dir = readdir(d))!=NULL){
        if(dir-> d_type != DT_DIR){
            char* entry = new char[sizeof(char)*(strlen(dir->d_name)+ strlen(path))+1];
            snprintf(entry,257,"%s/%s",path,dir->d_name);
            contents.push_back(entry);
        }
        else if(dir -> d_type == DT_DIR && strcmp(dir->d_name,".")!=0 && strcmp(dir->d_name,"..")!=0 ){ // if it is directory

            char dirpath[256];
            snprintf(dirpath,257,"%s/%s",path,dir->d_name);
            vector<char*> toappend = dirContents(dirpath);
            contents.insert(contents.end(),toappend.begin(),toappend.end());
        }
    }

    closedir(d);
    return contents;

}