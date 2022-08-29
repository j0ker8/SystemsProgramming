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

//On Exit we close all used file descriptors and free all dynamically allocated memory
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
    //we send the bye command to the server so the server closes the client connection
    write(ChannelConnection_WR,"BYE",4);
    //call the DOExit command to free memory and close file descriptors
    DoExit();
}
// this function is used for converting all letters of a string to uppercase
void ConvertToUpperCase(char* string)
{
    int j=0;
    while(string[j])
    {
        //convert to uppercase character by character
        string[j]=toupper(string[j]);
        j++;
    }
}

void SignalReceived(int Status)
{
    //during our upload and download method (RETR, STOR and APPE) when any of the "REST", "REIN", "ABOR" commands 
    //received, a signal of type SIGTERM will be send to the realted process and that signal will cause this function
    //to be executed and it sets the parameter UploadInterrupt to 1
    //in our file transfer functions we check this parameter and if it set to 1 the process will terminate and the
    //transfering will be stopped.
    UploadInterrupt = 1;
}

//this function will be called for STOR and APPE commands to upload a file to the server
void UploadFile(const char* filepath)
{
    //check if data channel already created and opened using the PORT command
    if (DataChannel!=0)
    {
        //the uploading process will be done in a child process for not stopping the parent and still lets the parent to continue processing the commands
        if ((UploadProcess = fork()) == 0)
        {
            //Detail provided in SignalReceived function
            signal(SIGTERM,SignalReceived);
            //open the file specified to be uploaded
            int FileUploadFD = open(filepath,O_RDONLY);
            //specifies the actual data read. can be less than or equal to MAX_BUF 
            int ActualDataRead = 0;
            //allocate enough memory to buffer equal to MAX_BUF
            char* Buffer = malloc(MAX_BUF);
            fprintf(stderr,"Start Uploading the file ...\n");
            //While still more bytes to read
            while((ActualDataRead=read(FileUploadFD,Buffer,MAX_BUF))!=0)
            {
                //this is our client marker shows the position of our file cursor in file transfer functions
                ClientMarker += ActualDataRead;
                //if the process interupted it exits. More detail provided in SignalReceived function
                if (UploadInterrupt==1)
                {
                    exit(0);
                }
                else
                {
                    //writes the read buffer to data channel (Named Pipe)
                    write(DataChannel,Buffer,ActualDataRead);
                }
            }
            fprintf(stderr,"Finish Uploading the file ...\n");
            //closes and frees the memory
            close(FileUploadFD);
            free(Buffer);
            exit(0);
        }
    }
}

//this function will be called on RETR command
void DownloadFile(const char* filename)
{
    //check if the data channel already oppened
    if (DataChannel!=0)
    {
        //the upload process will be executed in child process so the parent can continue executing the commands
    if ((UploadProcess = fork()) == 0)
    {
        //allocate MAX_BUF size to the Buffer
    char* Buffer = malloc(MAX_BUF);
    //fprintf(stderr,"Start Dowloading the file / data ...\n");
    int FileDownloadD = open(filename,O_CREAT | O_WRONLY,0777);
    //Specifies the actual data read can be less than or ecual to MAX_BUF
    int ActualDataRead;
    //wait to read data from Data Channel (Named Pipe)
    //whil still there is data available
    while((ActualDataRead=read(DataChannel,Buffer,MAX_BUF))!=0)
    {
        //this is our client marker shows the position of our file cursor in file transfer functions
        ClientMarker += ActualDataRead;
        //if the process interupted exit the loop
        if (UploadInterrupt==1)
        {
            break;
        }
        else if (ActualDataRead<MAX_BUF) // this is the situation where we reached to the end of file transfer 
        {
            //data will be written to filedescriptor
            write(FileDownloadD,Buffer,ActualDataRead);
            printf("Download Complete.\n");
            break;
        }
        else
        {
            //data will be written to filedescriptor
            write(FileDownloadD,Buffer,ActualDataRead);
        }
    }
    //fprintf(stderr,"Finish Downloading the file / data ...\n");
    //close the file descriptor and free the buffer
    close(FileDownloadD);
    free(Buffer);
    exit(0);
    }
    }
}

//this function will be used for LIST command when the LIST command uses the Data Channel to transfer the data
void PrintDataChannelContent()
{
    char Buffer;
    printf("Response:\n");
    //reads the DataChannel Character by Character and writes everything to the standard output
    while(read(DataChannel,&Buffer,1)!=0)
    {
        if (Buffer == '\0') // we reached to the end of the string so we go outside the loop
        {
            break;
        }
        else
        {
            //prints the buffer in standard output
            printf("%c",Buffer);
        }
    }
    //printf("Enter a command :\n");
}

//this command will be executed when we send some specific commands to server and received the successfull message
//from the server and we start processing the client side. these commands are PORT, STOR, APPE, RETV, LIST, ...
void ProcessCommands(char* Command)
{
    //splits the commands and parameters
    char * CMD = strtok(Command, " ");
    char * Parameters = strtok(NULL, " ");
    //Converts the command to uppercase so we dont care how the user types the command and for any case we can process it
    ConvertToUpperCase(CMD);
    //th PORT command is received
    if (strcmp(CMD,"PORT")==0)
    {
        //for using the NamedPipe at the beggining of the application we asked the user to enter the server address (server folder path)
        //and we store it in ServerAddress. All the named pipe channels are created in the server folder so we have to address it for the client
        //the enough memory required to save the DataChannelName is equal to the length of ServerAddress+Parameter + 2 (one for / and one for \0 at the end of string).
        char* DataChannelName = malloc(strlen(ServerAddress)+strlen(Parameters)+2);
        //the sprintf command will concat our strings in the given format and store it in the first variable 
        sprintf(DataChannelName,"%s/%s",ServerAddress,Parameters);
        //we open the data channel and store it's desciptor to DataChannel variable
        DataChannel = open(DataChannelName,O_RDWR);
        //since we no more need the DataChannelName we free its memory
        free(DataChannelName);
    }
    else if (strcmp(CMD,"STOR")==0 || strcmp(CMD,"APPE")==0) //STOR or APPE commands Received
    {
        //execute the Fileupload function
        UploadFile(Parameters);
    }
    else if (strcmp(CMD,"REST")==0 || strcmp(CMD,"REIN")==0 || strcmp(CMD,"ABOR")==0) //REST, REIN or ABOR command received
    {
        //send the SIGTERM signal to the client process
        kill(UploadProcess,SIGTERM);
    }
    else if (strcmp(CMD,"RETR")==0) //RETR command received
    {
        //starts downloading the file
        DownloadFile(Parameters);
    }
    else if (strcmp(CMD,"LIST")==0) //LIST command received
    {
        //printing the Data Channle Content
        PrintDataChannelContent();
    }
    else if (strcmp(CMD,"BYE")==0) //BYE command Received
    {
        //Do exit and free all used memories
        DoExit();
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT,CTRLCCTRLV_Detected); //Register for CTRL-C signal
    signal(SIGTSTP,CTRLCCTRLV_Detected); //Register for CTRL-Z signal
    //allocate enough memore to ChannelName
    ChannelName = malloc(20);
    printf("FTP Client by Babak and Shardul.\n");
    printf("Please Enter The Server Address:\n");
    //allocate enough memore to ServerAddress
    ServerAddress = malloc(150); //this variable is used to store the address of the FTP Server. we need it to create our correct Named Pipe Address
    ConnectionChannel = malloc(200); //Main Connection Name
    scanf(" %[^\n]s",ServerAddress); //Ask user for entering th data. The special formula specified in scanf allows the system to accept (space) in enteries
    sprintf(ConnectionChannel,"%s/FTPConnection",ServerAddress);
    //tries to pen a connection to server
    while ((FTPConnection = open(ConnectionChannel,O_RDWR))==-1){
        printf("trying to connect to server.\n");
        sleep(1);
    }
    //since the server is waiting on the read command we write this hi command to server so server finds out new connection is received
    write(FTPConnection,"hi",2);
    printf("Connected to server.\n");
    //the server will send a unique channel name for processing our commands
    read(FTPConnection,ChannelName,20); // wait for the command channel name from the server
    printf("Received Channel Name %s.\n",ChannelName);
    //for making the program more understandable we used 2 separate named pipes for control connection
    //one for read and one for write
    //creates the read chanel name
    ChannelName_Read = malloc(strlen(ServerAddress)+1+strlen(ChannelName)+4);
    sprintf(ChannelName_Read,"%s/%s_RD",ServerAddress,ChannelName);
    //creates the write chanel name
    ChannelName_Write = malloc(strlen(ServerAddress)+1+strlen(ChannelName)+4);
    sprintf(ChannelName_Write,"%s/%s_WR",ServerAddress,ChannelName);
    //tries to open the read channel
    printf("Trying to connect to %s\n",ChannelName_Read);
    while ((ChannelConnection_WR = open(ChannelName_Read,O_WRONLY))==-1){
        sleep(1);
    }
    printf("Channel Read Created\n");
    //tries to open the write channel
    ChannelConnection_RD = open(ChannelName_Write,O_RDWR); 
    printf("Connected to Channel %s.\n",ChannelName);
    while(1)
    {
        //in an infinit loop we ask the user to enter a command
        //we pass this command to server using write channel
        //then listen to the read channel and wait until the server processes our command
        //and write some result to the read channel. we print this response to the standard output
        //and for some special commands we do the post processing in the client side using the ProcessCommands function
        char* Command = malloc(100);
        printf("Enter a command :\n");
        scanf(" %[^\n]s",Command);
        printf("Request: %s\n",Command);
        char* ResponseBuffer = malloc(100);
        //send the command to the write channel
        write(ChannelConnection_WR,Command,strlen(Command)+1);//I add one to make server detect the end of the string
        //sleep(1);
        //wait until server writes some response in the read channel
            read(ChannelConnection_RD,ResponseBuffer,100);
            
            if ((ResponseBuffer[0] == '2' && ResponseBuffer[1] == '0' && ResponseBuffer[2] == '0')
                || (ResponseBuffer[0] == '1' && ResponseBuffer[1] == '2' && ResponseBuffer[2] == '5')
                || (ResponseBuffer[0] == '1' && ResponseBuffer[1] == '1' && ResponseBuffer[2] == '0')
                || (ResponseBuffer[0] == '2' && ResponseBuffer[1] == '2' && ResponseBuffer[2] == '0')
                || (ResponseBuffer[0] == '2' && ResponseBuffer[1] == '2' && ResponseBuffer[2] == '6')
                || (ResponseBuffer[0] == '2' && ResponseBuffer[1] == '5' && ResponseBuffer[2] == '0'))
            { 
                //if any of these responses received we do the post processing commands
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