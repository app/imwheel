#include <stdio.h>

int main(int argc, char **argv)
{
	char *device="/dev/psaux";
	FILE *f;
	char buf[2]={0xF3, 100};

	if(!(f=fopen(device, "w")))
	{
		perror(device);
		exit(1);
	}
	if(argc>1)
		buf[1]=atoi(argv[1]);
	fwrite(buf,1,2,f);
	fclose(f);
}
