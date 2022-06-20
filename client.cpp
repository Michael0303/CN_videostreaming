#include <iostream>
#include <string>
#include <sstream>
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h> 
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <signal.h>
#include "opencv2/opencv.hpp"

#define BUFF_SIZE 1024
#define ERR_EXIT(a){ perror(a); exit(1); }
using namespace std;
using namespace cv;

string END = "END";
string OK = "OK";
string NO = "NO";

void allsend(char *buf, int sockfd){
    char *ptr = buf;
    int count = 0;
    while (count < BUFF_SIZE){
        int size = send(sockfd, ptr, BUFF_SIZE - count, 0);
        ptr += size;
        count += size;
    }
    return;
}

void allrecv(char *buf, int sockfd){
    char *ptr = buf;
    int count = 0;
    while (count < BUFF_SIZE){
        int size = recv(sockfd, ptr, BUFF_SIZE - count, 0);
        ptr += size;
        count += size;
    }
    return;
}

void handlels(int sockfd){
    char buf[BUFF_SIZE] = {};
    bzero(buf, BUFF_SIZE);
    string tmp = "ls";
    strncpy(buf, tmp.c_str(), tmp.length());
    allsend(buf, sockfd);
    while (1){
        bzero(buf, BUFF_SIZE);
        allrecv(buf, sockfd);
        if (strncmp(buf, END.c_str(), END.length()) == 0){
            break;
        }
        bzero(buf, BUFF_SIZE);
        allrecv(buf, sockfd);
        string filename = buf;
        cout << filename << endl;
    }
    return;
}

void handleput(int sockfd, string filename){
    char buf[BUFF_SIZE] = {};
    if (access(filename.c_str(), 0) != 0){
        cout << "The " << filename << " doesn't exist." << endl;
    } else {
        bzero(buf, BUFF_SIZE);
        string tmp = "put";
        strncpy(buf, tmp.c_str(), tmp.length());
        allsend(buf, sockfd);
        
        bzero(buf, BUFF_SIZE);
        strncpy(buf, filename.c_str(), filename.length());
        allsend(buf, sockfd);

        cout << "puting " << filename << "......" << endl;
        FILE *fp = fopen(filename.c_str(), "rb");
        while (!feof(fp)){
            bzero(buf, BUFF_SIZE);
            int readbytes = fread(buf, 1, BUFF_SIZE, fp);
            char length[BUFF_SIZE] = {};
            tmp = to_string(readbytes);
            strncpy(length, tmp.c_str(), tmp.length());
            allsend(length, sockfd);
            allsend(buf, sockfd);
        }
        bzero(buf, BUFF_SIZE);
        strncpy(buf, END.c_str(), END.length());
        allsend(buf, sockfd);
        fclose(fp);
        //cout << "puting complete" << endl;
    }
    return;
}

void handleget(int sockfd, string filename){
    char buf[BUFF_SIZE] = {};
    bzero(buf, BUFF_SIZE);
    string tmp = "get";
    strncpy(buf, tmp.c_str(), tmp.length());
    allsend(buf, sockfd);

    bzero(buf, BUFF_SIZE);
    strncpy(buf, filename.c_str(), filename.length());
    allsend(buf, sockfd);

    bzero(buf, BUFF_SIZE);
    allrecv(buf, sockfd);
    tmp = buf;
    if (tmp == NO){
        cout << "The " << filename << " doesn't exist." << endl;
    } else{
        cout << "geting " << filename << "......" << endl;
        FILE *fp = fopen(filename.c_str(), "wb");
        while (1){
            char length[BUFF_SIZE] = {};
            allrecv(length, sockfd);
            if (strncmp(length, END.c_str(), END.length()) == 0){
                break;
            }
            int size = atoi(length);
            bzero(buf, BUFF_SIZE);
            allrecv(buf, sockfd);
            int writebytes = fwrite(buf, 1, size, fp);
            //cout << "write " << writebytes << " bytes" << endl;
        }
        fclose(fp);
        //cout << "geting complete" << endl;
    }
    return;
}

void handleplay(int sockfd, string filename){
    // check .mpg
    int length = filename.rfind(".");
    if (length < 0){
        cout << "The " << filename << " is not a mpg file." << endl;
        return;
    }
    //cout << "length = " << length << endl;
    string type;
    type = type.assign(filename, length, filename.length());
    //cout << "type = " << type << endl;
    if (type != ".mpg"){
        //cout << type << endl;
        cout << "The " << filename << " is not a mpg file." << endl;
        return;
    }
    char buf[BUFF_SIZE] = {};
    bzero(buf, BUFF_SIZE);
    string tmp = "play";
    strncpy(buf, tmp.c_str(), tmp.length());
    allsend(buf, sockfd);

    // check NOFILE
    bzero(buf, BUFF_SIZE);
    strncpy(buf, filename.c_str(), filename.length());
    allsend(buf, sockfd);
    bzero(buf, BUFF_SIZE);
    allrecv(buf, sockfd);
    tmp = buf;
    if (tmp == NO){
        cout << "The " << filename << " doesn't exist." << endl;
        return;
    }

    Mat client_img;
    bzero(buf, BUFF_SIZE);
    allrecv(buf, sockfd);
    int width = atoi(buf);
    bzero(buf, BUFF_SIZE);
    allrecv(buf, sockfd);
    int height = atoi(buf);
    //cout << "width = " << width << " height = " << height << endl;
    // Allocate container to load frames    
    client_img = Mat::zeros(height, width, CV_8UC3);
    
    // Ensure the memory is continuous (for efficiency issue.)
    if(!client_img.isContinuous()){
         client_img = client_img.clone();
    }
    cout << "playing the video..." << endl;
    
    while(1){
        // get imgSize
        bzero(buf, BUFF_SIZE);
        allrecv(buf, sockfd);
        tmp = buf;
        if (tmp == END){
            break;
        }
        //cout << "recv imgsize = \n" << tmp << endl;
        int imgSize = atoi(tmp.c_str());
        if (imgSize == 0){
            //cout << "error" << endl;
            break;
        }
    
        // Allocate a buffer to load the frame (there would be 2 buffers in the world of the Internet)
        uchar buffer[imgSize];
        int count = 0;
        uchar *iptr = client_img.data;
        while (count < imgSize){
            bzero(buffer, imgSize);
            int size = recv(sockfd, buffer, imgSize, 0);
            //cout << "recv " << size << " bytes" << endl;
            memcpy(iptr, buffer, size);
            iptr += size;
            count += size;
        }
      
        // show the frame 
        imshow("Video", client_img);  
        
        // Press ESC on keyboard to exit
        // Notice: this part is necessary due to openCV's design.
        // waitKey function means a delay to get the next frame. You can change the value of delay to see what will happen
        char c = (char)waitKey(33.3333);
        if(c==27){
            bzero(buf, BUFF_SIZE);
            strncpy(buf, NO.c_str(), NO.length());
            allsend(buf, sockfd);
            break;
        }
        bzero(buf, BUFF_SIZE);
        strncpy(buf, OK.c_str(), OK.length());
        allsend(buf, sockfd);
    }
    //cout << "end send" << endl;
	destroyAllWindows();
	return;
}

int main(int argc , char *argv[]){
    int sockfd, read_byte;
    struct sockaddr_in addr;
    char buffer[BUFF_SIZE] = {};
    int id = atoi(argv[1]);

    char IP[BUFF_SIZE] = {};
    char *portptr;
    int port = 8787;
    strncpy(IP, "127.0.0.1", 9);

    if (argc > 2){
        string arg = argv[2];
        //cout << arg << endl;
        int length = arg.rfind(":");
        if (length < 0){
            cout << "invalid IP:port form" << endl;
            return 0;
        }
        string ip;
        ip = ip.assign(arg, 0, length);
        strncpy(IP, ip.c_str(), ip.length());
        string portstr;
        portstr = portstr.assign(arg, length + 1, arg.length());
        //cout << portstr << endl;
        port = atoi(portstr.c_str());
        //cout << "IP = " << ip << " port = " << port << endl;
    }

    // Get socket file descriptor
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        ERR_EXIT("socket failed\n");
    }

    // Set server address
    bzero(&addr,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(IP);
    addr.sin_port = htons(port);

    /*mkdir*/
    char dirname[BUFF_SIZE] = {};
    sprintf(dirname, "b08902018_%d_client_folder", id);
    mkdir(dirname, 0777);
    chdir(dirname);

    // Connect to the server
    if(connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        ERR_EXIT("connect failed\n");
    }

    while (1){
        string input, command, filename;
        char buf[BUFF_SIZE] = {};
        getline(cin, input);

        stringstream inputsplit(input);
        getline(inputsplit, command, ' ');
        //cout << "commamd is: " << command << endl;

        if (command == "ls"){          
            handlels(sockfd);
        } else if (command == "put"){
            while (getline(inputsplit, filename, ' '))
                handleput(sockfd, filename);               
        } else if (command == "get"){
            while (getline(inputsplit, filename, ' '))
                handleget(sockfd, filename); 
        } else if (command == "play"){
            getline(inputsplit, filename, ' ');
            handleplay(sockfd, filename);
        } else {
            //error command
            cout << "Command not found." << endl;
        }
    }   
    close(sockfd);
    return 0;
}