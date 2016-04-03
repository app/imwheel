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
	return(ch);
}

int main(int argc, char **argv)
{
	int fd;
	char ch,
		getdevtype=0xF2,
		enabledev=0xF4,
		disabledev=0xF5,
		resetps2=0xFF;

	if(argc>1)
		fd=open(argv[1],O_RDWR);
	else
		fd=open("/dev/mouse",O_RDWR);

	fprintf(stderr,"disable\n");
	write(fd,&disabledev,1);

	fprintf(stderr,"flush\n");
	tcflush(fd, TCIFLUSH);
	fprintf(stderr,"getdev\n");
	write(fd,&getdevtype,1);
	fprintf(stderr,"sleep(1)\n");
	sleep(1);
	fprintf(stderr,"read\n");
	ch=readps2(fd);
	fprintf(stderr,"device type=%02X(%4d)\n",ch&0xff,ch);

	fprintf(stderr,"reset ps2\n");
	write(fd,&resetps2,1);
	fprintf(stderr,"sleep(1)\n");
	sleep(1);
	fprintf(stderr,"read\n");
	ch=readps2(fd);
	fprintf(stderr,"reset response=%02X(%4d)\n",ch&0xff,ch);

	fprintf(stderr,"flush\n");
	tcflush(fd, TCIFLUSH);
	fprintf(stderr,"getdev\n");
	write(fd,&getdevtype,1);
	fprintf(stderr,"sleep(1)\n");
	sleep(1);
	fprintf(stderr,"read\n");
	ch=readps2(fd);
	fprintf(stderr,"device type=%02X(%4d)\n",ch&0xff,ch);

	close(fd);
	return(0);
}
