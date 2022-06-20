#include <iostream>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h> 
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/stat.h>
#include <signal.h>
#include "opencv2/opencv.hpp"

#define BUFF_SIZE 1024
#define ERR_EXIT(a){ perror(a); exit(1); }
#define MAXTHREAD 256

using namespace std;
using namespace cv;

string END = "END";
string OK = "OK";
string NO = "NO";

typedef struct client{
    int tid;
    FILE *openfp;
    DIR *opendir;
    int sockfd;
} Client;

typedef struct thread{
    pthread_t tid;
    Client c;
    int used;
} Thread;

int allsend(char *buf, int sockfd){
    char *ptr = buf;
    int count = 0;
    while (count < BUFF_SIZE){
        int size = send(sockfd, ptr, BUFF_SIZE - count, 0);
        if (size <= 0)
            return -1;
        ptr += size;
        count += size;
    }
    return 0;
}

int allrecv(char *buf, int sockfd){
    char *ptr = buf;
    int count = 0;
    while (count < BUFF_SIZE){
        int size = recv(sockfd, ptr, BUFF_SIZE - count, 0);
        if (size <= 0)
            return -1;
        ptr += size;
        count += size;
    }
    return 0;
}


/*pthread*/
Thread t[MAXTHREAD];

void sighandler(int sig){
    return;
}

void ls(Client *c){
    char buf[BUFF_SIZE] = {};
    c->opendir = opendir(".");
    struct dirent *dir;
    if (c->opendir){
        while ((dir = readdir(c->opendir)) != NULL){
            bzero(buf, BUFF_SIZE);
            string filename = dir->d_name;
            if (filename == "." | filename == "..")
                continue;
            strncpy(buf, OK.c_str(), OK.length());
            allsend(buf, c->sockfd);
            bzero(buf, BUFF_SIZE);
            strncpy(buf, filename.c_str(), filename.length());
            allsend(buf, c->sockfd);
        }
        closedir(c->opendir);
    }
    bzero(buf, BUFF_SIZE);
    strncpy(buf, END.c_str(), END.length());
    allsend(buf, c->sockfd);
    return;
}

void putfile(Client *c){
    char buf[BUFF_SIZE] = {};
    if (allrecv(buf, c->sockfd) < 0){
        return;
    }
    string filename = buf;
    c->openfp = fopen(filename.c_str(), "wb");
    while (1){
        char length[BUFF_SIZE] = {};
        if (allrecv(length, c->sockfd) < 0){
            break;
        }
        if (strncmp(length, END.c_str(), sizeof(END.c_str())) == 0){
            break;
        }
        int size = atoi(length);
        bzero(buf, BUFF_SIZE);
        if (allrecv(buf, c->sockfd) < 0){
            break;
        }
        fwrite(buf, 1, size, c->openfp);
    }
    fclose(c->openfp);
    return;
}

void getfile(Client *c){
    char buf[BUFF_SIZE] = {};
    if (allrecv(buf, c->sockfd) < 0){
        return;
    } 
    char filename[BUFF_SIZE] = {};
    strncpy(filename, buf, BUFF_SIZE);
    if (access(filename, 0) != 0){
        bzero(buf, BUFF_SIZE);
        strncpy(buf, NO.c_str(), NO.length());
        allsend(buf, c->sockfd);
    } else{
        bzero(buf, BUFF_SIZE);
        strncpy(buf, OK.c_str(), OK.length());
        allsend(buf, c->sockfd);

        c->openfp = fopen(filename, "rb");
        while (!feof(c->openfp)){
            bzero(buf, BUFF_SIZE);
            int readbytes = fread(buf, 1, BUFF_SIZE, c->openfp);
            //cout << "read " << readbytes << " bytes" << endl;
            char length[BUFF_SIZE] = {};
            string tmp = to_string(readbytes);
            strncpy(length, tmp.c_str(), tmp.length());
            allsend(length, c->sockfd);
            allsend(buf, c->sockfd);
        }
        bzero(buf, BUFF_SIZE);
        strncpy(buf, END.c_str(), END.length());
        allsend(buf, c->sockfd);
        fclose(c->openfp);
    }
    return;
}   

void plaympg(Client *c){
    char buf[BUFF_SIZE] = {};
    if (allrecv(buf, c->sockfd) < 0){
        return;
    } 
    char filename[BUFF_SIZE] = {};
    strncpy(filename, buf, BUFF_SIZE);
    if (access(filename, 0) != 0){
        bzero(buf, BUFF_SIZE);
        strncpy(buf, NO.c_str(), NO.length());
        allsend(buf, c->sockfd);
    } else{
        bzero(buf, BUFF_SIZE);
        strncpy(buf, OK.c_str(), OK.length());
        allsend(buf, c->sockfd);
    
        Mat server_img;
        VideoCapture cap(filename);
        
        // Get the resolution of the video
        int width = cap.get(CV_CAP_PROP_FRAME_WIDTH);
        int height = cap.get(CV_CAP_PROP_FRAME_HEIGHT);
        string tmp;
        bzero(buf, BUFF_SIZE);
        tmp = to_string(width);
        strncpy(buf, tmp.c_str(), tmp.length());
        allsend(buf, c->sockfd);
        bzero(buf, BUFF_SIZE);
        tmp = to_string(height);
        strncpy(buf, tmp.c_str(), tmp.length());
        allsend(buf, c->sockfd);
        // Allocate container to load frames 
        server_img = Mat::zeros(height, width, CV_8UC3);    
    
        // Ensure the memory is continuous (for efficiency issue.)
        if(!server_img.isContinuous()){
            server_img = server_img.clone();
        }
        //cout << "start send" << endl;
        
        while(1){
            // Get a frame from the video to the container of the server.
            cap >> server_img;   

            // Get the size of a frame in bytes 
            int imgSize = server_img.total() * server_img.elemSize();
            if (imgSize > 0){
                // send imgSize
                bzero(buf, BUFF_SIZE);
                tmp = to_string(imgSize);
                strncpy(buf, tmp.c_str(), tmp.length());
                allsend(buf, c->sockfd);
                //cout << "send imgsize = \n" << buf << endl;
            } else{
                //cout << "end of video" << endl;
                bzero(buf, BUFF_SIZE);
                strncpy(buf, END.c_str(), END.length());
                allsend(buf, c->sockfd);
                break;
            }
            // Allocate a buffer to load the frame (there would be 2 buffers in the world of the Internet)
            uchar buffer[imgSize];
            // Copy a frame to the buffer
            memcpy(buffer, server_img.data, imgSize);
            uchar *iptr = buffer;
            int count = 0;
            while (count < imgSize){
                int size = send(c->sockfd, iptr, imgSize, 0);
                //cout << "send " << size << " bytes" << endl;
                iptr += size;
                count += size;
            }
            bzero(buf, BUFF_SIZE);
            if (allrecv(buf, c->sockfd) < 0)
                break;
            tmp = buf;
            if (tmp != OK){
                //cout << "close" << endl;
                break;
            }
        }
        //cout << "end send" << endl;
        cap.release();
    }
	return;
}

void *threadfunction(void *data){
    Client *c = (Client *)data;
    while (1){
        char buf[BUFF_SIZE] = {};
        if (allrecv(buf, c->sockfd) < 0){
            cout << "Client fd = " << c->sockfd << " disconnected." << endl;
            break;
        }
        if (strncmp(buf, "ls", 2) == 0){
            //cout << "Client fd = " << c->sockfd << " ls" << endl;
            ls(c);
            //cout << "ls complete" << endl;
        } else if (strncmp(buf, "put", 3) == 0){
            //cout << "Client fd = " << c->sockfd << " put" << endl;
            putfile(c);
            //cout << "put complete" << endl;
        } else if (strncmp(buf, "get", 3) == 0){
            //cout << "Client fd = " << c->sockfd << " get" << endl;
            getfile(c);
            //cout << "get complete" << endl;
        } else if (strncmp(buf, "play", 4) == 0){
            //cout << "Client fd = " << c->sockfd << " play" << endl;
            plaympg(c);
            //cout << "play complete" << endl;
        }
    }
    t[c->tid].used = -1;
    close(c->sockfd);
    cout << "close socket fd = " << c->sockfd << endl;
    pthread_exit((void*)NULL);
}

int main(int argc, char *argv[]){
    int server_sockfd, client_sockfd, write_byte;
    struct sockaddr_in server_addr, client_addr;
    int client_addr_len = sizeof(client_addr);
    int port = atoi(argv[1]);

    // Get socket file descriptor
    if((server_sockfd = socket(AF_INET , SOCK_STREAM , 0)) < 0){
        ERR_EXIT("socket failed\n")
    }

    // Set server address information
    bzero(&server_addr, sizeof(server_addr)); // erase the data
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);
    
    // Bind the server file descriptor to the server address
    if(bind(server_sockfd, (struct sockaddr *)&server_addr , sizeof(server_addr)) < 0){
        ERR_EXIT("bind failed\n");
    }
        
    // Listen on the server file descriptor
    if(listen(server_sockfd , 3) < 0){
        ERR_EXIT("listen failed\n");
    }
    
    /*mkdir*/
    mkdir("b08902018_server_folder", 0777);
    chdir("./b08902018_server_folder");

    for (int i = 0; i < MAXTHREAD; i++)
        t[i].used = 0;
      
    cout << "server start" << endl;

    /*signal*/
    signal(SIGPIPE, sighandler);


    while (1){
        if((client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, (socklen_t*)&client_addr_len)) < 0){
            ERR_EXIT("accept failed\n");
        }
        printf("new connection: fd = %d\n", client_sockfd);
        for (int i = 0; i < MAXTHREAD; i++){
            if (t[i].used == 0){
                t[i].used = 1;
                t[i].c.sockfd = client_sockfd;
                t[i].c.tid = i;
                Client *ptr = &t[i].c;
                pthread_create(&t[i].tid, NULL, threadfunction, ptr);
                break;
            }

        }
        for (int i = 0; i < MAXTHREAD; i++)
            if (t[i].used == -1){
                pthread_join(t[i].tid, NULL);
                t[i].used = 0;
            }

    }
    close(server_sockfd);
    return 0;
}