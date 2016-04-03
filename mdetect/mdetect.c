#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <termios.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#define openps2 open(mousedev,O_RDWR|O_SYNC|O_NOCTTY)
//#define flushps2 tcflush(fd, TCIFLUSH)
#define flushps2 flushfd(fd)
#define K(a) ((a)*1024)

typedef unsigned char uchar;

char readps2();
void dumpps2();
void diag1();
void diag2();
void diag3();
int filterps2buf(uchar *buf, int len);
void flushfd(int fd);

char DisableDev		=0xF5;
char GetDeviceType	=0xF2;
char SetStdPS2		=0xFF;

char *mousedev="/dev/psaux";

// mouse config data (detected and used)
int len=0;
char mask[16];
char **buttons;
int numbuttons=0;

// local data
int fd;
uchar ch;

int main(int argc, char **argv)
{
	fd=openps2;
	if(fd<0)
		return(1);
	//printf("DisableDev\n");
	write(fd,&DisableDev,1);
	flushps2;
	//printf("SetStdPS2\n");
	write(fd,&SetStdPS2,1);
	readps2();
	flushps2;
	//printf("GetDeviceType\n");
	write(fd,&GetDeviceType,1);
	readps2();
	close(fd);
	
	fd=openps2;
	if(fd<0)
		return(1);
	diag1();
	diag2();
	diag3();
	close(fd);
	return(0);
}

char readps2()
{
	while(read(fd,&ch,1) && (ch==0xfa || ch==0xaa));
	/*
	{
		printf("<%02X>",ch);
		fflush(stdout);
	}
	*/
	return(ch);
}

void dumpps2()
{
	printf("Dump\n");
	while(read(fd,&ch,1))
	{
		printf(" %02x ",ch);
		fflush(stdout);
	}
}

int filterps2buf(uchar *buf, int l)
{
	int i;

	for(i=0;i<l;i+=len)
	{
		while(buf[i]==0xAA || buf[i]==0xFA)
		{
			l--;
			memmove(buf+i,buf+1+i,l);
		}
		if(!len)
			break;
	}
	return(l);
}

void unmaskps2(uchar *buf, int l)
{
	int i,j;
	
	for(i=0;i<l;i+=len)
		for(j=0;j<len && j+i<l;j++)
			buf[j+i]^=mask[j];
}

int numbits(uchar *buf)
{
	int byte,ct;
	uchar bit;

	for(ct=byte=0;byte<len;byte++)
		for(bit=1;bit;bit=bit<<1)
			ct+=(buf[byte]&bit?1:0);
	return(ct);
}

void andbits(uchar *buf, uchar *buf2, int l)
{
	int byte;

	for(byte=0;byte<l;byte++)
		buf[byte%len]&=buf2[byte];
}

void orbits(uchar *buf, uchar *buf2, int l)
{
	int byte;

	for(byte=0;byte<l;byte++)
		buf[byte%len]|=buf2[byte];
}

/*
int isadd(int len, int n)
{
	int i;

	for(i=len;i>=n;i=i-n);
	return(i);
}
*/

void flushfd(int fd)
{
	int oldflags;
	uchar buf[K(1)];

	oldflags=fcntl(fd,F_GETFL);
	fcntl(fd,F_SETFL,oldflags|O_NONBLOCK);
	while(read(fd,buf,K(1)) > 0);
	fcntl(fd,F_SETFL,oldflags);
}

int getbit(uchar *buf, int n)
{
	return((buf[n/8]>>(n%8))&1);
}

int nextbit(int n, uchar *in, uchar *mask)
{
	int i;

	for(i=n+1; i/8<len && getbit(in,i) && !getbit(mask,i);i++);
	if(i/8>=len)
		return(-1);
	else
		return(i);
}

/*
unsigned long long numcombo(int n)
{
	unsigned long long c;
	int i;

	for(i=1,c=1;i<n;i++)
	{
		c*=i;
		printf("%d) %Lu\n",i,c);
	}
	return(c);
}

int bitcombo(uchar *all, int cur, int ***ialist)
{
	static int *iacur;
	static uchar *used;
	static int ialistlen, n;
	int i;

	// setup
	if(!cur)
	{
		int nc;

		n=numbits(all);
		nc=numcombo(n);
		iacur=malloc(n*sizeof(int));
		*ialist=malloc(nc*sizeof(int*));
		for(i=nc-1;i>=0;i--)
			*ialist[i]=malloc(n*sizeof(int));
		ialistlen=0;
		used=malloc(len);
	}
	// work
	for(i=nextbit(-1,all,used);i>=0;i=nextbit(i,all,used))
	{
		iacur[cur]=i;
		if(numbits(used)==n)
		{ // end
			memcpy(*ialist[ialistlen],iacur,len);
			ialistlen++;
		}
		else
		{ // recurse
			used[i/8]|=i%8;
			bitcombo(all,cur+1,ialist);
			used[i/8]^=i%8;
		}
	}
	// cleanup
	if(!cur)
	{
		free(iacur);
		free(used);
	}
	return(ialistlen);
}
*/

////////////////////////////////////////////////////////////////////////////////
// Packet Length
// Mask
void diag1()
{
	uchar buf[K(1)];
	int i;
	
	printf("[ Mouse Packet Length & Mouse Mask ]\n");
	printf("Lift mouse in air.\nYou will be asked to press the left mouse button once.\nDo NOT shake mouse during test or motion data will interfere.\n");
	printf("Press ENTER when ready: ");
	fflush(stdout);
	scanf("%*c");

	//fd=openps2;
	flushps2;

	printf("Press left mouse button once then press ENTER: ");
	fflush(stdout);
	scanf("%*c");

	i=read(fd,buf,K(1));
	//close(fd);

	len=filterps2buf(buf,i);

	for(i=0;i<len;i++)
		printf("%02X  %s",buf[i],(!((i+1)%(len/2))?"\n":""));

	len=len/2;
	printf("Packet Length = %d\n",len);

	for(i=0;i<len;i++)
		mask[i]=buf[i]&buf[i+len];

	printf("Mask = 0x");
	for(i=0;i<len;i++)
		printf("%02X%s",mask[i],(!((i+1)%len)?"\n":""));
	printf("\n");
}

////////////////////////////////////////////////////////////////////////////////
// Mouse Buttons
void diag2()
{
	uchar buf[K(1)];
	int i,l,j;

	printf("[ Mouse Buttons ]\n");
	printf("This mouse buttons catagory also includes other things that act like buttons.\n");
	printf("Wheels that click are included in this catagory.\n");
	printf("Wheels that go in two directions most likely act like two separate buttons.\n");
	while(numbuttons<1)
	{
		printf("How many mouse buttons do you want to configure: ");
		fflush(stdout);
		scanf("%d%*c",&numbuttons);
		if(numbuttons<1)
			printf("Oh please...\n");
	}
	buttons=malloc(len*numbuttons);
	printf("Lift mouse in air and be prepared to press (or whatever) each button in order.\n");
	printf("It's important that you don't shake the mouse durring these configurations.\n");
	for(i=0;i<numbuttons;i++)
	{
		buttons[i]=malloc(len);
		do {
			printf("Press ENTER when ready: ");
			fflush(stdout);
			scanf("%*c");

			//fd=openps2;
			flushps2;

			printf("Press mouse button %d once then press ENTER: ",i+1);
			fflush(stdout);
			scanf("%*c");

			l=read(fd,buf,K(1));
			//close(fd);
			for(j=0;j<l;j++)
				printf("%02X  %s",buf[j],(!((j+1)%len)?"\n":""));
			
			l=filterps2buf(buf,l);
			unmaskps2(buf,l);

			if(l != len*2)
				printf("Please do it again, the wrong amount of data was received!\n");
		} while(l != len*2);
		memcpy(buttons[i],buf,len);
		printf("Mouse button %d = 0x",i+1);
		for(j=0;j<len;j++)
			printf("%02X%s",buttons[i][j],(!((j+1)%len)?"\n":""));
	}
	printf("\n");
}

////////////////////////////////////////////////////////////////////////////////
// Mouse Motion
void diag3()
{
	uchar buf[K(16)],all[16];
	int i,l,n;
	//int **ialist,ialistlen;
	
	printf("[ Mouse Motion ]\n");
	printf("Put the mouse down.\nYou will be asked to move the mouse in a circle.\nThe circle should be drawn starting on the right side of the circle,\ngoing counterclockwise.\nDon't use any mouse buttons during this configuration.\n");

	do {
		printf("Press ENTER when ready: ");
		fflush(stdout);
		scanf("%*c");

		//fd=openps2;
		//sleep(1);
		flushps2;

		printf("Circle the mouse once (see above) then press ENTER: ");
		fflush(stdout);
		scanf("%*c");

		l=read(fd,buf,K(16));
		//close(fd);

		printf("Raw:\n");
		for(i=0;i<l;i++)
			printf("%02X  %s",buf[i],(!((i+1)%len)?"\n":""));

		l=filterps2buf(buf,l);
		unmaskps2(buf,l);

		printf("\nFiltered:\n");
		for(i=0;i<l;i++)
			printf("%02X  %s",buf[i],(!((i+1)%len)?"\n":""));

		if(l%len)
			printf("\nPlease do it again, the wrong amount of data was received!\n");
	} while(l%len);

	memset(all,0,sizeof(all));
	orbits(all,buf,l);
	printf("\nall=");
	for(i=0;i<len;i++)
		printf("%02X  %s",all[i],(!((i+1)%len)?"\n":""));

	n=numbits(all);
	printf("numbits(all)=%d\n",n);
	//printf("numcombo(%d)=%Lu\n",n,numcombo(n));

	// Circle detect
	//ialistlen=bitcombo(all,0,&ialist); // 355687428096000 combos! no way!

	printf("\n");
}
