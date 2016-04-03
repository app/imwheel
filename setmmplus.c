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
#include <time.h>
#include <termios.h>

char readps2(int fd)
{
	char ch;

	while(read(fd,&ch,1) && (ch==(char)0xfa || ch==(char)0xaa))
	{
		fprintf(stderr,"<%02X>",ch&0xff);
		fflush(stdout);
	}
	fprintf(stderr,"[%02X]",ch&0xff);
	return(ch);
}

int main(int argc, char **argv)
{
	int fd;
	char ch,
		getdevtype=0xf2,
		disabledev=0xf5,
		setmmplus[]={0xe6,0xe8,0,0xe8,3,0xe8,2,0xe8,1,0xe6,0xe8,3,0xe8,1,0xe8,2,0xe8,3,},
		setupmore[7]={0xf6,0xe6,0xf4,0xf3,100,0xe8,3},
		resetps2=0xff;

	if(argc>1)
		fd=open(argv[1],O_RDWR);
	else
		fd=open("/dev/mouse",O_RDWR);

	write(fd,&disabledev,1);

	tcflush(fd, TCIFLUSH);
	write(fd,&getdevtype,1);
	sleep(1);
	ch=readps2(fd);
	fprintf(stderr,"device type=%02X(%4d)\n",ch&0xff,ch);

	write(fd,&resetps2,1);
	sleep(1);
	ch=readps2(fd);
	fprintf(stderr,"reset response=%02X(%4d)\n",ch&0xff,ch);

	tcflush(fd, TCIFLUSH);
	write(fd,&getdevtype,1);
	sleep(1);
	ch=readps2(fd);
	fprintf(stderr,"device type=%02X(%4d)\n",ch&0xff,ch);

	write(fd,&setmmplus,sizeof(setmmplus));
	write(fd,&setupmore,7);

	tcflush(fd, TCIFLUSH);
	write(fd,&getdevtype,1);
	sleep(1);
	ch=readps2(fd);
	fprintf(stderr,"device type=%02X(%4d)\n",ch&0xff,ch);

	tcflush(fd, TCIFLUSH);
	write(fd,&getdevtype,1);
	sleep(1);
	ch=readps2(fd);
	fprintf(stderr,"device type=%02X(%4d)\n",ch&0xff,ch);
	close(fd);
	return(0);
}
