#define _XOPEN_SOURCE 500
#include<ftw.h>
#include<stdlib.h>
#include<stdio.h>
#include<limits.h>
#include<errno.h>
#include<unistd.h>
#include<sys/stat.h>

int matchl(const char *filename, char *actualpath)
{
        int counterr=0;
        while(filename[counterr]==actualpath[counterr])
        {
                if(actualpath[counterr]=='\0' || filename[counterr]=='\0')
                        break;
                counterr++;
        }

	if(actualpath[counterr]==0 && filename[counterr]==47)
                return 0;
        else return -1;
}

int linksss=0;
int nftwfunc(const char *filename, const struct stat *statptr, int fileflags, struct FTW *pfwt)
{
	
	if(fileflags == FTW_SL)
	{
		char arr[] = {'L'};
		char *actualpath=realpath(filename,NULL);
		int matchervar = matchl(filename,actualpath);
		if(matchervar==0) linksss++;
		
	}
	return 0;
}

int main(int argc, char *argv[])
{
	char * startpath;
	if(argc==1)
	{
		startpath=".";
	}
	else startpath = argv[1] ;
	char *x=realpath(startpath,NULL);
	int fd_limit = 1000;
	int flags =FTW_PHYS | FTW_DEPTH;
	int ret;
	ret = nftw(x, nftwfunc, fd_limit, flags);
	printf("Found %d cycles",linksss);
	return 0;
}
