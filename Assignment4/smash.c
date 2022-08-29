#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<string.h>

char **split(char *string, char *seperators, int *count){ //This function splits the string based on the seperators and returns the seperated elements in an array, stores the length of array in *count. 
  int len = strlen(string);
  
  *count = 0;
  
  int i = 0;
  while (i < len)
  {
    while (i < len)
    {
      if (strchr(seperators, string[i]) == NULL)
        break;
      i++;
    }
    
    int old_i = i;
    while (i < len)
    {
      if (strchr(seperators, string[i]) != NULL)
        break;
      i++;
    }

    if (i > old_i) *count = *count + 1;
  }
  
  char **strings = malloc(sizeof(char *) * *count);
  
  i = 0;

  char buffer[16384];
  int string_index = 0;
  while (i < len)
  {
    while (i < len)
    {
      if (strchr(seperators, string[i]) == NULL)
        break;
      i++;
    }
    
    int j = 0;
    while (i < len)
    {
      if (strchr(seperators, string[i]) != NULL)
        break;
      
      buffer[j] = string[i];
      i++;
      j++;
    }
    
    if (j > 0)
    {
      buffer[j] = '\0';

      int to_allocate = sizeof(char) *
                        (strlen(buffer) + 1);
      
      strings[string_index] = malloc(to_allocate);
      
      strcpy(strings[string_index], buffer);
      
      string_index++;
    }
  }

  return strings;
}

int checkandprocesscd(char * input){ // this functions checks for "cd" command and changes the directory based upon the arguments.
  
  int chdirargcount = 0;
  char **chdcmd = split(input," ",&chdirargcount); //storing the different arguements of the cd command.
  
  if(chdirargcount==0) // if the command is empty, return.
  {
      return 0;
  }
  else {
      if(chdcmd[0][0]=='c' && chdcmd[0][1]=='d' && strlen(chdcmd[0])==2){ //if the command is "cd"
          if(chdirargcount==1) // if there are no arguements given with cd, change the directory to HOME
          {
              char * homedir = getenv("HOME");
              if(chdir(homedir)==-1){
                  printf("Error : Not able to change directory");
                  return 0;
              }
              return 1;
          }
          else if(chdirargcount==2){ // if there are 2 arguments i.e cd and the arguement for cd.
              if(chdir(chdcmd[1])==-1)
              {
                  printf("Error : Directory name incorrect");
                  return 0;
              }
              return 1;
          }
          else{
              printf("Error in cd : incorrect arguements");
          }
      }
      return 0;
  }
  
}

int checkdotslash(const char * input){ // this function just checks if there is ./ present in some line.
    if (input[0]=='.' && input[1]=='/'){
        return 1;
    }
    else{
        return 0;
    }
    return 0;
}

char * processdotslash(char * input){ // this function processes the dot slash command.
    
    char buff[100];
    
    if(checkdotslash(input))  // if dot slash exists.
    {
        input++; // skip .
        input++; // skip /
        getcwd(buff,100); // store current working directory in buff
        int bufflen = strlen(buff);
        int inputlength = strlen(input);
        int totallen;
        
        int slashexists=0;
        if(*input=='/'){ // the input is .is like .//shell/hello.c
            slashexists=1; //flag for if there is another slash at the start.
            totallen=bufflen+inputlength;
        }
        else{ // if the input is like ./shell/hello.c
            slashexists=0;
            totallen=bufflen+inputlength+1;
        }
        
        char * totalpath = malloc(totallen*sizeof(char));
        
        if(slashexists==1) // transform the input to /home/admin/shell/hello.c if slash already existed with the input
        {
            strcat(totalpath,buff); // append current working directory
            strcat(totalpath,input); // append the user entered path with current working directory.
        }
        else{ // transform the input to /home/admin/shell/hello.c if the slash didn't exist with the input.
            strcat(totalpath,buff); // append curent working directory
            char * slash = malloc(sizeof(char)); 
            *slash='/';
            strcat(totalpath,slash); // append slash
            strcat(totalpath,input); // append remaining user entered path
        }
        return totalpath;
    }
    else return input; // if there is no ./ at start just return the input.
}

void evalinputs(char ** argc, int pipedcmdcount){ // Pass the pipe seperated and non whitespace seperated commands along with the count of the commands.
    pid_t childpid;
    int fdA[2];
    int fdB[2];
    
    int nPipes = pipedcmdcount - 1; // number of pipes is number of piped commands - 1
    
    if(nPipes>0){
    
      for (int i=0;i < pipedcmdcount;i++){ // for every seperate commands joined by pipes
        
        int cmdargcount = 0;
        char **cmd = split(argc[i]," ",&cmdargcount); // splitting the arguments
        
        int dotslash = checkdotslash(*cmd); // if the command has ./ in front of it/
        if(dotslash)
        {
          *cmd=processdotslash(cmd[0]); // return the full path name of the command that is to be executed because there is ./ associated with it.
        }
        
        char *cmdargarray[cmdargcount+1]; // creating an array for the second argument of exec() functions which has NULL at the end of it.
        for (int z=0; z < cmdargcount ; z++){ 
          cmdargarray[z]=cmd[z];
        }
        cmdargarray[cmdargcount]=NULL; // appending NULL at the end of all the arguments.
        
        if (i%2==0){ // we are using 2 alternating pipes between the commands. For even commands, pipeB is used, for odd commands, pipeA is used.
            pipe(fdB);
        }
        else{
          pipe(fdA);
        } 
        
        childpid = fork();
        
        if(childpid==-1){
            exit(1);
        }
        
        else if(childpid==0){ // Child process code
            
          if (i==0){
                dup2(fdB[1],1);
            }
          else if (i == nPipes){ // if it's the last pipe. No need to redirect to another pipe
                if(i%2==0){
                    dup2(fdA[0],0);
                }
                else {
                  dup2(fdB[0],0);
                }
            }
          else { // if the pipe is in the middle, we need to take input from one and transfer to another depending on whether it's odd or even
                if(i%2==0){
                    dup2(fdA[0],0);
                    dup2(fdB[1],1); 
                }
                
                else {
                    dup2(fdB[0],0);
                    dup2(fdA[1],1);
                }
                
            }
            
          if(dotslash==0){ // if the command does not have ./ inside it, use execvp function since PATH is already added in execvp
              if (execvp(cmdargarray[0],cmdargarray)<0)
              {
                  printf("Error in execution");
              }
              
            }
          
          else{ // if the command has ./ use execv function
            if(execv(cmdargarray[0],cmdargarray)<0){
              printf("Error in execution");
            }
          }
            
          }
        
        else{  // Parent process code, closing appropriate file descriptors depending upon the case.
            if(i==0){
                close(fdB[1]);
            }
            else if(i==nPipes){
                if(i%2==0){
                    close(fdB[1]);
                }
                else close(fdA[1]);
            }
            else {
                if (i%2==0){
                    close(fdA[0]);
                    close(fdB[1]);
                }
                else {
                    close(fdB[0]);
                    close(fdA[1]);
                }
            }
            
            int status;
            waitpid(childpid,&status,0); // wait for the child to die
        }
      }
    } // if there are pipes in the input
    
    else if (nPipes==0) { // if there are no pipes in the input
      
      if(checkandprocesscd(argc[0])) // process cd command if the command is cd
      {
        return;
      }
      else {
        
      childpid = fork(); // since we don't have any pipes, it's a single command, we have to fork just once
      
      if(childpid==0){ // processing for the child
        
        // just processing the input arrays for exec function
        int cmdargcount = 0;
        char **cmd = split(argc[0]," ",&cmdargcount);
        
        int dotslash = checkdotslash(*cmd);
        if(dotslash)
        {
          *cmd=processdotslash(cmd[0]);
        }
        
        char *cmdargarray[cmdargcount+1];
        for (int z=0; z < cmdargcount ; z++){
          cmdargarray[z]=cmd[z];
        }
        cmdargarray[cmdargcount]=NULL;
        // processing input arrays finished here
        
        
        
        if(dotslash==0){ // if there is no ./ use execvp
          if (execvp(cmdargarray[0],cmdargarray)<0)
          {
                printf("Error in execution");
          }
        }
        else // if there is ./ use execv
        {
          if(execv(cmdargarray[0],cmdargarray)<0)
          {
            printf("Error in execution");
          }
        }
        
        for (int z=0; z < cmdargcount ; z++){
          free(cmd[z]);
        }
        free(cmd);
        free(cmdargarray);
      }
      
      waitpid(childpid,NULL,0); // wait for the child to die.
      }
      
    } 
    
    return;
}

void processline(char * input){ // processing a raw line
    
    int countseqcmds = 0;
    char ** seqcmds = split(input,";\n",&countseqcmds); // splitting commands on \n and ; characters
    
    for (int i=0; i < countseqcmds ; i++)
    {
      int countpipedcommands = 0;
      char ** pipcmds = split(seqcmds[i],"|",&countpipedcommands);  // Here we are splitting the commands that are seperated by pipes and we will process them in the evalinputs function
      
      evalinputs(pipcmds,countpipedcommands); // evaluating all pipe seperated commands.
      
      for (int i = 0; i < countpipedcommands; i++)
      free(pipcmds[i]);
     
      free(pipcmds); 
    }
    
    for (int i = 0; i < countseqcmds; i++)
    free(seqcmds[i]);
  
    free(seqcmds);
}


int main(){
    
    while(1){ //infinite loop to ask user for input
        char * input = malloc(100*sizeof(char));
        printf("input>");
        fgets(input,100,stdin);
        processline(input); // process the line
    }
    return 0;
}