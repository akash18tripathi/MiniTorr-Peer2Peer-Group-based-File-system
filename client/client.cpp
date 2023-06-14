#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <pthread.h>
#include <fcntl.h>
#include <openssl/sha.h>
#include <bits/stdc++.h>
#include <mutex>


#define chunk_size 524288
#define chunk_buffer 16384


using namespace std;

mutex m;
bool loggedIn;
unordered_map<string,string> chunksPresent;
unordered_map<string,unordered_map<int,vector<int>>> fileToPortToChunks;
unordered_map<string,unordered_map<int,string>> fileToPortToChunkBit;

struct arg_struct {
    int port;
    string fileName;
};

int asServerPort;
char * peerIP;
int trackerPort;
char * trackerIP;

bool sortByListSize(pair<int, vector<int>> a,pair<int, vector<int>> b){
    return a.second.size()<b.second.size();
}

vector<string> makeSplit(string s, char delim)
{
    vector<string> ans;
    int n = s.length();
    int start = 0, end = 0;
    for (int i = 0; i < n; i++)
    {
        if (s[i] == delim)
        {
            ans.push_back(s.substr(start, end - start));
            start = i + 1;
            end = i + 1;
        }
        else
        {
            end++;
        }
    }
    ans.push_back(s.substr(start, end - start));
    return ans;
}

int findNumberOfChunks(string fileName){
    ifstream testFile(fileName, ios::binary);
    testFile.seekg (0, ios::end);
    int end = testFile.tellg();
    int numChunks = ceil((double)end/(double)chunk_size);
    return numChunks;
}


void* runAsServer(void *arg){

    cout << "Thread start...\n";
    char const* ip = peerIP;

    int server_sock , client_sock;

    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    char buffer[16384];
    int n;

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(server_sock<0){
        perror("[-]Socket Error");
        exit(1);
    }
    printf("[-]TCP server socket created for thread\n");

    memset(&server_addr, '\0' ,sizeof(server_addr));
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(asServerPort);
    server_addr.sin_addr.s_addr = inet_addr(ip);

    n = bind(server_sock,(struct sockaddr*)&server_addr, sizeof(server_addr));
    int opt=1;
    if(n<0){
        if(setsockopt(server_sock, SOL_SOCKET,
                   SO_REUSEADDR | SO_REUSEPORT, &opt,
                   sizeof(opt))){
                    perror("[-]Bind error");
        exit(1);
                   }
        ;
        
    }
    printf("[-]Bind to the port number: %d , for thread\n",asServerPort);
    listen(server_sock,10000);
    printf("Listening from thread server.....\n");
    while(1){
        addr_size = sizeof(client_addr);    
        cout << "Waiting..."<<endl;
        client_sock = accept(server_sock,(struct sockaddr*)&client_addr,&addr_size);
        printf("[-]Client connected from thread server\n");

        bzero(buffer,16384);
        recv(client_sock, buffer, 16384,0);
        printf("Client : %s\n", buffer);
        string response = string(buffer);
        vector<string> input = makeSplit(response,' ');
        if(input[0]=="download_init"){

            //Recieved file sharing request- number of chunks required
            int times = stoi(input[2]);
            char chunkBuffer[chunk_buffer];
            int fd = open(&input[1][0],O_RDONLY);
            
            if(fd==-1){
                perror("[-]Error while opening file");
                exit(1);
            }
            unsigned char hash[SHA_DIGEST_LENGTH];
            unsigned char chunkSHA[SHA_DIGEST_LENGTH];
            for(int i=0;i<times;i++){
                // Recieve Chunk Requests
                bzero(chunkBuffer,chunk_buffer);
                recv(client_sock, chunkBuffer, chunk_buffer,0);
                printf("Client : %s\n", chunkBuffer);
                vector<string> req = makeSplit(string(chunkBuffer),' ');

                
                int chunkNum = stoi(req[1]);
                string shaString="";
                for(int j=0;j<32;j++){
                    bzero(chunkBuffer,chunk_buffer);
                    pread(fd,chunkBuffer,sizeof(chunkBuffer),chunkNum*chunk_size+j*chunk_buffer);
                    bzero(hash,SHA_DIGEST_LENGTH); 
                    SHA1((unsigned char *)chunkBuffer, sizeof(chunkBuffer) - 1, hash);
                    shaString.append((char *)hash);
                    if(send(client_sock,chunkBuffer,sizeof(chunkBuffer),0) == -1){
                        perror("[-]Error while sending data");
                        exit(1);
                    }
                }
                bzero(chunkSHA,SHA_DIGEST_LENGTH); // == 20
                SHA1((unsigned char *)&shaString[0], sizeof(&shaString[0])- 1, chunkSHA);
                //Send chunk SHAA
                send(client_sock,chunkSHA,sizeof(chunkSHA),0);
            }

            close(client_sock);
        }else if(input[0]=="chunk_present"){

            string chunkInfo = chunksPresent[input[1]];
            string res="having "+ chunkInfo;
            cout << res <<endl;
            bzero(buffer,16384);
            strcpy(buffer,&res[0]);
            printf("Peer : %s\n",buffer);
            send(client_sock, buffer, sizeof(buffer), 0);
            close(client_sock);

        }else if(input[0]=="join_group"){
            string res;
            cout << "Type accept or reject: ";
            getline(cin,res);
            cout << "Response is : "<< res<<endl;
            if(res=="accept"){
                bzero(buffer,16384);
                strcpy(buffer,&res[0]);
                printf("Admin : %s\n",buffer);
                send(client_sock, buffer, sizeof(buffer), 0);
                close(client_sock);
            }else{
                bzero(buffer,16384);
                strcpy(buffer,"reject");
                printf("Admin : %s\n",buffer);
                send(client_sock, buffer, sizeof(buffer), 0);
                close(client_sock);
            }
            
        }
        
    }

    printf("[+]Client Disconnected from thread, thread ending...\n\n");

}

void contactPeersToGatherInfo(int port,string filename){
    cout << "Thread Started for Gathering Chunk Information...\n";
    char const* ip = "127.0.0.1";
    
    int sock;

    struct sockaddr_in addr;
    socklen_t addr_size;
    char buffer[16384];
    int n;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock<0){
        perror("[-]Socket Error");
        exit(1);
    }
    printf("[-]TCP server socket created from Download thread.\n");
    memset(&addr, '\0' ,sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    int conn = connect(sock,(struct sockaddr*)&addr,sizeof(addr));
    if(conn<0){
        perror("[-]Error in connecting....");
        exit(1);
    }else{
        printf("[-]Connected to server!\n");
    }
    

    string msg = "chunk_present "+filename;
    bzero(buffer,16384);
    strcpy(buffer,&msg[0]);
    printf("Client : %s\n",buffer);
    send(sock,buffer,sizeof(buffer),0);
    bzero(buffer,16384);
    recv(sock, buffer, 16384,0);
    printf("Peer : %s\n", buffer);
    vector<string> res = makeSplit(string(buffer),' ');
    if(res[0]=="having"){
        string bitstr=res[1];
        vector<int> v;
        for(int i=0;i<bitstr.length();i++){
            if(bitstr[i]=='1'){
                v.push_back(i);
            }
        }
        m.lock();
        fileToPortToChunks[filename][port]=v;
        fileToPortToChunkBit[filename][port]=bitstr;
        m.unlock();
    }

    
    close(sock);
    printf("[+]Disconnected from server thread\n\n");
}

void contactPeersAndDownload(int port,string filename,vector<int> chunksReq,string dstPath){
    cout << "Thread Started for Gathering Chunk Download...\n";
    char const* ip = "127.0.0.1";
    int sock;

    struct sockaddr_in addr;
    socklen_t addr_size;
    char buffer[16384];
    int n;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock<0){
        perror("[-]Socket Error");
        exit(1);
    }
    printf("[-]TCP server socket created from Download thread.\n");

    memset(&addr, '\0' ,sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    int conn = connect(sock,(struct sockaddr*)&addr,sizeof(addr));
    if(conn<0){
        perror("[-]Error in connecting....");
        exit(1);
    }else{
        printf("[-]Connected to server!\n");
    }

    //initialize download command
    string snd = "download_init "+filename+" "+ to_string(chunksReq.size());
    bzero(buffer,16384);
    strcpy(buffer,&snd[0]);
    printf("Client : %s\n",buffer);
    send(sock,buffer,sizeof(buffer),0);
    string fileDir= dstPath+filename;
    cout << fileDir<<endl;
    int fd = open(&fileDir[0],O_WRONLY | O_CREAT,0777);
    
    if(fd==-1){
        perror("[-]Error in creating file.");
        exit(1);
    }
    string fileSHA=""; 
    string recievedSHA="";   
    char chunkBuffer[chunk_buffer];
    unsigned char hash[SHA_DIGEST_LENGTH]; 
    unsigned char rChunkSHA[SHA_DIGEST_LENGTH];
    unsigned char chunkSHA[SHA_DIGEST_LENGTH];
    for(int i=0;i<chunksReq.size();i++){
        
        string shaString="";
        snd = filename+" "+to_string(chunksReq[i]);
        bzero(chunkBuffer,chunk_buffer);
        strcpy(chunkBuffer,&snd[0]);
        printf("Client : %s\n",chunkBuffer);
        send(sock,chunkBuffer,sizeof(chunkBuffer),0);
        for(int j=0;j<32;j++){
            bzero(chunkBuffer,chunk_buffer);
            int n = read(sock,chunkBuffer,chunk_buffer);
            
            if(n<=0){
                cout << "Error while recieving";
            }
            // == 20
            bzero(hash,SHA_DIGEST_LENGTH); // == 20
            SHA1((unsigned char *)chunkBuffer, sizeof(chunkBuffer) - 1, hash);
            shaString.append((char *)hash);
            
            pwrite(fd,chunkBuffer,sizeof(chunkBuffer),chunksReq[i]*chunk_size+j*chunk_buffer);
            bzero(chunkBuffer,chunk_buffer);
        
        }

        //Recieve SHA from Sender
        bzero(rChunkSHA,SHA_DIGEST_LENGTH);
        recv(sock, rChunkSHA, sizeof(rChunkSHA),0);
        recievedSHA.append((char *)rChunkSHA);

        //Calculate SHA
        bzero(chunkSHA,SHA_DIGEST_LENGTH);// == 20
        SHA1((unsigned char *)&shaString[0] ,sizeof(&shaString[0]) - 1, chunkSHA);
        fileSHA.append((char *)chunkSHA);

        //Compare Recieved vs Calculated SHA
        if(strcmp((char *)rChunkSHA,(char *)chunkSHA)==0){
            printf("SHA of chunk %d : Fine\n",chunksReq[i]);
        }else{
            printf("SHA of chunk %d : Corrupt\n",chunksReq[i]);
        }
    }
    if(recievedSHA==fileSHA){
        printf("SHA of File is Fine\n");

    }else{
        printf("SHA not same: File Corrupted!!!!\n");
    }
     

    close(sock);
    printf("[+]Disconnected from server thread\n\n");
}

void peerCommands(int sock,string inp){
    
    char buffer[16384];
    //recieve response from tracker
    bzero(buffer,16384);
    recv(sock,buffer,16384,0);
    printf("Tracker : %s\n",buffer);
    string response = string(buffer);
    if(response=="Invalid"){
        return;
    }
    vector<string> input = makeSplit(inp,' ');
    vector<string> resVec = makeSplit(response,' ');
    if(input[0]=="create_user"){
        if(response=="Exists"){
            cout << "===============User Already Exists============\n";
            return;
        }else if(response=="Success"){
            cout << "===============User Created ============\n";
            return;
        }
    }else if(input[0]=="login"){
        if(response=="Already Logged In"){
            cout << "===============Already Logged In============\n";
            return;
        }else if(response=="Absent"){
            cout << "==============User doesn't exists============\n";
            return;
        }else if(response=="Success"){
            loggedIn=true;
            bzero(buffer,16384);
            strcpy(buffer,&to_string(asServerPort)[0]);
            printf("Client : %s\n",buffer);
            send(sock, buffer, sizeof(buffer), 0);
            cout << "===============Port Sent successfully============\n";
            cout << "===============Login Success=====================\n";
            return;
        }
    }else if(input[0]=="logout"){
        if(response=="Success"){
            loggedIn=false;     
            cout << "===============Logged Out Successfully!==============\n";
        }

    }else if(input[0]=="create_group"){
        if(response=="Success"){
            cout << "==============Group Created Successfully!===========\n";
            return;
        }else if(response=="Exists"){
            cout << "==============Group Already Exists===================\n";
            return;
        }

    }else if(input[0]=="join_group"){
        if(response=="Success"){
            cout << "================Request in Queue==============\n";
            return;
        }else if(response=="Already a Member"){
            cout << "==============Already a Member===================\n";
            return;
        }
        else if(response=="Group Doesnt Exist"){
            cout << "==============Group Doesn't Exists===============\n";
            return;
        }else if(response=="Already In Queue"){
            cout << "==============Request Already in Queue===============\n";
            return;
        }

    }else if(input[0]=="accept_request"){
        if(response=="Success"){
            cout << "================Request Accepted==============\n";
            return;
        }else if(response=="No Pending Request"){
            cout << "===========No Pending Requests in Queue===========\n";
            return;
        }
        else if(response=="Group Doesnt Exist"){
            cout << "==============Group Doesn't Exists===============\n";
            return;
        }

    }else if(input[0]=="list_requests"){
        if(response=="Success"){
            bzero(buffer,16384);
            recv(sock, buffer, 16384,0);
            printf("Tracker : %s\n", buffer);
            cout << "==============Pending Requests Displayed===========\n";
            return;
        }else if(response=="Group Doesnt Exist"){
            cout << "==============Group Doesn't Exists===========\n";
            return;
        }

    }else if(input[0]=="leave_group"){
        if(response=="Success"){
            cout << "==============Left Group Successfully!===========\n";
            return;
        }else if(response=="Not a Group Member"){
            cout << "==============You are not a member===================\n";
            return;
        }
        else if(response=="No Group Present"){
            cout << "==============Group Doesn't Exists===============\n";
            return;
        }

    }else if(input[0]=="list_groups"){
        if(response=="Success"){
            bzero(buffer,16384);
            recv(sock, buffer, 16384,0);
            printf("Tracker : %s\n", buffer);
            cout << "==============Displayed Group Details===========\n";
            return;
        }

    }else if(input[0]=="show_downloads"){
        if(response=="Success"){
            bzero(buffer,16384);
            recv(sock, buffer, 16384,0);
            printf("Tracker : %s\n", buffer);
            cout << "==============Displayed Downloads So Far===========\n";
            return;
        }

    }else if(input[0]=="list_files"){
        if(response=="Success"){
            bzero(buffer,16384);
            recv(sock, buffer, 16384,0);
            printf("Tracker : %s\n", buffer);
            cout << "==============Displayed Files in Group===========\n";
            return;
        }else if(response=="No Group Present"){
            cout << "==============Group Doesn't Exists===========\n";
            return;
        }

    }else if(input[0]=="stop_share"){
        if(response=="Success"){
            cout << "==============Stopped Sharing===========\n";
            return;
        }else if(response=="No Group Present"){
            cout << "==============Group Doesn't Exists===========\n";
            return;
        }
        else if(response=="No File Present"){
            cout << "==============File Doesn't Exists in Group===========\n";
            return;
        }

    }else if(input[0]=="upload_file"){
        if(response=="Invalid Group Name"){
            cout << "==============Group Doesn't Exists===================\n";
            return;
        }
        if(response=="Invalid File Path"){
            cout << "==============Enter Valid File path===================\n";
            return;
        }
        if(response=="Success"){
            //find num of chunks file has
            int chunks = findNumberOfChunks(input[1]);
            string bitstr="";
            vector<int> v;
            for(int i=0;i<chunks;i++){
                bitstr+="1";
            }
            chunksPresent[input[1]]=bitstr;
            //send port to tracker
            string snd = "my_port "+ to_string(asServerPort);
            bzero(buffer,16384);
            strcpy(buffer,&snd[0]);
            printf("Client : %s\n",buffer);
            send(sock, buffer, sizeof(buffer), 0);
            bzero(buffer,16384);
            //recieve acknowledgement
            recv(sock, buffer, 16384,0);
            printf("Tracker : %s\n", buffer);
            if(string(buffer)=="recieved_port"){
                //send num of chunks to tracker
                bzero(buffer,16384);
                strcpy(buffer,&to_string(chunks)[0]);
                printf("Client : %s\n",buffer);
                send(sock, buffer, sizeof(buffer), 0);
            }
            cout << "==============Uploaded Succesfully============\n";

            return;
        }
    }else if(input[0]=="download_file"){
        if(response=="Absent"){
            cout << "==============File not present============\n";
            return;
        }else if(response=="Invalid groupname"){
            cout << "==============Group Does not Exist============\n";
            return;
        }else if(response=="File Unavailable"){
            cout << "==============File Not Present============\n";
            return;
        }else if(response=="Not A Member"){
            cout << "==============Please Join group to download============\n";
            return;
        }else if(response=="Path Unavailable"){
            cout << "==============Destination Path Invalid============\n";
            return;
        }else if(resVec[0]=="take_from"){

            //Tracker provides port of other clients having some chunk of that file
            vector<int> portsToConnect;
            for(int i=1;i<resVec.size();i++){
                cout << i <<" : " << resVec[i]<<endl;
                int prt = stoi(resVec[i]);
                portsToConnect.push_back(prt);
            }
            string filename = input[2];
            string dstPath = input[3];
            bzero(buffer,16384);
            string snd = "file_info "+filename;
            //Send filename to tracker which we want to download
            strcpy(buffer,&snd[0]);
            printf("Client : %s\n",buffer);
            send(sock, buffer, sizeof(buffer), 0);
            bzero(buffer,16384);
            //recieve number of chunks of file from tracker
            recv(sock, buffer, 16384,0);
            printf("Tracker : %s\n", buffer);
            int chunks = stoi(makeSplit(string(buffer),' ')[0]);
            cout << "Chunks : " << chunks << endl;
            vector<thread> t_arr;

            //Contacting other clients 
            for(int i=0;i<portsToConnect.size();i++){
                t_arr.push_back(thread(contactPeersToGatherInfo,portsToConnect[i],filename));
            }
            for(int i=0;i<t_arr.size();i++){
                if(t_arr[i].joinable()){
                    t_arr[i].join();
                }
            }
            
            //Get Port to chunk mapping
            unordered_map<int,vector<int>> mp = fileToPortToChunks[filename];

            //Chunk to port mapping
            map<int,vector<int>> revMap;

            //Piece Selection Algorithm
            for(auto i: fileToPortToChunks){
                for(auto j : i.second){
                    for(int k=0;k<j.second.size();k++){
                        if(revMap.find(j.second[k])==revMap.end()){
                            vector<int> temp;
                            temp.push_back(j.first);
                            revMap[j.second[k]]=temp;
                        }else{
                            revMap[j.second[k]].push_back(j.first);
                        }
                        
                    }
                }
            }

            //Get each chunk from just 1 random Port/client
            unordered_map<int,vector<int>> portToChunk;
            for(auto e: revMap){
                int chnk=e.first;
                int rnd = (rand())%e.second.size();
                portToChunk[e.second[rnd]].push_back(chnk);
            }

            vector<thread> dt_arr;
            //portToChunk
            for(auto e: mp){
                dt_arr.push_back(thread(contactPeersAndDownload,e.first,filename,e.second,dstPath));
                // can uncomment for Smooth Execution ->break;
            }
            for(int i=0;i<dt_arr.size();i++){
                if(dt_arr[i].joinable()){
                    dt_arr[i].join();
                }
            }

        }
    }
}

int main(int argc, char* argv[]){

    vector<string> peerString = makeSplit(argv[1],':');
    peerIP = (char *)(&peerString[0][0]);
    asServerPort = stoi(peerString[1]);
    string trackerInfoFile = argv[2];
    ifstream f(trackerInfoFile);
    string ipinp;
    int port;
    f >> ipinp;
    f >> port;
    trackerIP=(char *)(&ipinp[0]);
    trackerPort=port;
    loggedIn=false;
    int sock;

    struct sockaddr_in addr;
    socklen_t addr_size;
    char buffer[16384];
    int n;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock<0){
        perror("[-]Socket Error");
        exit(1);
    }
    printf("[-]TCP server socket created.\n");

    memset(&addr, '\0' ,sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_port=trackerPort;
    addr.sin_addr.s_addr = inet_addr(trackerIP);
    pthread_t t1;
    if(pthread_create(&t1, NULL, runAsServer, NULL) == -1){
        perror("pthread"); 
        exit(EXIT_FAILURE); 
    }
    connect(sock,(struct sockaddr*)&addr,sizeof(addr));
    printf("[-]Connected to Tracker!\n");
    while(1){
        bzero(buffer,16384);
        string s;
        //give command
        getline(cin,s);

        vector<string> inptStr = makeSplit(s,' ');
        if(inptStr[0] == "login" && loggedIn){
            cout << "You already have one active session" << endl;
            continue;
        }
        if(inptStr[0] != "login" && inptStr[0] != "create_user" && !loggedIn){
             cout << "Please login / create an account" << endl;
                continue;
        }

        strcpy(buffer,&s[0]);
        printf("Client : %s\n",buffer);
        send(sock,buffer,sizeof(buffer),0);
        peerCommands(sock,s);
    }
    close(sock);
    printf("[+]Disconnected from Tracker\n");

    return 0;
}