#include<stdio.h>
#include<unistd.h>
#include<fcntl.h>
#include<stdlib.h>
#include<string.h>
int parseheader(char rawheader[], int *headerdata,int headerlength)
{
        //printf("Running function");
        int offset=0;
        int arr[3];
        int countheaderelements=0;
	//printf("%s\n",rawheader);
	int i=0;
        while(countheaderelements<3 && i<headerlength){
                if(rawheader[i]==' ' || rawheader[i]=='\n' || rawheader[i]=='\t' || rawheader[i]=='\v' || rawheader[i]=='\f')
                {
                        //printf("Whitespace detected");
                        i++;
                        offset++;
                        continue;
                }
                else if(rawheader[i] >= '0' && rawheader[i] <= '9') {
                        int number=0;
                        while(rawheader[i] >= '0' && rawheader[i] <= '9')
                        {
                                //printf("Char : %d\n",*rawheader[i]-48);
                                number=(number*10)+(rawheader[i]-48);
                                //printf("Number : %d\n",number);
                                i++;
                                offset++;
                        }
                        arr[countheaderelements]=number;
                        //printf("%d\n",number);
                        countheaderelements++;
                }
                else{ 
			printf("In else function");
			return -1;
		}
        }

        if (countheaderelements == 3)
        {
                headerdata[0]=arr[0];
                headerdata[1]=arr[1];
                headerdata[2]=arr[2];
                return offset;
        }
        else return -1;
}


int writeImage(int fd, int width, int height, unsigned char *image){
        char header[50];
        sprintf(header,"P5\n%d %d\n255\n",width,height);
        write(fd,header,strlen(header));
        write(fd,image,(width*height));
}


int reversepicture(int fd, int width, int height,int headerlength,char *writefile){
	unsigned char * reverseddata = malloc(width*height*sizeof(unsigned char));
	unsigned char * lineinput = malloc(width * sizeof(unsigned char));
	int writelinepointer = 0;
	for(int j=height-1;j>=0; j--)
	{
		int writeposition = (j*width) + headerlength + 1;
		lseek(fd,writeposition,SEEK_SET);
		if(read(fd,lineinput,width)!=-1){
			for(int k = 0; k < width; k++){
				reverseddata[(writelinepointer*width) + k] = lineinput[k];
			}
		}
		writelinepointer++;
	}
	close(fd);
	int writefiledes = open(writefile, O_CREAT | O_RDWR, 0777);
	writeImage(writefiledes, width, height, reverseddata);
}


int comparearrays(const char *a, const char *b)
{
	while(*a && (*a == *b)){
		a++;
		b++;
	}
	return *(const char *)a - *(const char *)b;
}

int main(int argc, char **argv)
{

	if (argc==0){
	printf("Processing error.1");
	return 0;
	}
	char *filepath=argv[1];
	int fdr = open(filepath,O_RDONLY);
	if(fdr!=-1){
		char *magicnumber;
		lseek(fdr,0,SEEK_SET);
		int readmagicnumber = read(fdr,magicnumber,2);
		printf("%c\n",magicnumber[1]);
		if(comparearrays(magicnumber,"P5")==0){
			char buff[20];
			int opheader = read(fdr,buff,20);
			int headerdata[]={0,0,0};
			int offset=parseheader(buff,headerdata,20);
			int headeroffset=offset+4;
			int width = headerdata[0];
			int height = headerdata[1];
			int sekend=lseek(fdr,0,SEEK_END);
			int datapresent = sekend-(offset+4);
			int totaldataneeded = width*height;
			if(datapresent!=totaldataneeded)
			{
				printf("Processing error.2");
				return 1;
			}
			else {
				lseek(fdr,0,SEEK_SET);
				reversepicture(fdr,width,height,headeroffset,filepath);
			}

		}
		else{
			printf("Processing error.3");
			close(fdr);
			return -1;
		}
	}
	else printf("Processing error.4");
	close(fdr);
}
