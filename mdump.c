#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifndef O_SYNC
#define O_SYNC 0
#endif

#define SIGNOF(a) (a<0?-1:1)

char readps2(int fd)
{
	char ch;

	while(read(fd,&ch,1) && (ch==(char)0xfa || ch==(char)0xaa))
	{
		printf("<%02X>",ch&0xff);
		fflush(stdout);
	}
	return(ch);
}

int main(int argc, char **argv)
{
	int fd;
	char ch;
	int len=3,i,j;
#ifdef HAVE_GETTIMEOFDAY
	struct timeval tv,tv2;
	long t;
#endif

	if(argc>1)
		fd=open(argv[1],O_RDWR,O_NOCTTY|O_SYNC);
	else
		fd=open("/dev/mouse",O_RDWR,O_NOCTTY|O_SYNC);
	if(argc>2)
		len=atoi(argv[2]);
#ifdef HAVE_GETTIMEOFDAY
	gettimeofday(&tv2,NULL);
#endif
	ch=0xf2;
	write(fd,&ch,1);
	ch=readps2(fd);
	printf("device type=%02X(%4d)\n",ch&0xff,ch);
	while(read(fd,&ch,1))
	{
#ifdef HAVE_GETTIMEOFDAY
		gettimeofday(&tv,NULL);
		t=(tv.tv_sec-tv2.tv_sec)*1000000L+(tv.tv_usec-tv2.tv_usec);
		printf("%10ld (%3ldMHz) : ",t,1000000/t);
		tv2.tv_sec=tv.tv_sec;
		tv2.tv_usec=tv.tv_usec;
#endif
		for(i=1; i<=len; i++)
		{
			printf("%02X(%4d)",ch&0xff,ch);
			if(argc>3 || i>1)
			{
				printf("=");
				for(j=7;j>=0;j--)
					printf("%01d",(ch>>j)&1);
				printf("=%2d,%2d",(ch>>4), (ch&0x0F)<<28>>28);
				printf(")  ");
			}
			if(i<len)
				if(!read(fd,&ch,1))
					break;
		}
		//printf("\033[K\033[H");
		//fflush(stdout);
		printf("\n");
	}
	close(fd);
	return(0);
}
