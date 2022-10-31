#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/signal.h>

#ifndef MAX_BUF
#define MAX_BUF 1000
#endif

int DataChannel = 0;
int ServerMarker = 0;
int UploadProcess =0;
int ClientMarker = 0;
int UploadInterrupt=0;
char* ChannelName;
char* ChannelName_Read;
char* ChannelName_Write;
char* ServerAddress;
char* ConnectionChannel;
int FTPConnection;
int ChannelConnection_WR;
int ChannelConnection_RD;

void DoExit()
{
    if (DataChannel > 0)
    {
        close(DataChannel);
    }
    free(ChannelName);
    free(ChannelName_Read);
    free(ChannelName_Write);
    free(ServerAddress);
    free(ConnectionChannel);
    close(FTPConnection);
    close(ChannelConnection_WR);
    close(ChannelConnection_RD);
    exit(0);
}
// CTRL-Z or CTRL-C signal received
void CTRLCCTRLV_Detected(int signo)
{
    printf("\nConnection Closed.\n");
    write(ChannelConnection_WR,"BYE",4);
    DoExit();
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

void SignalReceived(int Status)
{
    UploadInterrupt = 1;
}

void UploadFile(const char* filepath)
{
    if (DataChannel!=0)
    {
        if ((UploadProcess = fork()) == 0)
        {
            signal(SIGTERM,SignalReceived);
            int FileUploadFD = open(filepath,O_RDONLY);
            int ActualDataRead = 0;
            char* Buffer = malloc(MAX_BUF);
            fprintf(stderr,"Start Uploading the file ...\n");
            while((ActualDataRead=read(FileUploadFD,Buffer,MAX_BUF))!=0)
            {
                ClientMarker += ActualDataRead;
                if (UploadInterrupt==1)
                {
                    exit(0);
                }
                else
                {
                    write(DataChannel,Buffer,ActualDataRead);
                }
            }
            fprintf(stderr,"Finish Uploading the file ...\n");
            close(FileUploadFD);
            free(Buffer);
            exit(0);
        }
    }
}

void DownloadFile(const char* filename)
{
    if (DataChannel!=0)
    {
    if ((UploadProcess = fork()) == 0)
    {
    char* Buffer = malloc(MAX_BUF);
    //fprintf(stderr,"Start Dowloading the file / data ...\n");
    int FileDownloadD = open(filename,O_CREAT | O_WRONLY,0777);
    int ActualDataRead;
    while((ActualDataRead=read(DataChannel,Buffer,MAX_BUF))!=0)
    {
        ClientMarker += ActualDataRead;
        if (UploadInterrupt==1)
        {
            break;
        }
        else if (ActualDataRead<MAX_BUF)
        {
            write(FileDownloadD,Buffer,ActualDataRead);
            printf("Download Complete.\n");
            break;
        }
        else
        {
            write(FileDownloadD,Buffer,ActualDataRead);
        }
    }
    //fprintf(stderr,"Finish Downloading the file / data ...\n");
    close(FileDownloadD);
    free(Buffer);
    exit(0);
    }
    }
}

void PrintDataChannelContent()
{
    char Buffer;
    printf("Response:\n");
    while(read(DataChannel,&Buffer,1)!=0)
    {
        if (Buffer == '\0')
        {
            break;
        }
        else
        {
            printf("%c",Buffer);
        }
    }
    //printf("Enter a command :\n");
}

void ProcessCommands(char* Command)
{
    char * CMD = strtok(Command, " ");
    char * Parameters = strtok(NULL, " ");
    ConvertToUpperCase(CMD);
    if (strcmp(CMD,"PORT")==0)
    {
        char* DataChannelName = malloc(strlen(ServerAddress)+strlen(Parameters)+2);
        sprintf(DataChannelName,"%s/%s",ServerAddress,Parameters);
        DataChannel = open(DataChannelName,O_RDWR);
        free(DataChannelName);
    }
    else if (strcmp(CMD,"STOR")==0 || strcmp(CMD,"APPE")==0)
    {
        UploadFile(Parameters);
    }
    else if (strcmp(CMD,"REST")==0 || strcmp(CMD,"REIN")==0 || strcmp(CMD,"ABOR")==0)
    {
        kill(UploadProcess,SIGTERM);
    }
    else if (strcmp(CMD,"RETR")==0)
    {
        DownloadFile(Parameters);
    }
    else if (strcmp(CMD,"LIST")==0)
    {
        PrintDataChannelContent();
    }
    else if (strcmp(CMD,"BYE")==0)
    {
        DoExit();
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT,CTRLCCTRLV_Detected); //Register for CTRL-C signal
    signal(SIGTSTP,CTRLCCTRLV_Detected); //Register for CTRL-Z signal
    ChannelName = malloc(20);
    printf("FTP Client by Babak and Shardul.\n");
    printf("Please Enter The Server Address:\n");
    ServerAddress = malloc(150);
    ConnectionChannel = malloc(200);
    scanf(" %[^\n]s",ServerAddress);
    sprintf(ConnectionChannel,"%s/FTPConnection",ServerAddress);
    while ((FTPConnection = open(ConnectionChannel,O_RDWR))==-1){
        printf("trying to connect to server.\n");
        sleep(1);
    }
    write(FTPConnection,"hi",2);
    printf("Connected to server.\n");
    read(FTPConnection,ChannelName,20); // wait for the command channel name from the server
    printf("Received Channel Name %s.\n",ChannelName);

    ChannelName_Read = malloc(strlen(ServerAddress)+1+strlen(ChannelName)+4);
    sprintf(ChannelName_Read,"%s/%s_RD",ServerAddress,ChannelName);

    ChannelName_Write = malloc(strlen(ServerAddress)+1+strlen(ChannelName)+4);
    sprintf(ChannelName_Write,"%s/%s_WR",ServerAddress,ChannelName);

    printf("Trying to connect to %s\n",ChannelName_Read);
    while ((ChannelConnection_WR = open(ChannelName_Read,O_WRONLY))==-1){
        sleep(1);
    }
    printf("Channel Read Created\n");
    ChannelConnection_RD = open(ChannelName_Write,O_RDWR); 
    printf("Connected to Channel %s.\n",ChannelName);
    while(1)
    {
        char* Command = malloc(100);
        printf("Enter a command :\n");
        scanf(" %[^\n]s",Command);
        printf("Request: %s\n",Command);
        char* ResponseBuffer = malloc(100);
        write(ChannelConnection_WR,Command,strlen(Command)+1);//I add one to make server detect the end of the string
        //sleep(1);
            read(ChannelConnection_RD,ResponseBuffer,100);
            if ((ResponseBuffer[0] == '2' && ResponseBuffer[1] == '0' && ResponseBuffer[2] == '0')
                || (ResponseBuffer[0] == '1' && ResponseBuffer[1] == '2' && ResponseBuffer[2] == '5')
                || (ResponseBuffer[0] == '1' && ResponseBuffer[1] == '1' && ResponseBuffer[2] == '0')
                || (ResponseBuffer[0] == '2' && ResponseBuffer[1] == '2' && ResponseBuffer[2] == '0')
                || (ResponseBuffer[0] == '2' && ResponseBuffer[1] == '2' && ResponseBuffer[2] == '6')
                || (ResponseBuffer[0] == '2' && ResponseBuffer[1] == '5' && ResponseBuffer[2] == '0'))
            { 
                //200 Command Successfull
                ProcessCommands(Command);
                printf("Response: %s\n",ResponseBuffer);
            }
            else
            {
                printf("Response: %s\n",ResponseBuffer);
            }
        free(Command);
        free(ResponseBuffer);
    }
}
