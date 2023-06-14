#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <bits/stdc++.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>


#define chunk_size 524288

using namespace std;

struct arg_struct {
    int client_sock;
};

struct client_details{
    int port;
    int clientSock;
    string password;
    string username;

};

struct group_details{
    string admin;
    map<string,client_details> members;
    map<string,vector<client_details>> fileToPorts;
    map<string,client_details> pendingReq;
};

int trackerPort;
unordered_map<string,string> usersMap;
unordered_map<int,client_details> sockToDetails;
unordered_map<int,client_details> portToDetails;

unordered_map<string,bool> loggedInUsers;
unordered_map<string,client_details> userToDetails;
unordered_map<string,client_details> fileToClient;

unordered_map<string,vector<int>> fileToPeers;
unordered_map<string,int> numOfChunks;

unordered_map<string,group_details> groupDetails;
vector<string> downloads;


bool doesFileExists (string name) {
  struct stat buffer;   
  return (stat (name.c_str(), &buffer) == 0); 
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
    int numChunks = ceil(end/chunk_size);
    return numChunks;
}
mode_t getPermissions(string s)
{
    struct stat sb;
    stat(s.c_str(), &sb);
    mode_t perm = 0;
    perm = ((sb.st_mode & S_IRUSR) ? 0400 : 0) | ((sb.st_mode & S_IWUSR) ? 0200 : 0) | ((sb.st_mode & S_IXUSR) ? 0100 : 0) | ((sb.st_mode & S_IRGRP ? 0040 : 0) | ((sb.st_mode & S_IWGRP) ? 0020 : 0) | ((sb.st_mode & S_IXGRP) ? 0010 : 0) | ((sb.st_mode & S_IROTH) ? 0004 : 0) | ((sb.st_mode & S_IWOTH) ? 0002 : 0) | ((sb.st_mode & S_IXOTH) ? 0001 : 0));
    return perm;
}

// void copyFile(string from, string to)
// {
//     int srcPtr;
//     int dstPtr;
//     char character;
//     srcPtr = open(from.c_str(), O_RDONLY);
//     dstPtr = open(to.c_str(), O_CREAT | O_WRONLY, getPermissions(from));

//     if (srcPtr == -1)
//     {   
//         cout << "Cannot open file sorry!!" << endl;
//         return;
//     }
//     if (dstPtr == -1)
//     {
//         cout << "The given file already exists" << endl;
//         return;
//     }
//     while (read(srcPtr, &character, 1))
//     {
//         write(dstPtr, &character, 1);
//     }
//     close(srcPtr);
//     close(dstPtr);
//     return;
// }

bool copyFile(char *SRC, char* DEST)
{
    std::ifstream src(SRC, std::ios::binary);
    std::ofstream dest(DEST, std::ios::binary);
    dest << src.rdbuf();
    return src && dest;
}


void logout(int client_sock,vector<string> input){
    char buffer[16384];
    if(input.size()!=1){
        // Error cmd
        bzero(buffer,16384);
        strcpy(buffer,"Invalid");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
        return;
    }
    loggedInUsers.erase(sockToDetails[client_sock].username);
    //loggedInUsers[sockToDetails[client_sock].username]=false;
    bzero(buffer,16384);
    strcpy(buffer,"Success");
    printf("Tracker : %s\n",buffer);
    send(client_sock, buffer, sizeof(buffer), 0);
    return;
}


void createUser(int client_sock,vector<string> input){
    char buffer[16384];
    if(input.size()!=3){
        // Error cmd
        bzero(buffer,16384);
        strcpy(buffer,"Invalid");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
    }else{
        if(usersMap.find(input[1])!=usersMap.end()){
            bzero(buffer,16384);
            strcpy(buffer,"Exists");
            printf("Tracker : %s\n",buffer);
            send(client_sock, buffer, sizeof(buffer), 0);
        }else{
            //Create user
            usersMap[input[1]]=input[2];
            bzero(buffer,16384);
            strcpy(buffer,"Success");
            printf("Tracker : %s\n",buffer);
            send(client_sock, buffer, sizeof(buffer), 0);
        }
    }
}

void login(int client_sock,vector<string> input){
    char buffer[16384];
    if(input.size()!=3){
        // Error cmd
        bzero(buffer,16384);
        strcpy(buffer,"Invalid");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
    }else{
        if(loggedInUsers.find(input[1])!=loggedInUsers.end()){
            bzero(buffer,16384);
            strcpy(buffer,"Already Logged In");
            printf("Tracker : %s\n",buffer);
            send(client_sock, buffer, sizeof(buffer), 0);
        }else if(usersMap.find(input[1])!=usersMap.end()){
            bzero(buffer,16384);
            strcpy(buffer,"Success");
            printf("Tracker : %s\n",buffer);
            send(client_sock, buffer, sizeof(buffer), 0);
            bzero(buffer,16384);
            recv(client_sock, buffer, 16384,0);
            printf("Client : %s\n", buffer);
            struct client_details cd;
            cd.username = input[1];
            cd.clientSock = client_sock;
            cd.password = usersMap[input[1]];
            cd.port = stoi(buffer);
            loggedInUsers[input[1]]=true;
            sockToDetails[client_sock] = cd;
            userToDetails[input[1]]=cd;
            portToDetails[cd.port] = cd;
        }else{
            bzero(buffer,16384);
            strcpy(buffer,"Absent");
            printf("Tracker : %s\n",buffer);
            send(client_sock, buffer, sizeof(buffer), 0);
        }
    }
    return;

}

void uploadFile(int client_sock,vector<string> input){
    char buffer[16384];
    if(input.size()!=3){
        // Error cmd
        bzero(buffer,16384);
        strcpy(buffer,"Invalid");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
        return;
    }else{
        if(!doesFileExists(input[1])){
            bzero(buffer,16384);
            strcpy(buffer,"Invalid File Path");
            printf("Tracker : %s\n",buffer);
            send(client_sock, buffer, sizeof(buffer), 0);
            return;
        }
        
        string fileName = makeSplit(input[1],'/').back();
        string groupName = input[2];
        string dstFile = groupName+"/"+fileName;
        cout << "Copying......"<<endl;
        copyFile(&input[1][0],&dstFile[0]);
        cout << "Done copying......"<<endl;
        if(groupDetails.find(groupName)==groupDetails.end()){
            bzero(buffer,16384);
            strcpy(buffer,"Invalid Group Name");
            printf("Tracker : %s\n",buffer);
            send(client_sock, buffer, sizeof(buffer), 0);
            return;
        }
        cout <<"Group is there"<<endl;
        if(groupDetails[groupName].fileToPorts.find(fileName)==groupDetails[groupName].fileToPorts.end()){
            vector<client_details> v;
            v.push_back(sockToDetails[client_sock]);
            groupDetails[groupName].fileToPorts[fileName]=v;
        }else{
            groupDetails[groupName].fileToPorts[fileName].push_back(sockToDetails[client_sock]);
        }

        cout << "File Uploaded : " << fileName<<"\n";
        fileToClient[fileName]=sockToDetails[client_sock];
        bzero(buffer,16384);
        strcpy(buffer,"Success");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
        //recieve port
        bzero(buffer,16384);
        recv(client_sock, buffer, 16384,0);
        printf("Client : %s\n", buffer);
        vector<string> res = makeSplit(string(buffer),' ');
        if(res[0]=="my_port"){
            if(fileToPeers.find(fileName)==fileToPeers.end()){
                vector<int> v;
                v.push_back(stoi(res[1]));
                fileToPeers[fileName]=v;
            }else{
                fileToPeers[fileName].push_back(stoi(res[1]));
            }
        }
        bzero(buffer,16384);
        strcpy(buffer,"recieved_port");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
        //recieve num of chunks
        bzero(buffer,16384);
        recv(client_sock, buffer, 16384,0);
        printf("Client : %s\n", buffer);
        numOfChunks[fileName]= stoi(string(buffer));

    }
}


void downloadFile(int client_sock,vector<string> input){
    char buffer[16384];
    if(input.size()!=4){
        // Error cmd
        bzero(buffer,16384);
        strcpy(buffer,"Invalid");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
        return;
    }else{
        string groupName = input[1];
        string fileName = input[2];
        string dstPath = input[3];
        if(groupDetails.find(groupName)==groupDetails.end()){
            bzero(buffer,16384);
            strcpy(buffer,"Invalid groupname");
            printf("Tracker : %s\n",buffer);
            send(client_sock, buffer, sizeof(buffer), 0);
            return;
        }
        if(groupDetails[groupName].fileToPorts.find(fileName)==groupDetails[groupName].fileToPorts.end()){
            bzero(buffer,16384);
            strcpy(buffer,"File Unavailable");
            printf("Tracker : %s\n",buffer);
            send(client_sock, buffer, sizeof(buffer), 0);
            return;
        }
        // if(doesFileExists(dstPath)){
        //     bzero(buffer,16384);
        //     strcpy(buffer,"Path Unavailable");
        //     printf("Tracker : %s\n",buffer);
        //     send(client_sock, buffer, sizeof(buffer), 0);
        //     return;
        // }

        string myName = sockToDetails[client_sock].username;
        bool isAMember=false;
        for(auto e: groupDetails[groupName].members){
            if(e.first==myName){
                isAMember=true;
                break;
            }
        }
        if(!isAMember){
            bzero(buffer,16384);
            strcpy(buffer,"Not A Member");
            printf("Tracker : %s\n",buffer);
            send(client_sock, buffer, sizeof(buffer), 0);
            return;
        }
        

        cout << "File To be Downloaded : " << fileName<<endl;

        
        string res = "take_from";
        for(int e: fileToPeers[fileName]){
            if(loggedInUsers.find(portToDetails[e].username)!=loggedInUsers.end()){
                res+= " "+ to_string(e);
            } 
        }
        bzero(buffer,16384);
        strcpy(buffer,&res[0]);
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
        bzero(buffer,16384);
        recv(client_sock, buffer, 16384,0);
        printf("Client : %s\n", buffer);
        vector<string> response = makeSplit(string(buffer),' ');
        if(response[0]=="file_info"){
            string numChunks = to_string(numOfChunks[response[1]]);
            bzero(buffer,16384);
            strcpy(buffer,&numChunks[0]);
            printf("Tracker : %s\n",buffer);
            send(client_sock, buffer, sizeof(buffer), 0);
        }

        
    }
}

void createGroup(int client_sock,vector<string> input){
    char buffer[16384];
    if(input.size()!=2){
        // Error cmd
        bzero(buffer,16384);
        strcpy(buffer,"Invalid");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
        return;
    }
    string groupName = input[1];

    if(groupDetails.find(groupName)!=groupDetails.end()){
        bzero(buffer,16384);
        strcpy(buffer,"Exists");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
        return;
    }else{
        group_details gd;
        gd.admin = sockToDetails[client_sock].username;
        gd.members[sockToDetails[client_sock].username] = sockToDetails[client_sock];
        groupDetails[groupName] = gd;
        if (mkdir(&groupName[0], 0777) == -1){
            cerr << "Error :  " << strerror(errno) << endl;
        }else{
            cout << "Directory created "<< groupName << endl;
        }

        bzero(buffer,16384);
        strcpy(buffer,"Success");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
        return;
    }

}

void requestGroupJoin(int port,int client_sock,string groupName,string uname){
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

    bzero(buffer,16384);
    strcpy(buffer,"join_group");
    printf("Tracker : %s\n",buffer);
    send(sock,buffer,sizeof(buffer),0);
    bzero(buffer,16384);
    cout << "Waiting for Acceptance...."<<endl;
    recv(sock,buffer,16384,0);
    printf("Admin : %s\n",buffer);
    if(string(buffer)=="accept"){
        groupDetails[groupName].members[uname] = sockToDetails[client_sock];
        bzero(buffer,16384);
        strcpy(buffer,"Success");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
        
    }else{
        bzero(buffer,16384);
        strcpy(buffer,"Rejected");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
    }
    close(sock);
}

void joinGroup(int client_sock,vector<string> input){
    char buffer[16384];
    if(input.size()!=2){
        // Error cmd
        bzero(buffer,16384);
        strcpy(buffer,"Invalid");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
        return;
    }
    string groupName = input[1];

    if(groupDetails.find(groupName)==groupDetails.end()){
        bzero(buffer,16384);
        strcpy(buffer,"Group Doesnt Exist");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
        return;
    }
    string uname = sockToDetails[client_sock].username;
    if(groupDetails[groupName].members.find(uname)!=groupDetails[groupName].members.end()){
        bzero(buffer,16384);
        strcpy(buffer,"Already a Member");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
        return;
    }
    // int adminPort = userToDetails[groupDetails[groupName].admin].port;
    // thread t(requestGroupJoin,adminPort,client_sock,groupName,uname);
    // t.join();
    //bool res = requestGroupJoin(adminPort);
    if(groupDetails[groupName].pendingReq.find(uname)!=groupDetails[groupName].pendingReq.end()){
        bzero(buffer,16384);
        strcpy(buffer,"Already In Queue");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
        return;
    }

    groupDetails[groupName].pendingReq[uname]=sockToDetails[client_sock];
    bzero(buffer,16384);
    strcpy(buffer,"Success");
    printf("Tracker : %s\n",buffer);
    send(client_sock, buffer, sizeof(buffer), 0);
    // groupDetails[groupName].members[uname] = sockToDetails[client_sock];
    // bzero(buffer,16384);
    // strcpy(buffer,"Success");
    // printf("Tracker : %s\n",buffer);
    // send(client_sock, buffer, sizeof(buffer), 0);
    
    return;
}

void acceptRequest(int client_sock,vector<string> input){
    char buffer[16384];
    if(input.size()!=3){
        // Error cmd
        bzero(buffer,16384);
        strcpy(buffer,"Invalid");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
        return;
    }
    string groupName = input[1];
    string uname = input[2];

    if(groupDetails.find(groupName)==groupDetails.end()){
        bzero(buffer,16384);
        strcpy(buffer,"Group Doesnt Exist");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
        return;
    }

    if(groupDetails[groupName].pendingReq.find(uname)==groupDetails[groupName].pendingReq.end()){
        bzero(buffer,16384);
        strcpy(buffer,"No Pending Request");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
        return;
    }
    groupDetails[groupName].members[uname] = groupDetails[groupName].pendingReq[uname];
    groupDetails[groupName].pendingReq.erase(uname);
    bzero(buffer,16384);
    strcpy(buffer,"Success");
    printf("Tracker : %s\n",buffer);
    send(client_sock, buffer, sizeof(buffer), 0);
    return;
    
}

void listPendingRequests(int client_sock,vector<string> input){
    char buffer[16384];
    if(input.size()!=2){
        // Error cmd
        bzero(buffer,16384);
        strcpy(buffer,"Invalid");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
        return;
    }
    string groupName=input[1];
    if(groupDetails.find(groupName)==groupDetails.end()){
        bzero(buffer,16384);
        strcpy(buffer,"Group Doesnt Exist");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
        return;
    }
    bzero(buffer,16384);
    strcpy(buffer,"Success");
    printf("Tracker : %s\n",buffer);
    send(client_sock, buffer, sizeof(buffer), 0);
    string reqs="Pending Users :\n";
    
    for(auto e: groupDetails[groupName].pendingReq){
        reqs+= e.first+"\n";
    }
    bzero(buffer,16384);
    strcpy(buffer,&reqs[0]);
    printf("Tracker : %s\n",buffer);
    send(client_sock, buffer, sizeof(buffer), 0);
    return;

}

void leaveGroup(int client_sock,vector<string> input){
    char buffer[16384];
    if(input.size()!=2){
        // Error cmd
        bzero(buffer,16384);
        strcpy(buffer,"Invalid");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
        return;
    }
    string groupName = input[1];

    if(groupDetails.find(groupName)==groupDetails.end()){
        bzero(buffer,16384);
        strcpy(buffer,"No Group Present");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
        return;
    }
    string uname = sockToDetails[client_sock].username;
    if(groupDetails[groupName].members.find(uname)==groupDetails[groupName].members.end()){
        bzero(buffer,16384);
        strcpy(buffer,"Not a Group Member");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
        return;
    }
    groupDetails[groupName].members.erase(uname);
    bzero(buffer,16384);
    strcpy(buffer,"Success");
    printf("Tracker : %s\n",buffer);
    send(client_sock, buffer, sizeof(buffer), 0);
    return;
}

void listGroups(int client_sock,vector<string> input){
    char buffer[16384];
    if(input.size()!=1){
        // Error cmd
        bzero(buffer,16384);
        strcpy(buffer,"Invalid");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
        return;
    }
    bzero(buffer,16384);
    strcpy(buffer,"Success");
    printf("Tracker : %s\n",buffer);
    send(client_sock, buffer, sizeof(buffer), 0);
    string groups="GROUPS :\n";
    for(auto e: groupDetails){
        groups+= e.first+"\n";
    }
    bzero(buffer,16384);
    strcpy(buffer,&groups[0]);
    printf("Tracker : %s\n",buffer);
    send(client_sock, buffer, sizeof(buffer), 0);
    return;

}

void listFiles(int client_sock,vector<string> input){
    char buffer[16384];
    if(input.size()!=2){
        // Error cmd
        bzero(buffer,16384);
        strcpy(buffer,"Invalid");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
        return;
    }
    string groupName = input[1];
    if(groupDetails.find(groupName)==groupDetails.end()){
        bzero(buffer,16384);
        strcpy(buffer,"No Group Present");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
        return;
    }

    bzero(buffer,16384);
    strcpy(buffer,"Success");
    printf("Tracker : %s\n",buffer);
    send(client_sock, buffer, sizeof(buffer), 0);
    string files="Files :\n";
    for(auto e: groupDetails[groupName].fileToPorts){
        files+= e.first+"\n";
    }
    bzero(buffer,16384);
    strcpy(buffer,&files[0]);
    printf("Tracker : %s\n",buffer);
    send(client_sock, buffer, sizeof(buffer), 0);
    return;

}

void stopSharing(int client_sock,vector<string> input){
    char buffer[16384];
    if(input.size()!=3){
        // Error cmd
        bzero(buffer,16384);
        strcpy(buffer,"Invalid");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
        return;
    }
    string groupName = input[1];
    string fileName = input[2];

    if(groupDetails.find(groupName)==groupDetails.end()){
        bzero(buffer,16384);
        strcpy(buffer,"No Group Present");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
        return;
    }
    if(groupDetails[groupName].fileToPorts.find(fileName)==groupDetails[groupName].fileToPorts.end()){
        bzero(buffer,16384);
        strcpy(buffer,"No File Present");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
        return;
    }
    int result = remove(&fileName[0]);
    groupDetails[groupName].fileToPorts.erase(fileName);
    bzero(buffer,16384);
    strcpy(buffer,"Success");
    printf("Tracker : %s\n",buffer);
    send(client_sock, buffer, sizeof(buffer), 0);
    return;
}

void showDownloads(int client_sock,vector<string> input){
    char buffer[16384];
    if(input.size()!=1){
        // Error cmd
        bzero(buffer,16384);
        strcpy(buffer,"Invalid");
        printf("Tracker : %s\n",buffer);
        send(client_sock, buffer, sizeof(buffer), 0);
        return;
    }
    bzero(buffer,16384);
    strcpy(buffer,"Success");
    printf("Tracker : %s\n",buffer);
    send(client_sock, buffer, sizeof(buffer), 0);

    string reqs="DOWNLOADS :\n";
    
    for(string e: downloads){
        reqs+= e+"\n";
    }
    bzero(buffer,16384);
    strcpy(buffer,&reqs[0]);
    printf("Tracker : %s\n",buffer);
    send(client_sock, buffer, sizeof(buffer), 0);
    return;
}

void* handleClient(void* args){

    int client_sock = ((struct arg_struct*)args)->client_sock;
    printf("[-]A new thread to handle client %d is opened\n",client_sock);
    printf("[-]Client connected from Tracker\n");
    char buffer[16384];
    while(1){
        bzero(buffer,16384);
        recv(client_sock, buffer, 16384,0);
        //Request Recieved
        printf("Client : %s\n", buffer);
        string request(&buffer[0]);
        vector<string> input = makeSplit(request,' ');
        if(input[0]=="create_user"){
            createUser(client_sock,input);
        }else if(input[0]=="login"){
            login(client_sock,input);
        }else if(input[0]=="upload_file"){
            uploadFile(client_sock,input);
        }else if(input[0]=="download_file"){
            downloadFile(client_sock,input);
        }else if(input[0]=="logout"){
            logout(client_sock,input);
        }else if(input[0]=="create_group"){
            createGroup(client_sock,input);
        }else if(input[0]=="join_group"){
            joinGroup(client_sock,input);
        }else if(input[0]=="leave_group"){
            leaveGroup(client_sock,input);
        }else if(input[0]=="list_groups"){
            listGroups(client_sock,input);
        }else if(input[0]=="show_downloads"){
            showDownloads(client_sock,input);
        }else if(input[0]=="list_files"){
            listFiles(client_sock,input);
        }else if(input[0]=="stop_share"){
            stopSharing(client_sock,input);
        }else if(input[0]=="accept_request"){
            acceptRequest(client_sock,input);
        }else if(input[0]=="list_requests"){
            listPendingRequests(client_sock,input);
        }else{
            //Invalid command
            bzero(buffer,16384);
            strcpy(buffer,"Invalid");
            printf("Tracker : %s\n",buffer);
            send(client_sock, buffer, sizeof(buffer), 0);
        }
        
    }
    close(client_sock);
    printf("[+]Client %d Disconnected from Tracker\n",client_sock);

}

int main(int argc, char* argv[]){

    string trackerInfoFile = argv[1];
    ifstream f(trackerInfoFile);
    string ipinp;
    int port;
    f >> ipinp;
    f >> port;
    char const* ip = (char const *)(&ipinp[0]);

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
    printf("[-]TCP server socket created.\n");

    memset(&server_addr, '\0' ,sizeof(server_addr));
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=port;
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
    printf("[-]Bind to the port number: %d\n",port);
    listen(server_sock,5);
    printf("Tracker Listening.....\n");
    
    while(1){
        addr_size = sizeof(client_addr);    
        client_sock = accept(server_sock,(struct sockaddr*)&client_addr,&addr_size);
        if(client_sock<0){
            perror("[-]Error while accepting client");
        }
        struct arg_struct *argS = (struct arg_struct *)malloc(sizeof(struct arg_struct));
        argS->client_sock = client_sock;
        pthread_t t1;
        if(pthread_create(&t1, NULL, handleClient, (void *)argS) == -1){
            perror("pthread"); 
            exit(EXIT_FAILURE); 
        }
    }
    
    //close(client_sock);
    printf("[+]Client Disconnected from Tracker\n\n");


    return 0;
}