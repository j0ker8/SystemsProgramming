#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <pwd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <time.h>
#include <grp.h>

#ifndef MAX_BUF
#define MAX_BUF 1000
#endif

int CommunicationFD;
int DataChannel;
int CommandChannel_Write;
int CommandChannel_Read;
int IsUserLoggedIn=0;
int FileTransferProcess=0;
int ServerMarker = 0;
int Killed = 0;
int prevcmdRNFR = 0;

char* CommandBuffer;
char* DataChannelName;
char* HomeDirectory;
char* WorkingDirectory;
char* ChannelName;
char* ChannelName_Read;
char* ChannelName_Write;
char* RNFRarg;


void DoExit()
{
    if (DataChannel > 0)
    {
        close(DataChannel);
        unlink(DataChannelName);
        free(DataChannelName);
    }
    close(CommandChannel_Write);
    close(CommandChannel_Read);
    free(HomeDirectory);
    free(WorkingDirectory);
    free(ChannelName);
    free(ChannelName_Read);
    free(ChannelName_Write);
    free(RNFRarg);
    close(CommunicationFD);
    exit(0);
}
void DisconnectClient()
{
    if (DataChannel > 0)
    {
        close(DataChannel);
        unlink(DataChannelName);
        free(DataChannelName);
    }
    close(CommandChannel_Write);
    close(CommandChannel_Read);
    free(ChannelName);
    free(ChannelName_Read);
    free(ChannelName_Write);
    free(RNFRarg);
    exit(0);
}
// CTRL-Z or CTRL-C signal received
void CTRLCCTRLV_Detected(int signo)
{
    printf("\nServer Closed.\n");
    DoExit();
}

void SendResponseToClient(const char* ResponseString)
{
    fprintf(stderr,"Response Sent to %s, Length: %lu\n",ResponseString,strlen(ResponseString));
    write(CommandChannel_Write,ResponseString,strlen(ResponseString)+1);
}

int FileTransferInterupted;
void InteruptFileTransfer(int status)
{
    FileTransferInterupted = 1;
}

void KillTheDataProcess(int status)
{
    Killed = 1;
}

void ReportStatus(int status)
{
    char Response[50] = {0};
    sprintf(Response,"211 Transfering File ServerMarker=%d.",ServerMarker);
    SendResponseToClient(Response);
}

const char* GetFileName(char* filepath)
{
    char* FileName = NULL;
    char* Token = strtok(filepath, "/");
    while (Token != NULL)
    {
        if (FileName != NULL)
        {
            free(FileName);
        }
        FileName = malloc(strlen(Token));
        strcpy(FileName, Token);
        Token = strtok(NULL, "/");
    }
    return FileName;
}

void SendStringToDataChannel(char DataString[])
{
    if (fork()==0)
    {
        for(int i=0;i<strlen(DataString);i++)
        {
            write(DataChannel,&DataString[i],1);
        }
        char * a = "\0";
        write(DataChannel,a,1);
        free(a);
        exit(0);
    }
}

void DownloadTheFile(char* FileName)
{
    if ((FileTransferProcess=fork())==0)
    {
        signal(SIGTERM,InteruptFileTransfer);
        signal(SIGUSR1,KillTheDataProcess);
        signal(SIGUSR2,ReportStatus);
        ServerMarker = 0;
        int FileFinished = 0;
        FileTransferInterupted = 0;
        char* FileBuffer = malloc(MAX_BUF);
        int FDDownload = open(FileName,O_RDONLY);
        fprintf(stderr,"Start Downloading the file ...\n");
        while(FileFinished==0 && FileTransferInterupted==0 && Killed == 0)
        {
            int ActualReadData = read(FDDownload,FileBuffer,MAX_BUF);
            //sleep(1);
            if (ActualReadData < MAX_BUF)
            {
                FileFinished = 1;
            }
            write(DataChannel,FileBuffer,ActualReadData);
            ServerMarker+=ActualReadData;
        }
        close(FDDownload);
        free(FileBuffer);
        if (FileTransferInterupted==1)
        {
            SendResponseToClient("110 Restart marker reply.");
            //File is incomplete so delete it
            remove(FileName);
        }
        else if (Killed == 1)
        {
            //File is incomplete so delete it
            remove(FileName);
        }
        else
        {
            fprintf(stderr,"Finish Downloading the file ...\n");
        }
        ServerMarker = 0;
        exit(0);
    }
}

void UploadTheFile(char* filepath)
{
    if ((FileTransferProcess=fork())==0)
    {
        signal(SIGTERM,InteruptFileTransfer);
        signal(SIGUSR1,KillTheDataProcess);
        signal(SIGUSR2,ReportStatus);
        ServerMarker = 0;
        const char* FileName = GetFileName(filepath);
        int FileFinished = 0;
        FileTransferInterupted = 0;
        char* FileBuffer = malloc(MAX_BUF);
        int FDUpload = open(FileName,O_CREAT | O_WRONLY,0777);
        fprintf(stderr,"Start Uploading the file ...\n");
        while(FileFinished==0 && FileTransferInterupted==0 && Killed == 0)
        {
            int ActualReadData = read(DataChannel,FileBuffer,MAX_BUF);
            sleep(1);
            //fprintf(stderr,"Data Received ...\n");
            if (ActualReadData < MAX_BUF)
            {
                FileFinished = 1;
            }
            //fprintf(stderr,"Saved %d chunk\n",ActualReadData);
            write(FDUpload,FileBuffer,ActualReadData);
            ServerMarker+=ActualReadData;
        }
        close(FDUpload);
        free(FileBuffer);
        if (FileTransferInterupted==1)
        {
            SendResponseToClient("110 Restart marker reply.");
            //File is incomplete so delete it
            remove(filepath);
        }
        else if (Killed == 1)
        {
            //File is incomplete so delete it
            remove(filepath);
        }
        else
        {
            fprintf(stderr,"Finish Uploading the file ...\n");
            fprintf(stderr,"File Saved To %s\n",FileName);
        }
        ServerMarker = 0;
        exit(0);
    }
}

void AppendTheFile(char* filepath)
{
    if ((FileTransferProcess=fork())==0)
    {
        signal(SIGTERM,InteruptFileTransfer);
        signal(SIGUSR1,KillTheDataProcess);
        signal(SIGUSR2,ReportStatus);
        ServerMarker = 0;
        const char* FileName = GetFileName(filepath);
        int FileFinished = 0;
        FileTransferInterupted = 0;
        char* FileBuffer = malloc(MAX_BUF);
        int FDUpload = open(FileName,O_CREAT | O_WRONLY,0777);
        lseek(FDUpload,0,SEEK_END);
        fprintf(stderr,"Start Uploading the file ...\n");
        while(FileFinished==0 && FileTransferInterupted==0 && Killed==0)
        {
            int ActualReadData = read(DataChannel,FileBuffer,MAX_BUF);
            //fprintf(stderr,"Data Received ...\n");
            if (ActualReadData < MAX_BUF)
            {
                FileFinished = 1;
            }
            //fprintf(stderr,"Saved %d chunk\n",ActualReadData);
            write(FDUpload,FileBuffer,ActualReadData);
            ServerMarker+=ActualReadData;
        }
        close(FDUpload);
        free(FileBuffer);
        fprintf(stderr,"Finish Uploading the file ...\n");
        fprintf(stderr,"File Saved To %s\n",FileName);
        if (FileTransferInterupted==1)
        {
            //File is incomplete so delete it
            SendResponseToClient("110 Restart marker reply.");
            remove(filepath);
        }
        else if (Killed == 1)
        {
            //File is incomplete so delete it
            remove(filepath);
        }
        else
        {
            fprintf(stderr,"Finish Uploading the file ...\n");
            fprintf(stderr,"File Saved To %s\n",FileName);
        }
        ServerMarker = 0;
        exit(0);
    }
}

const char* CWD(const char* parameter)
{
    //since the user is not allowed to go out side the Home Directory we add the Home Directory to all the pathes requested by user
    //allocate enough space equal to length of home directory + length of the parameter
    char* NewWoringDirectory = malloc(strlen(HomeDirectory)+strlen(parameter));
    sprintf(NewWoringDirectory,"%s%s",HomeDirectory,parameter); // add the user requested parameter to the end of the home directory
    if (chdir(NewWoringDirectory)==-1)
    {
        return "550 REQUESTED ACTION NOT TAKED";
    }
    else
    {
        free(NewWoringDirectory);
        getcwd(WorkingDirectory, 200);
        return "250 Requested file action okay, completed.";
    }
}

const char* CDUP()
{
    char s[100];
    getcwd(s,100); // Get current directory
    if (strcmp(s,HomeDirectory)==0){ // if the current directory is the home directory, you're not allowed to CDUP
        return "550 NOT ALLOWED TO ACCESS PARENT DIR";
    }
    else {
        if(chdir("..")==-1){ // If the command fails
            return "550 REQUESTED ACTION NOT TAKED";
        } 
        else return "250 REQUESTED FILE ACTION OKAY, COMPLETED"; // On successful execution, return this code
    }
}

const char* REIN()
{
    if (FileTransferProcess>0)
    {
        kill(FileTransferProcess,SIGUSR1);
        FileTransferProcess = 0;
    }
    if (DataChannel!=0)
    {
        close(DataChannel);
        DataChannel=0;
    }
    chdir(HomeDirectory);
    getcwd(WorkingDirectory, 200);
    free(RNFRarg);
    prevcmdRNFR = 0;
    IsUserLoggedIn = 0;
    ServerMarker = 0;
    return "220 Service ready for new user.";
}

const char* QUIT()
{
    if (FileTransferProcess>0)
    {
        SendResponseToClient("221 Service closing control connection.");
        int Status;
        waitpid(FileTransferProcess,&Status,0);
        DoExit();
        return "";
    }
    else
    {
        SendResponseToClient("221 Service closing control connection.");
        DoExit();
        return "";
    }
}

const char* PORT(const char* parameter)
{
    if (IsUserLoggedIn == 0)
    {
        return "530 Not logged in.";
    }
    else if (mkfifo(parameter,0777) != 0)
    {
        return "500 Channel Already Exists.";
    }
    else
    {
        DataChannelName = malloc(strlen(parameter));
        strcpy(DataChannelName,parameter);
        DataChannel = open(parameter,O_RDWR);
        return "200 Command okay.";
    }
}

const char* RETR(char* parameter)
{
    if (IsUserLoggedIn==0)
    {
        return "530 Not logged in.";
    }
    else if (DataChannel==0)
    {
        return "425 Can't open data connection.";
    }
    else
    {
        DownloadTheFile(parameter);
        return "125 Data connection already open; transfer starting.";
    }
}

const char* STOR(char* parameter)
{
    UploadTheFile(parameter);
    return "125 Data connection already open; transfer starting.";
}

const char* APPE(char* parameter)
{
    if (IsUserLoggedIn==0)
    {
        return "530 Not logged in.";
    }
    else if (DataChannel==0)
    {
        return "425 Can't open data connection.";
    }
    else
    {
        AppendTheFile(parameter);
        return "125 Data connection already open; transfer starting.";
    }
}

const char* REST()
{
    if (FileTransferProcess>0)
    {
        kill(FileTransferProcess,SIGTERM);
        FileTransferProcess = 0;
        return "";
    }
    else
    {
        return "225 Data connection open; no transfer in progress.";
    }
}

const char* RNFR(const char* parameter)
{
    prevcmdRNFR = 1; // If change this flag to one when RNFR command is encountered.
    RNFRarg = malloc(strlen(parameter)); // Assign memory to this pointer to store the Rename directory argument for later use in RNTO command
    strcpy(RNFRarg,parameter); // Store the argument
    return "350 REQUESTED FILE ACTION PENDING FURTHER INFORMATION"; // Returning the code.
}

const char* RNTO(const char* parameter)
{
    if (prevcmdRNFR==1){  // If we previously encountered RNFR
        prevcmdRNFR=0; // change the flag to 0
        if (rename(RNFRarg,parameter)==0){ // Try rename function with the stored value for renaming 
            free(RNFRarg); // Free the memory after use.
            return "250 REQUESTED FILE ACTION OKAY, COMPLETED"; // Return the code
        }
        else{
            return "500 FILE ACTION CANNOT BE PROCESSED"; // if we cannot perform the function
        }
    }
    else
    {
        return "500 FILE ACTION CANNOT BE PROCESSED"; 
    }
}

const char* ABOR()
{
    if (FileTransferProcess>0)
    {
        kill(FileTransferProcess,SIGUSR1);
        FileTransferProcess = 0;
        close(DataChannel);
        unlink(DataChannelName);
        free(DataChannelName);
        return "226 Closing data connection.";
    }
    else
    {
        return "225 Data connection open; no transfer in progress.";
    }
}

const char* DELE(const char* parameter)
{
    if(remove(parameter)==0){ // Try deleting the file 
      return "250 REQUESTED FILE ACTION OKAY, COMPLETED"; // Error code if remove command is successful
    }
    else {
      return "501 SYNTAX ERROR IN PARAMETER OR ARGUMENTS"; // Error code if command is unsuccessful
    }
}

const char* RMD(const char* parameter) 
{
    if(rmdir(parameter)==0){ // Deleting a directory
      return "250 REQUESTED FILE ACTION OKAY, COMPLETED";
    }
    else {
      return "501 SYNTAX ERROR IN PARAMETER OR ARGUMENTS";
    }
}

const char* MKD(const char* parameter)
{
    if(mkdir(parameter,0777)==0){ // Creating a directory with 0777 permissions
        return "250 REQUESTED FILE ACTION OKAY, COMPLETED";
    }
    else return "501 SYNTAX ERROR IN PARAMETER OR ARGUMENTS";
}

const char* PWD()
{
    if (IsUserLoggedIn==0)
    {
        return "530 Not logged in.";
    }
    else 
    {
        char* response = malloc(210);
        int HomeDirectoryLength = strlen(HomeDirectory);
        //user should noe be aware of the exact serverside HomeDirectory
        //So We remove the HomeDirectory path from the current working directory of the client
        //for doing this we just need to add the length of the homedirectory to the WorkingDirectory
        //Pointer so we changing it's first character pointing to it.
        char* CorrectDirectory = malloc(strlen(WorkingDirectory)-HomeDirectoryLength+1);
        int cnt = 0;
        for(int i=HomeDirectoryLength;i<strlen(WorkingDirectory);i++)
        {
            CorrectDirectory[cnt++] = WorkingDirectory[i];
        }
        sprintf(response,"257 \"%s/\"",CorrectDirectory);
        free(CorrectDirectory);
        return response;
    }
}

const char* LIST(const char* dir,int WriteToDataChannel)
{
    struct stat s;
    int a = stat(dir,&s);  // Running stat function on the input string
    if (a==-1) // If the stat function is unsuccessful, it means there is no parameter. Hence the function should return list of files in the current directory
    {
        struct dirent *entries;
        DIR *dirobject = opendir("."); // Open current directory 
        if(!dirobject){ // If there no objects
            return "501 SYNTAX ERROR IN PARAMETER OR ARGUMENTS";
        }
        else { // If there are subdirectories, we print the subdirectories
            char returnstring [4000] = { 0 };
            while((entries = readdir(dirobject))!=NULL){ 
                if(entries->d_name[0]=='.'){ // If the first char is "."
                    if(strlen(entries->d_name)==1){ // If only "." is present we print it.
                        strcat(returnstring,entries->d_name); // Add it to the string to be returned
                        strcat(returnstring,"\n");
                    }
                    else if (strlen(entries->d_name)==2 && entries->d_name[1]=='.'){ // IF the length of the name of subdirectory is 2 and the second char is "."
                        strcat(returnstring,entries->d_name); // Add it to the string to be returned.
                        strcat(returnstring,"\n");
                    }
                    //else continue;
                }
                else {
                    strcat(returnstring,entries->d_name); // If the first letter is not ".", add it to return string.
                    strcat(returnstring,"\n");
                }
            }
            //printf("%s\n",returnstring);
            if (WriteToDataChannel==1) // If the result is to be returned to data channel
            {
                SendStringToDataChannel(returnstring); // Write result to data channel 
                return "250 REQUESTED FILE ACTION OKAY, COMPLETED"; // Return the error code
            }
            else
            {
                return returnstring; // or return the returnstring. This is for usage in STAT function
            }
        }
    }
    else { // If the stat function is successful
        if(S_ISREG(s.st_mode)){ // If the file is a Regular file
            char  perm[] = "----------"; // permission string
            
            struct passwd *userid; // Structure to store info about the owner
            struct group *groupid; // Structure to store info about the group
            int unamesize = 0; //int to store size of owner's name
            int gnamesize = 0; //int to store size of group name
            char * username ; // Pointer to name of the file owner
            char * groupname ; // Pointer to group name for the file
            if ((userid=getpwuid(s.st_uid))){ // Storing the user id of the file owner
                //printf("%s\n",userid->pw_name);
                unamesize = strlen(userid->pw_name); //Storing the size of owner's name
            }
            if ((groupid=getgrgid(s.st_gid))){ // Storing the group id of file owner
                //printf("%s\n",groupid->gr_name);
                gnamesize = strlen(groupid->gr_name); // Storing size of group's name
            }
            
            if(unamesize>0){ 
                username = malloc(unamesize);
                strcpy(username,userid->pw_name); // Storing name of file owner from userid structure
            }
            
            if (gnamesize>0){
                groupname = malloc(gnamesize);
                strcpy(groupname,groupid->gr_name); // Storing name of group form groupid structure
            }
            
            if(s.st_mode & S_IRUSR) perm[1]='r'; // editing the permissions string according to the mode flags
            if(s.st_mode & S_IWUSR) perm[2]='w';
            if(s.st_mode & S_IXUSR) perm[3]='x';
            if(s.st_mode & S_IRGRP) perm[4]='r';
            if(s.st_mode & S_IWGRP) perm[5]='w';
            if(s.st_mode & S_IXGRP) perm[6]='x';
            if(s.st_mode & S_IROTH) perm[7]='r';
            if(s.st_mode & S_IWOTH) perm[8]='w';
            if(s.st_mode & S_IXOTH) perm[9]='x';
            
            int filesize = s.st_blksize; // Storing size of the file
            
            struct tm *lastmod = localtime(&s.st_mtime); // Storing time from the stat object
            
            char * timestring = asctime(lastmod); 
            timestring[strcspn(timestring,"\n")] = ' '; // processing time_t type and converting it to string

            char finalstring[100] ;
            int k = sprintf(finalstring,"%s %s %s %d %s%s\n",perm,username,groupname,filesize,timestring,dir); // storing data in correct format in finalstring
            //puts(finalstring);
            //printf("%s",finalstring);
            if (WriteToDataChannel==1)  
            {
                SendStringToDataChannel(finalstring); // Write the string to Datachannel
                return "250 REQUESTED FILE ACTION OKAY, COMPLETED";
            }
            else
            {
                return finalstring;
            }
            
        }
        else if (S_ISDIR(s.st_mode)){ // If the argument is a directory
            struct dirent *entries;
            DIR *dirobject = opendir(dir); // Open the dir
            if(!dirobject){ // if open dir fails
                return "501 SYNTAX ERROR IN PARAMETER OR ARGUMENTS";
            }
            else { // If there are greater than zero subdirs
                char returnstring [1024];
                while((entries = readdir(dirobject))!=NULL){ // return ".", ".." and other files not starting with ".".
                    if(entries->d_name[0]=='.'){ 
                        if(strlen(entries->d_name)==1){
                            strcat(returnstring,entries->d_name);
                            strcat(returnstring,"\n");
                        }
                        else if (strlen(entries->d_name)==2 && entries->d_name[1]=='.'){
                            strcat(returnstring,entries->d_name);
                            strcat(returnstring,"\n");
                        }
                        else continue;
                    }
                    else {
                        strcat(returnstring,entries->d_name);
                        strcat(returnstring,"\n");
                    }
                }
                //printf("%s\n",returnstring);
                if (WriteToDataChannel==1)
                {
                    SendStringToDataChannel(returnstring);
                    return "250 REQUESTED FILE ACTION OKAY, COMPLETED";
                }
                else
                {
                    return returnstring;
                }
            }
        }
        return "250 REQUESTED FILE ACTION OKAY, COMPLETED";
    }
    return "501 SYNTAX ERROR IN PARAMETER OR ARGUMENTS";
}

const char* STAT(const char* parameter)
{
    if (parameter == NULL)
    {
        if (FileTransferProcess>0)
        {
            if (kill(FileTransferProcess,SIGUSR2)==-1)
            {
                FileTransferProcess=0;
                return "211 Server Ready, Control and Data Connection is Open.";
            }
        }
        else if (DataChannel>0)
        {
            return "211 Server Ready, Control and Data Connection is Open.";
        }
        else
        {
            return "211 Server Ready, Control Connection is Open.";
        }
    }
    else 
    {
        //the parameter is path name do the list command but use the control connection to respond
        LIST(parameter,0);
        return "";
    }
}

const char* NOOP()
{
    return "200 Command okay.";
}

int GetProcessIDlength()
{
    int pid = getpid();
    if (pid>10000) // The Maximum Number of ProcessID = 32768
    {
        return 5;
    }
    else if (pid>1000)
    {
        return 4;
    }
    else
    {
        return 3;
    }
}

void ConvertToUpperCase(char* string)
{
    int j=0;
    while(string[j])
    {
        string[j]=toupper(string[j]);
        j++;
    }
}

const char* ExecuteCommand(char* Command)
{
    char* CMD = strtok(Command, " ");
    char* Parameters = strtok(NULL, " ");   
    ConvertToUpperCase(CMD);
    if (strcmp(CMD,"USER")==0)
    {
        IsUserLoggedIn = 1;
        return "230 User logged in, Proceed.";
    }
    else if (strcmp(CMD,"CWD")==0)
    {
        if (IsUserLoggedIn==0)
        {
            return "530 Not logged in.";
        }
        else if (Parameters==NULL)
        {
            return "501 Syntax error in parameters or arguments.";
        }
        else
        {
            return CWD(Parameters);
        }
    }
    else if (strcmp(CMD,"CDUP")==0)
    {
        return CDUP();
    }
    else if (strcmp(CMD,"REIN")==0)
    {
        return REIN();
    }
    else if (strcmp(CMD,"QUIT")==0)
    {
        return QUIT();
    }
    else if (strcmp(CMD,"PORT")==0)
    {
        return PORT(Parameters);
    }
    else if (strcmp(CMD,"RETR")==0)
    {
        return RETR(Parameters);
    }
    else if (strcmp(CMD,"STOR")==0)
    {
        if (IsUserLoggedIn==0)
        {
            return "530 Not logged in.";
        }
        else if (DataChannel==0)
        {
            return "425 Can't open data connection.";
        }
        else
        {
            return STOR(Parameters);
        }
    }
    else if (strcmp(CMD,"APPE")==0)
    {
        return APPE(Parameters);
    }
    else if (strcmp(CMD,"REST")==0)
    {
        return REST();
    }
    else if (strcmp(CMD,"RNFR")==0)
    {
        return RNFR(Parameters);
    }
    else if (strcmp(CMD,"RNTO")==0)
    {
        return RNTO(Parameters);
    }
    else if (strcmp(CMD,"ABOR")==0)
    {
        return ABOR();
    }
    else if (strcmp(CMD,"DELE")==0)
    {
        return DELE(Parameters);
    }
    else if (strcmp(CMD,"RMD")==0)
    {
        return RMD(Parameters);
    }
    else if (strcmp(CMD,"MKD")==0)
    {
        return MKD(Parameters);
    }
    else if (strcmp(CMD,"PWD")==0)
    {
        return PWD();
    }
    else if (strcmp(CMD,"LIST")==0)
    {
        if (IsUserLoggedIn==0)
        {
            return "530 Not logged in.";
        }
        else if (DataChannel==0)
        {
            return "425 Can't open data connection.";
        }
        else
        {
            return LIST(Parameters,1);
        }
    }
    else if (strcmp(CMD,"STAT")==0)
    {
        return STAT(Parameters);
    }
    else if (strcmp(CMD,"NOOP")==0)
    {
        return NOOP();
    }
    else if (strcmp(CMD,"BYE")==0)
    {
        SendResponseToClient("200");
        printf("Client %s Disconnected.\n",ChannelName);
        DisconnectClient();
        return "";
    }
    else if (strcmp(CMD,"ABOUT")==0)
    {
        return "Named Pipe FTP Server by Babak Tajalli Nejad and Shardul Yadav";
    }
    else
    {
        return "502 Command not implemented.";
    }
}

void NewConnectionReceived(int FD)
{
        int ProcessIDlength = GetProcessIDlength();
        ChannelName = malloc(ProcessIDlength+9);
        sprintf(ChannelName,"Channel_%d",getpid());

        ChannelName_Read = malloc(strlen(ChannelName)+4);
        sprintf(ChannelName_Read,"%s_RD",ChannelName);

        ChannelName_Write = malloc(strlen(ChannelName)+4);
        sprintf(ChannelName_Write,"%s_WR",ChannelName);

        unlink(ChannelName_Write);
        unlink(ChannelName_Read);
        mkfifo(ChannelName_Write, 0777);
        mkfifo(ChannelName_Read, 0777);
        printf("Channel Name %s sent to client.\n",ChannelName);
        write(FD,ChannelName,strlen(ChannelName)); // Send Channel name to client
        printf("Wait for connection to %s\n",ChannelName_Read);
        CommandChannel_Read = open(ChannelName_Read,O_RDONLY); 
        while ((CommandChannel_Write = open(ChannelName_Write,O_WRONLY))==-1){
            sleep(1);
        }
        fprintf(stderr,"Channels Created. Waiting for command. Channel Name : %s ",ChannelName);
        while(1)
        {
            char* cmd = malloc(100);
            read(CommandChannel_Read,cmd,100);
            fprintf(stderr,"Command Received from %s : %s\n",ChannelName,cmd);
            const char* Result = ExecuteCommand(cmd);
            fprintf(stderr,"Response Sent to %s : %s, Length: %lu\n",ChannelName,Result,strlen(Result));
             // I add 1 to the string length because the client can detect the end of string otherwise it would read some old and incorrect data at the end of string
            if (strcmp(Result,"")!=0)
            {
                SendResponseToClient(Result);
            }
            free(cmd);
        }

}

int main(int argc, char *argv[])
{
    if (argc>1)
    {
        chdir(argv[1]);
    }
    WorkingDirectory = malloc(200);
    HomeDirectory = malloc(200);
    getcwd(WorkingDirectory, 200);
    getcwd(HomeDirectory, 200);
    DataChannel = 0;
    signal(SIGINT,CTRLCCTRLV_Detected); //Register for CTRL-C signal
    signal(SIGTSTP,CTRLCCTRLV_Detected); //Register for CTRL-Z signal
    unlink("FTPConnection");
    if (mkfifo("FTPConnection",0777)!=0)
    {
        printf("Unexpected Error occured.\n Please retry.\n");
        exit(0);
    }
    printf("FTP Server Starts\n");
    CommunicationFD = open("FTPConnection",O_RDWR);
    while(1)
    {
        CommandBuffer = malloc(100);
        printf("Wait For New Connection\n");
        read(CommunicationFD,CommandBuffer,100); // wait for connection
        if (strlen(CommandBuffer)==0) 
        // When a clients closed its connection the server passed the previous read command but didn't read any data and it went to infinit loop and no wait at read command at all
        // for solving this problem we check the data if it is empty we reset the connection by closing and reopen it again
        {
            close(CommunicationFD);
            CommunicationFD = open("FTPConnection",O_RDWR);
        }
        else if (fork()==0) //Child
        {
            NewConnectionReceived(CommunicationFD);
            exit(0);
        }
        free(CommandBuffer);
    }
    return 0;
}
