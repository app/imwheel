/* Intellimouse Wheel Thinger Utilities
 * Copylefted under the GNU Public License
 * Author : Jonathan Atkins <jcatki@jonatkins.org>
 * PLEASE: contact me if you have any improvements, I will gladly code good ones
 */
#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <regex.h>
#include <termios.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <errno.h>
#if 0
#include <sys/select.h>
#else
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#endif
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#define UTIL_C
#include "util.h"
#undef UTIL_C
#include "imwheel.h"

#ifndef HAVE_STRTOL
#define strtol(a,b,c) atoi(a)
#endif

#define RCFILE "/.imwheelrc"

typedef void (*sighandler_t)(int);

int buttons[NUM_BUTTONS+1]={4,5,6,7,8,9,0};
const char *button_names[]={
	"Up",
	"Down",
	"Left",
	"Right",
	"Thumb1",
	"Thumb2"
};
int statebits[STATE_MASK+1];
char *wname=NULL;
XClassHint xch={NULL,NULL};
struct WinAction *wa=NULL;
int num_wa=0;
#ifdef DEBUG
int debug=1;
#else
int debug=0;
#endif
struct Trans masktrans[MAX_MASKTRANS]=
{
	{"ShiftMask",	ShiftMask},
	{"LockMask",	LockMask},
	{"ControlMask",	ControlMask},
	{"Mod1Mask",	Mod1Mask}, //Alt
	{"Mod2Mask",	Mod2Mask},
	{"Mod3Mask",	Mod3Mask},
	{"Mod4Mask",	Mod4Mask},
	{"Mod5Mask",	Mod5Mask},
};
const int reps[1<<NUM_STATES]=
{
	1,			//None
	1,			//ShiftMask
	2,			//          ControlMask
	5,			//ShiftMask|ControlMask
	10,			//                      Mod1Mask
	1,			//ShiftMask|            Mod1Mask
	20,			//          ControlMask|Mod1Mask
	50			//ShiftMask|ControlMask|Mod1Mask
};
const char *keys[NUM_BUTTONS][1<<NUM_STATES]=
{
	{
		"Page_Up",		//None
		"Up",			//ShiftMask
		"Page_Up",		//          ControlMask
		"Page_Up",		//ShiftMask|ControlMask
		"Left",			//                      Mod1Mask
		"Left",			//ShiftMask|            Mod1Mask
		"Left",			//          ControlMask|Mod1Mask
		"Left"			//ShiftMask|ControlMask|Mod1Mask
	},
	{
		"Page_Down",	//None
		"Down",			//ShiftMask
		"Page_Down",	//          ControlMask
		"Page_Down",	//ShiftMask|ControlMask
		"Right",		//                      Mod1Mask
		"Right",		//ShiftMask|            Mod1Mask
		"Right",		//          ControlMask|Mod1Mask
		"Right"			//ShiftMask|ControlMask|Mod1Mask
	},
	{
		"Left",                 //None
		"Home",                 //ShiftMask
		"Left",                 //          ControlMask
		"Left",                 //ShiftMask|ControlMask
		"Left",                 //                      Mod1Mask
		"Left",                 //ShiftMask|            Mod1Mask
		"Left",                 //          ControlMask|Mod1Mask
		"Left"                  //ShiftMask|ControlMask|Mod1Mask
	},
	{
		"Right",                //None
		"End",                  //ShiftMask
		"Right",                //          ControlMask
		"Right",                //ShiftMask|ControlMask
		"Right",                //                      Mod1Mask
		"Right",                //ShiftMask|            Mod1Mask
		"Right",                //          ControlMask|Mod1Mask
		"Right"                 //ShiftMask|ControlMask|Mod1Mask
	},
	{
		"Page_Down",	//None
		"Page_Up",		//ShiftMask
		"Down",			//          ControlMask
		"Up",			//ShiftMask|ControlMask
		"Down",			//                      Mod1Mask
		"Up",			//ShiftMask|            Mod1Mask
		"Down",			//          ControlMask|Mod1Mask
		"Up"			//ShiftMask|ControlMask|Mod1Mask
	},
	{
		"Page_Up",		//None
		"Page_Down",	//ShiftMask
		"Up",			//          ControlMask
		"Down",			//ShiftMask|ControlMask
		"Up",			//                      Mod1Mask
		"Down",			//ShiftMask|            Mod1Mask
		"Up",			//          ControlMask|Mod1Mask
		"Down"			//ShiftMask|ControlMask|Mod1Mask
	}
};

/*----------------------------------------------------------------------------*/

#ifndef HAVE_STRDUP
char* strdup(char *a)
{
	char *b;
	malloc(strlen(a)+1);
	strcpy(b,a);
	return(b);
}
#endif

/*----------------------------------------------------------------------------*/

void setupstatebits()
{
	statebits[None                          ]=0;
	statebits[ShiftMask                     ]=1;
	statebits[          ControlMask         ]=2;
	statebits[ShiftMask|ControlMask         ]=3;
	statebits[                      Mod1Mask]=4;
	statebits[ShiftMask            |Mod1Mask]=5;
	statebits[          ControlMask|Mod1Mask]=6;
	statebits[ShiftMask|ControlMask|Mod1Mask]=7;
}

/*----------------------------------------------------------------------------*/

RETSIGTYPE exitParent(int num)
{
	exit(0);
}

/*----------------------------------------------------------------------------*/

int isUsedButton(int b)
{
	int i;

	Printf("isUsedButton(%d)=",b);
	for(i=0;i<NUM_BUTTONS;i++)
		if(buttons[i]==b)
			break;
	Printf("%s\n",i<NUM_BUTTONS?"yes":"no");
	return i<NUM_BUTTONS;
}

/*----------------------------------------------------------------------------*/

int buttonIndex(int b)
{
	int i;

	for(i=0;i<NUM_BUTTONS;i++)
		if(buttons[i]==b)
			return(i);
	return(NUM_BUTTONS);
}

/*----------------------------------------------------------------------------*/

void getOptions(int argc, char **argv, char *opts, const struct option *options)
{
	int ch,i,j,killold=False,invalidOpts=False;

	while((ch=getopt_long_only(argc,argv,opts,options,&i))>=0)
	{
		Printf("option : -%c",ch);
		if(optarg)
			Printf("\t=%s\n",optarg);
		if(debug)
			fflush(stdout);
		//Printf(" options[%d].name=%s",options[i].name);
		switch(ch)
		{
			/*case 0:
				Printf("\toption[%d]=\"%s\"",ch,i,options[i].name);
				if(optarg)
					Printf("\toptarg=\"%s\"",optarg);*/
			case 'a':
				autodelay=atoi(optarg);
				break;
			case 'c':
				doConfig=!doConfig;
				break;
			case '4':
				buttonFlip=!buttonFlip;
				break;
			case 'f':
				focusOverride=!focusOverride;
				break;
			case 'g':
				handleFocusGrab=!handleFocusGrab;
				break;
			case 'X':
				displayName=strdup(optarg);
				break;
			case 'W':
				useFifo=True;
				if(optarg)
					fifoName=strdup(optarg);
				break;
			case 'd':
				detach=!detach;
				break;
			case 'p':
				fprintf(stderr,"imwheel: the -p option is deprecated.\n");
				break;
			case 'D':
				debug=!debug;
				break;
			case 'q':
				quit=!quit;
				break;
			case 'K':
				keystyledefaults=!keystyledefaults;
				break;
			case 'k':
				killold=!killold;
				break;
			case 'R':
				restart=!restart;
				break;
			case 'r':
				root_wheeling=!root_wheeling;
				break;
			case 'x':
				transpose=!transpose;
				break;
			case 's':
				sensitivity=atoi(optarg);
				break;
			case 't':
				threshhold=atoi(optarg);
				break;
			case 'v':
				printUsage(argv[0],NULL,NULL);
				exit(0);
				break;
			case 'b':
				memset(buttons,0,NUM_BUTTONS*sizeof(int));
				for(j=0;optarg[j] && j<NUM_BUTTONS;j++)
				{
					if(optarg[j]<'0' || optarg[j]>'9')
					{
						fprintf(stderr,"imwheel: ERROR: buttons: #%d: %c is not a number!\n",j,optarg[j]);
						exit(1);
					}
					buttons[j]=optarg[j]-'0';
				}
				break;
			case 'h':
			case '?':
				printUsage(argv[0],options,optionusage);
				exit(0);
				break;
			default:
				fprintf(stderr,"imwheel: ERROR: Option %s is unknown!\n",argv[optind]);
		}
		Printf("\n");
	}
	if(invalidOpts)
		exit(1);
	if(!restart)
	{
		signal(SIGINT,exitParent);
		if(killold)
			KillIMWheel();
		else if(debug)
			fprintf(stderr,"WARNING: imwheel is not cleaning up other instances of itself, BE CAREFUL!\n  An imwheel may be running already.\n  Two or more imwheel processes on the same X display,\n  or simultaneously using a wheel fifo,\n  will not operate as expected!\n");
		if(quit)
			exit(0);
		if(detach)
		{
			sighandler_t sh;
			
			sh=signal(SIGUSR1,exitParent);
			if(fork())
			{
				wait(NULL);
				exit(0);
			}
			else
				signal(SIGUSR1,sh);
		}
		else
			fprintf(stderr,"INFO: imwheel is not running as a daemon.\n");
		setsid();
		if(detach)
			kill(getppid(),SIGUSR1);
		fprintf(stderr,"INFO: imwheel started (pid=%d)\n",getpid());
	}
	if(useFifo)
		openFifo();
}

/*----------------------------------------------------------------------------*/

void KillIMWheel()
{
	DIR *dir=opendir("/proc");
	struct dirent *ent;
	while((ent=readdir(dir)))
	{
		FILE *f;
		pid_t pid=0;
		char path[1024];
		if(!strchr("0123456789",ent->d_name[0]))
			continue;
		if(atoi(ent->d_name)==getpid())
			continue;
		snprintf(path,1024,"/proc/%s/stat",ent->d_name);
		if(!(f=fopen(path,"r")))
			continue;
		if(fscanf(f,"%d (%1023[^)]",&pid,path)==2)
		{
			//printf("%s==%d: %s\n",ent->d_name, pid, path);
			if(!strcmp("imwheel",path) && pid!=getpid())
			{
				if(kill(pid,SIGTERM)>=0)
					fprintf(stderr,"INFO: imwheel(pid=%d) killed.\n",pid);
				else
					fprintf(stderr,"WARNING: kill imwheel(pid=%d) failed: %s\n",pid,strerror(errno));
			}
		}	
		fclose(f);
	}
	closedir(dir);
}

/*----------------------------------------------------------------------------*/

void printKeymap(Display *d, char km[32])
{
	int i;
	
	Printf("Keymap status:\n");
	for(i=0;i<32*8;i++)
		if(getbit(km,i))
			Printf("\t[%d,%d] %d 0x%x \"%s\"\n",i/8,i%8,i,i,
				XKeysymToString(XKeycodeToKeysym(d,i,0)));
	Printf("\n");
}

/*----------------------------------------------------------------------------*/

void printXModifierKeymap(Display *d, XModifierKeymap *xmk)
{
	int i,j;

	Printf("XModifierKeymap:\n");
	for(j=0; j<8; j++)
		for(i=0; i<xmk->max_keypermod && xmk->modifiermap[(j*xmk->max_keypermod)+i]; i++)
			Printf("\t[%d,%02d] %d 0x%x \"%s\"\n",
				j,i,
				xmk->modifiermap[(j*xmk->max_keypermod)+i],
				xmk->modifiermap[(j*xmk->max_keypermod)+i],
				XKeysymToString(XKeycodeToKeysym(d,xmk->modifiermap[(j*xmk->max_keypermod)+i],0)));
}

/*----------------------------------------------------------------------------*/

void setbit(char *buf, int n, Bool val)
{
	Printf("setbit:n=%d,%d val=%d\n",n/8,n%8,val);
	if(val)
		buf[n/8]|=(1<<(n%8));
	else
		buf[n/8]&=0xFF^(1<<(n%8));
}
/*----------------------------------------------------------------------------*/

int getbit(char *buf, int n)
{
	/*if(debug && buf[n/8]&(1<<(n%8)))
		Printf("getbit:n=%d,%d\n",n/8,n%8);*/
	return(buf[n/8]&(1<<(n%8)));
}

/*----------------------------------------------------------------------------*/

void printXEvent(XEvent *e)
{
	Printf("type=%d\n",e->type);
	Printf("serial    =%lu\n",e->xany.serial);
	Printf("send_event=%d\n",e->xany.send_event);
	Printf("display   =%p\n",e->xany.display);
	Printf("window    =0x%08x\n",(unsigned)e->xany.window);
	if(e->type&(ButtonRelease|ButtonPress))
	{
		Printf("root         =0x%08x\n",(unsigned)e->xbutton.root);
		Printf("subwindow    =0x%08x\n",(unsigned)e->xbutton.subwindow);
		Printf("time         =0x%08x\n",(unsigned)e->xbutton.time);
		Printf("     x,y     =%d,%d\n",e->xbutton.x,e->xbutton.y);
		Printf("x_root,y_root=%d,%d\n",e->xbutton.x_root,e->xbutton.y_root);
		Printf("state        =0x%x\n",(unsigned)e->xbutton.state);
		Printf("button       =0x%x\n",(unsigned)e->xbutton.button);
		Printf("same_screen  =0x%x\n",(unsigned)e->xbutton.same_screen);
		Printf("\n");
	}
}

/*----------------------------------------------------------------------------*/
/* Returns true if there is a window name
 */
char *windowName(Display *d, Window w)
{
	{
		int ret_format;
		Atom ret_type;
		unsigned long ret_nitems, ret_bafter;
		
#ifdef DEBUG
		Printf("0x%08x ",(unsigned)w);
#endif
		if(ATOM_NET_WM_NAME != None && ATOM_UTF8_STRING != None)
		{
			unsigned char *uwname=0;
			int status = XGetWindowProperty(d, w, ATOM_NET_WM_NAME, 0, 2048, False,
				ATOM_UTF8_STRING, &ret_type, &ret_format, &ret_nitems, &ret_bafter,
				&uwname);
			if(status == Success && uwname)
			{
#ifdef DEBUG
				Printf("_NET_WM_NAME(ATOM_UTF8_STRING)=\"%s\"\n",uwname);
#endif
				return uwname;
			}
		}
		if(ATOM_WM_NAME != None && ATOM_STRING != None)
		{
			unsigned char *uwname=0;
			int status = XGetWindowProperty(d, w, ATOM_WM_NAME, 0, 2048, False,
				ATOM_STRING, &ret_type, &ret_format, &ret_nitems, &ret_bafter,
				&uwname);
			if(status == Success && uwname)
			{
#ifdef DEBUG
				Printf("WM_NAME(STRING)=\"%s\"\n",uwname);
#endif
				return uwname;
			}
		}
	}
	{
		char *wname=0;

		XFetchName(d,w,(char**)&wname);
		if(wname)
		{
#ifdef DEBUG
			Printf("wname=\"%s\"\n",wname);
#endif
			return(wname);
		}
	}
#ifdef DEBUG
	Printf("(null)\n");
#endif
	return(NULL);
}

/*----------------------------------------------------------------------------*/

void printUsage(char *pname, const struct option options[], const char *usage[][2])
{
	int i,maxa=0,maxb=0,len;
	char str[80];

	printf("imwheel %s by -=<Long Island Man>=- <jcatki@jonatkins.org>\n",VERSION);
	if(!options || !usage)
		return;
	printf("%s",pname);
	for(i=0;options[i].name;i++)
	{
		len=0;
		if(options[i].val)
			len+=2;
		if(options[i].val && options[i].name)
			len++;
		if(options[i].name)
			len+=2+strlen(options[i].name);
		if(maxa<len)
			maxa=len;
		len=0;
		if(usage[i][0])
			len=strlen(usage[i][0]);
		if(maxb<len)
			maxb=len;
	}
		
	for(i=0;options[i].name;i++)
	{
		printf(" [");
		if(options[i].name)
			printf("--%s",options[i].name);
		if(options[i].val && options[i].name)
			printf("|");
		if(options[i].val)
			printf("-%c",options[i].val);
		if(usage[i][0])
			printf(" %s",usage[i][0]);
		printf("]");
	}
	printf("\n");
	for(i=0;options[i].name;i++)
	{
		*str=0;
		if(options[i].name)
			sprintf(str,"--%s",options[i].name);
		if(options[i].val && options[i].name)
			sprintf(str,"%s|",str);
		if(options[i].val)
			sprintf(str,"%s-%c",str,options[i].val);
		printf("%-*.*s",maxa,maxa,str);
		if(usage[i][0])
			printf(" %-*.*s",maxb,maxb,usage[i][0]);
		else
			printf(" %-*.*s",maxb,maxb,"");
		if(usage[i][1])
		{
			len=strlen(usage[i][1]);
			if(maxa+maxb+len+2>80)
				printf("\n%80.80s",usage[i][1]);
			else
				printf(" %s",usage[i][1]);
		}
		printf("\n");
	}
}

/*----------------------------------------------------------------------------*/

void Printf(char *fmt, ...)
{
	va_list ap;

	if(debug)
	{
		va_start(ap,fmt);
#ifdef HAVE_VPRINTF
		vprintf(fmt,ap);
#else
#	ifdef HAVE_DOPRNT
		_doprnt(fmt,ap);
#	else
		printf("%s",fmt);
#	endif
#endif
		va_end(ap);
	}
}

/*----------------------------------------------------------------------------*/

void delay(unsigned long micros)
{
	/*struct timeval tv[2];*/
	
	usleep(micros);
	/*
#ifdef HAVE_GETTIMEOFDAY
	gettimeofday(&tv[0],NULL);
	do
		gettimeofday(&tv[1],NULL);
	while ((tv[1].tv_sec*1000000-tv[0].tv_sec*1000000)+(tv[1].tv_usec-tv[0].tv_usec)<micros);
#endif
	*/
}

/*----------------------------------------------------------------------------*/
//returns Modifier Index if found.  -1 if not.

int isMod(XModifierKeymap *xmk, int kc)
{
	int i,j;

	if(!xmk)
		return -1;
	for(i=0;i<8;i++)
		for(j=0;j<xmk->max_keypermod;j++)
			if(xmk->modifiermap[(i*xmk->max_keypermod)+j]==kc)
				return(i);
	return(-1);
}

/*----------------------------------------------------------------------------*/

unsigned int makeModMask(XModifierKeymap *xmk, char km[32])
{
	int i,b;
	unsigned int mask=0;
	
	for(i=0;i<32*8;i++)
		if(getbit(km,i) && (b=isMod(xmk,i))>=0)
		{
			Printf("makeModMask: or to mask %d\n",(1<<b));
			mask|=(1<<b);
		}
	Printf("makeModMask: 0x%x\n",mask);
	return(mask);
}

/*----------------------------------------------------------------------------*/

void printfState(int state)
{
	int i;

	Printf("State Mask:\n");
	for(i=0;i<MAX_MASKTRANS;i++)
		if(state&masktrans[i].val)
			Printf("\t%s",masktrans[i].name);
	Printf("\n");
}

/*----------------------------------------------------------------------------*/

struct WinAction *addRC(struct WinAction *wa, struct WinAction *wap)
{
	struct WinAction *newwa;

	newwa=realloc(wa, sizeof(struct WinAction)*(num_wa+1));
	memcpy(&newwa[num_wa],wap,sizeof(struct WinAction));
	num_wa++;
	return(newwa);
}

/*----------------------------------------------------------------------------*/

char *gethome(char *fname)
{
	char *home;

	home=getenv("HOME");
	if(home)
	{
		if( (strlen(home)+strlen(fname)) > 1023)
		{
			fprintf(stderr,"WARNING: Unexpected data in $HOME!\n"
					       "  Individual file \"$HOME%s\" will NOT be parsed!\n"
						   "  This would be a security hole otherwise!\n",fname);
			return(NULL);
		}
	}
	return(home);
}

/*----------------------------------------------------------------------------*/

int file_allowed(char *fname)
{
	uid_t uid,euid;
	gid_t gid,egid;
	struct stat stats;

	uid=getuid();
	euid=geteuid();
	Printf("uid=%hu euid=%hu\n",uid,euid);
	gid=getgid();
	egid=getegid();
	Printf("gid=%u egid=%u\n",gid,egid);
	if(stat(fname,&stats)<0)
	{
		if(debug)
			perror(fname);
		return(0);
	}
	Printf("%s: stats.st_uid=%hu stats.st_gid=%u stats.st_mode=%03x\n",fname,stats.st_uid,stats.st_gid,stats.st_mode&0xfff);
	return(    (uid==stats.st_uid && stats.st_mode&S_IRUSR)
			|| (gid==stats.st_gid && stats.st_mode&S_IRGRP)
			|| (uid!=stats.st_uid && gid!=stats.st_gid && stats.st_mode&S_IROTH)
		  );
}

/*----------------------------------------------------------------------------*/

struct WinAction *getRC()
{
	char fname[2][1024]={"","/etc/X11/imwheel/imwheelrc"}, line[1024], *p, *q, winid[1024];
	int fi,i;
	struct WinAction *newwa=NULL;
	FILE *f=NULL;
	char *home;
	int pri=0;

	home=gethome(RCFILE);
	if(home)
		sprintf(fname[0],"%s%s",home,RCFILE);
	num_wa=0;
	for(fi=(home?0:1);fi<2 && !f;fi++)
	{
		Printf("getRC:filename=\"%s\"\n",fname[fi]);
		if(strlen(fname[fi]) && file_allowed(fname[fi]))
		{
			f=fopen(fname[fi],"r");
			if(!f && debug)
				perror(fname[fi]);
		}
		else
		{
			Printf("%s: file permissions for your REAL user do not allow you to read it!\n",fname[fi]);
		}
	}
	if(!f)
		return(NULL);
	while(fgets(line, 1024, f))
	{
		Printf("getRC:pre:line:\n%s",line);
		q=strchr(line,'"');
		if(q)
		{
			p=strchr(line,'#');
			if(p && p<q)
				*p=0;
		}
		q=strrchr(line,'"');
		if(q)
		{
			p=strchr(q,'#');
			if(p)
				*p=0;
		}
		else
		{
			p=strchr(line,'#');
			if(p)
				*p=0;
		}
		while(strlen(line) && strchr(" \t\n",line[strlen(line)-1]))
			line[strlen(line)-1]=0;
		while(strlen(line) && strchr(" \t",line[0]))
			memmove(line,line+1,strlen(line+1)+1);
		if(!strlen(line))
			continue;
		if(line[0]=='"')
		{	//new window
			Printf("getRC:win:line:\"%s\"\n",line);

			if(!(p=strchr(line+1,'"')))
				exitString("Missing closing quote in config:\n%s\n",line);
			*p=0;
			strcpy(winid,line+1);
			Printf("id=\"%s\"\n",winid);
			pri=0;
		}
		else
		{	//strip whitespace
			for(i=0;i<strlen(line);i++)
				while(strlen(line) && strchr(" \t",line[i]))
					memmove(line+i,line+i+1,strlen(line+i+1)+1);
			if(!strlen(line))
				continue;
			Printf("getRC:mod:line:\"%s\"\n",line);
			//new mod pod
			newwa=(struct WinAction*)realloc(newwa,sizeof(struct WinAction)*(num_wa+2));
			newwa[num_wa].id=strdup(winid);
			newwa[num_wa+1].id=NULL;
			newwa[num_wa].reps=1;
			newwa[num_wa].delay=0;
			newwa[num_wa].delayup=0;
			newwa[num_wa].pri=pri;
			newwa[num_wa].button=0;
			num_wa++;
			//Get Command (if any)
			if(line[0]=='@')
			{
				if(!strcasecmp(line+1,"Exclude"))
					newwa[num_wa-1].button=UNGRAB;
				else if(!strcasecmp(line+1,"Repeat"))
					newwa[num_wa-1].button=REPEAT;
				else if(!strncasecmp(line+1,"Priority=",9))
				{
					pri=strtol(line+10,NULL,10);
					Printf("Priority=%d\n",pri);
					newwa[num_wa-1].button=PRIORITY;
					newwa[num_wa-1].pri=pri;
				}
				else
					exitString("Unrecognized command : \"%s\"\n",line+1);
				continue;
			}
			Printf("Priority: %d\n",pri);
			//Get Keysym mask
			p=strchr(line,',');
			if(p)
				*p=0;
			else
				exitString("expected 3 args, got 1, in config.\n%s\n",line);
			Printf("Keysym mask: \"%s\"\n",line);
			newwa[num_wa-1].in=getPipeArray(line);
			memmove(line,p+1,strlen(p+1)+1);
			//Get Button
			p=strchr(line,',');
			if(p)
				*p=0;
			else
				exitString("Expected 3 args, got 2, in config.\n%s\n",line);
			Printf("Button: \"%s\"\n",line);
			if(!strncasecmp(line, "Button", 6))
			{
				sscanf(line+6,"%d",&i);
				Printf("(Button%d)",i);
				if(i>NUM_BUTTONS)
					i=NUM_BUTTONS;
				else
					newwa[num_wa-1].button=i;
			}
			else
			{
				for(i=0; i<NUM_BUTTONS; i++)
				{
					if(!strcasecmp(line,button_names[i]))
					{
						if(buttons[i])
							newwa[num_wa-1].button=i+4;//buttons[i];
						else
							newwa[num_wa-1].button=0;
						break;
					}
				}
			}
			if(i==NUM_BUTTONS) // nothing found
			{
				if(!strncasecmp(line,button_names[4],strlen(button_names[4])-1))
					newwa[num_wa-1].button=buttons[4];
				else
					exitString("Unrecognized wheel action in config.\n%s\n",line);
			}
			Printf("\t=%d\n",newwa[num_wa-1].button);
			if(!newwa[num_wa-1].button)
			{
				Printf("Discarding WA, no button in use...\n");
				num_wa--;
				if(newwa[num_wa].id)
					free(newwa[num_wa].id);
				newwa[num_wa].id=NULL;
				continue;
			}
			memmove(line,p+1,strlen(p+1)+1);
			//Get Keysym Out
			p=strchr(line,',');
			if(p)
				*p=0;
			if(*line && strlen(line))
			{
				Printf("Keysyms Out: \"%s\"\n",line);
				newwa[num_wa-1].out=getPipeArray(line);
			}
			else
				exitString("Unrecognized or missing Keysym Outs (arg 3) in config.\n%s\n",line);
			if(p)
				memmove(line,p+1,strlen(p+1)+1);
			else
				continue;
			//Get Reps
			p=strchr(line,',');
			if(p)
				*p=0;
			if(*line && strlen(line))
			{
				Printf("Reps: \"%s\"\n",line);
				newwa[num_wa-1].reps=strtol(line,NULL,10);
			}
			if(p)
				memmove(line,p+1,strlen(p+1)+1);
			else
				continue;
			//Get Delay
			p=strchr(line,',');
			if(p)
				*p=0;
			if(*line && strlen(line))
			{
				Printf("Delay: \"%s\"\n",line);
				newwa[num_wa-1].delay=strtol(line,NULL,10);
			}
			if(p)
				memmove(line,p+1,strlen(p+1)+1);
			else
				continue;
			//Get Delay Up
			if(*line && strlen(line))
			{
				Printf("Delay Up: \"%s\"\n",line);
				newwa[num_wa-1].delayup=strtol(line,NULL,10);
			}
		}
	}
	fclose(f);
	//sorting removes order
	//qsort(newwa,num_wa,sizeof(struct WinAction),wacmp);
	for(i=0;newwa && newwa[i].id;i++)
		printWA(&newwa[i]);
	//writeRC(newwa); //this was for debugging rc file writing
	return(newwa);
}

/*----------------------------------------------------------------------------*/

int wacmp(const void *a, const void *b)
{
	return(strcmp(((struct WinAction*)b)->id,((struct WinAction*)a)->id));
}

/*----------------------------------------------------------------------------*/

char *strsepc(char **str, char delim)
{	// this is to replace the non-portable strsep...
	char *p,*r;

	if(!*str)
		return(NULL);
	for(p=*str; *p && *p!=delim; p++);
	if(!*p)
	{
		r=*str;
		*str=NULL;
	}
	else
	{
		*p='\0';
		r=*str;
		*str=p+1;
	}
	return(r);
}

/*----------------------------------------------------------------------------*/

char **getPipeArray(char *str)
{
	int i,n;
	char *s,**a,*t;

	for(n=0,s=str;*s;s++)
		if(*s=='|')
			n++;
	if(strlen(str))
		n++;
	a=(char**)malloc(sizeof(char*)*(n+1));
	a[n]=NULL;
	for(i=0,s=str; s && i<n; i++)
	{
		t=strsepc(&s, '|');
		a[i]=strdup(t);
		Printf("%d) \"%s\" \"%s\"\n",i,a[i],(s?s:"(null)"));
	}
	return(a);
}

/*----------------------------------------------------------------------------*/

void exitString(char *format, char *arg)
{
	fprintf(stderr,format,arg);
	exit(1);
}

/*----------------------------------------------------------------------------*/

void printWA(struct WinAction *wa)
{
	int i;

	if(!wa)
		return;
	Printf("WinAction (%p):\n",wa);
	Printf("\tPriority         : %d\n",wa->pri);
	Printf("\tWindow Regex     : \"%s\"\n",wa->id);
	if(wa->button<MIN_COMMAND)
	{
		Printf("\tKeysyms Mask (%p):\n",wa->in);
		for(i=0; wa->in && wa->in[i]; i++)
			Printf("\t\t\"%s\"\n",wa->in[i]);
		Printf("\tButton           : %d\n",wa->button);
		Printf("\tKeysyms Out (%p) :\n",wa->out);
		for(i=0; wa->out && wa->out[i]; i++)
			Printf("\t\t\"%s\"\n",wa->out[i]);
		Printf("\tReps: %d\n",wa->reps);
		Printf("\tRep Delay: %d\n",wa->delay);
		Printf("\tKey Up Delay: %d\n",wa->delayup);
	}
	else
		switch(wa->button)
		{
			case UNGRAB:
				Printf("Command: Exclude  (UNGRAB)\n");
				break;
			case REPEAT:
				Printf("Command: Repeat   (REPEAT)\n");
				break;
			case PRIORITY:
				Printf("Command: Priority (PRIORITY)\n");
				break;
		}
}

/*----------------------------------------------------------------------------*/

void writeRC(struct WinAction *wa)
{
	int i;
	struct WinAction *p,*cur;
	FILE *f=stdout;
	char fname[1024];
	char *home;
	
	if(!wa)
		return;
	home=gethome(RCFILE);
	if(home)
		sprintf(fname,"%s%s",home,RCFILE);
	else
	{
		perror("imwheel,writeRC");
		return;
	}
	if(!(f=fopen(fname,"w")))
	{
		perror("imwheel,writeRC");
		return;
	}
	fprintf(f,"# IMWheel Configuration file (%s)\n# (C)Jon Atkins <jcatki@jonatkins.org>\n#\n# Generated by imwheel\n# Any extra comments will be lost on reconfiguration\n# However order will be maintained\n# Order!  ORDER, I SAY!!\n",fname);
	for(cur=NULL,p=wa;p->id;p=&p[1])
	{
		if(!cur || strcmp(cur->id,p->id))
		{
			fprintf(f,"\n\"%s\"\n",p->id);
			cur=p;
		}
		if(p->button<0x10)
		{
			for(i=0; p->in[i]; i++)
				fprintf(f,"%s%s",(i?"|":""),p->in[i]);
			fprintf(f,",\t%s,\t",button_names[p->button-4]);
			for(i=0; p->out[i]; i++)
				fprintf(f,"%s%s",(i?"|":""),p->out[i]);
			if(p->delayup>0||p->delay>0||p->reps>1||p->reps==0)
				fprintf(f,",\t%d",p->reps);
			if(p->delayup>0||p->delay>0)
				fprintf(f,",\t%d",p->delay);
			if(p->delayup>0)
				fprintf(f,",\t%d",p->delayup);
			fprintf(f,"\n");
		}
		else
		{
			switch(p->button)
			{
				case UNGRAB:
					fprintf(f,"@Exclude\n");
					break;
				case REPEAT:
					fprintf(f,"@Repeat\n");
					break;
				case PRIORITY:
					fprintf(f,"@Priority=%d\n",p->pri);
					break;
			}
		}
	}
	fclose(f);
}

/*----------------------------------------------------------------------------*/
char *regerrorstr(int err, regex_t *preg)
{
	static char *str=NULL;
	int len;

	len=regerror(err,preg,NULL,0);
	str=realloc(str,len);
	regerror(err,preg,str,len);
	return(str);
}

/*----------------------------------------------------------------------------*/

int regex(char *expression, char *str)
{
	regex_t preg;
	//regmatch_t pmatch;
	int err;

	if(!str || !expression)
		return(0);
	Printf("Testing: \"%s\" ?= \"%s\"\n",expression,str);
	if((err=regcomp(&preg,expression,REG_EXTENDED|REG_NOSUB)))
	{
		fprintf(stderr,"imwheel: RegComp ERROR In Expression:\n\"%s\"\n%s\nRemoving the offending expression...\n",expression,regerrorstr(err,&preg));
		expression[0]=0;
		regfree(&preg);
		return(1);
	}
	if((err=regexec(&preg,str,0,NULL,0)) && err!=REG_NOMATCH)
	{
		fprintf(stderr,"imwheel: RegExec ERROR In Expression:\n\"%s\"\n%s\nnRemoving the offending expression...\n",expression,regerrorstr(err,&preg));
		expression[0]=0;
		regfree(&preg);
		return(2);
	}
	regfree(&preg);
	if(err!=REG_NOMATCH)
		Printf("RegEx Match found:\"%s\" = \"%s\"\n",expression,str);
	return(err!=REG_NOMATCH);
}

/*----------------------------------------------------------------------------*/
struct WinAction *findWA(Display		 *d,
						 int			  button   ,
						 char			 *winname  ,
						 char			 *resname  ,
						 char			 *classname,
						 XModifierKeymap *xmk      ,
						 char			  KM[32]   )
{
	int i,j,ok,kc;
	struct WinAction *wap=NULL;
	char km[32];

	if(!wa)
		return(NULL);
	Printf("findWA:button=%d\n",button);
	for(i=0; wa[i].id; i++)
	{
		// skip removed or blank id's
		if(wa[i].id[0]==0)
			continue;
		// throw out PRIORITY placeholder commands
		if(wa[i].button == PRIORITY)
			continue;
		// throw out miss matched buttons (keep going for all commands)
		if(button != wa[i].button &&
				wa[i].button < MIN_COMMAND &&
				button != GRAB)
		{
			if(button<MIN_COMMAND)
				Printf("input button(%d) doesn't match rule-button(%d)\n",button,wa[i].button);
			continue;
		}
		// match window attribs
		if(!(regex(wa[i].id,winname  ) ||
			 regex(wa[i].id,resname  ) ||
			 regex(wa[i].id,classname) ||
			 ((!winname || !resname || !classname) &&
			  !strcmp(wa[i].id,"(null)"))))
		{
			Printf("Window ID doesn't match\n");
			continue;
		}
		if(!KM || !xmk || wa[i].button>=MIN_COMMAND)
		{
			if(!wap || wap->pri<wa[i].pri)
				wap=&wa[i];
			if(button<MIN_COMMAND)
				Printf("Not testing modifiers on a command\n");
			continue;
		}
		ok=1;
		memcpy(km,KM,32);
		if(wa[i].in[0] && wa[i].in[0][0])
		{
			if(strcasecmp(wa[i].in[0],"None"))
				for(j=0; ok && wa[i].in[j]; j++)
				{
					if(!wa[i].in[j][0])
						continue;
					kc=XKeysymToKeycode(d, XStringToKeysym(wa[i].in[j]));
					ok=getbit(km,kc);
					setbit(km,kc,0);
				}
			for(kc=0;ok && kc<32*8;kc++)
				if(isMod(xmk,kc)>-1)
					ok=!getbit(km,kc);
		}
		if(!ok)
		{
			Printf("Modifiers for input and actual keys pressed do not match\n");
			continue;
		}
		if(!wap || wap->pri<wa[i].pri)
		{
			if(wap)
				Printf("Match priority(%d) overrides previous match priority(%d)\n",wa[i].pri,wap->pri);
			wap=&wa[i];
		}
		else
			Printf("Match discarded, Priority(%d) is equal or less than %d\n",wa[i].pri,wap->pri);
	}
	if(!wap)
		Printf("No Match for : win=\"%s\" res=\"%s\" class=\"%s\"\n",winname,resname,classname);
	else
	{
		Printf("Matched with : id=\"%s\" button=%d pri=%d\n",wap->id,wap->button,wap->pri);
		// throw out UNGRABs when looking for things that need GRAB on
		if(wap->button == UNGRAB && button == GRAB)
		{
			Printf("Exclude may have a higher priority due to file position or the priority command\n");
			wap=NULL;
		}
	}
	return(wap);
}
/*----------------------------------------------------------------------------*/
int inputWaiting(Display *d, XEvent *e)
{
	if(useFifo)
	{
		if(fifofd!=-1)
		{
			fd_set set;
			struct timeval tv={0,0};
			
			FD_ZERO(&set);
			FD_SET(fifofd, &set);
			if(select(fifofd+1,&set,0,&set,&tv))
				return True;
		}
		else
			openFifo();
	}
	else if(XCheckMaskEvent(d,ButtonReleaseMask|ButtonPressMask,e))
	{
		return True;
	}
	return False;
}
/*----------------------------------------------------------------------------*/
void doWA(Display          *d     ,
		  XButtonEvent     *e     ,
		  XModifierKeymap  *xmk   ,
		  char              km[32],
		  struct WinAction *wap   )
{
	int i,rep;
	unsigned int button;
	KeyCode kc;
	static unsigned char nkm[8192];
	static unsigned char nbm[16]; //256 buttons. oh the insanity! ;)
	/*
	   struct timeval tv;
	   unsigned int mask;
	 */

	Printf("doWA:\n");
	printWA(wap);

	if(wap->button==REPEAT)
	{
		Printf("doWA: button down = %d\n",e->button);
		XTestFakeButtonEvent(d,e->button,True,CurrentTime);
		XFlush(d);
		/*Printf("doWA: button delay...\n");
		delay(1000); */
		Printf("doWA: button up = %d\n",e->button);
		XTestFakeButtonEvent(d,e->button,False,CurrentTime);
		XFlush(d);
		Printf("doWA: button done\n");
		return;
	}
	if(wap->button>=UNGRAB)
	{
		Printf("doWA: UNGRAB cannot be handled here: use @Repeat or resend the Button# in the imwheelrc, i.e. None,Up,Button4\n");
		return;
	}
	
	for(rep=0; rep<wap->reps || !wap->reps; rep++)
	{
		if(wap->reps)
			Printf("rep=%d\n",rep);
		//XSetInputFocus(d,e->subwindow,RevertToParent,CurrentTime);
		modMods(d,km,xmk,False,0);
		//XSync(d,False); //this seems to cause apps to receive crud at random times!
		memset(nkm,0,8192);
		memset(nbm,0,16);
		for(i=0; wap->out[i]; i++)
		{
			if(strncmp("Button",wap->out[i],6) && strncmp("-Button",wap->out[i],7))
			{
				if(wap->out[i][0]!='-')
				{
					kc=XKeysymToKeycode(d, XStringToKeysym(wap->out[i]));
					XTestFakeKeyEvent(d,kc,True,CurrentTime);
					setbit(nkm,kc,True);
				}
				else
				{
					kc=XKeysymToKeycode(d, XStringToKeysym(wap->out[i]+1));
					XTestFakeKeyEvent(d,kc,False,CurrentTime);
					setbit(nkm,kc,False);
				}
			}
			else
			{
				ungrabButtons(d,0);
				if(wap->out[i][6]!='-')
				{
					button=strtoul(wap->out[i]+6,NULL,10);
					XTestFakeButtonEvent(d,button,True,CurrentTime);
					if(button<256)
						setbit(nbm,button,True);
				}
				else
				{
					button=strtoul(wap->out[i]+7,NULL,10);
					XTestFakeButtonEvent(d,button,False,CurrentTime);
					if(button<256)
						setbit(nbm,button,False);
				}
				XSync(d,False);
				grabButtons(d,0);
			}
		}
		Printf("doWA: keyup delay=%d\n",wap->delayup);
		delay(wap->delayup);
		for(i--; i>=0; i--)
		{
			if(strncmp("Button",wap->out[i],6))
			{
				if(wap->out[i][0]!='-')
				{
					kc=XKeysymToKeycode(d, XStringToKeysym(wap->out[i]));
					if(getbit(nkm,kc))
					{
						XTestFakeKeyEvent(d,kc,False,CurrentTime);
						setbit(nkm,kc,False);
					}
				}
			}
			else
			{
				ungrabButtons(d,0);
				if(wap->out[i][0]!='-')
				{
					button=strtoul(wap->out[i]+6,NULL,10);
					if(button>=256 || getbit(nbm,button))
					{
						XTestFakeButtonEvent(d,button,False,CurrentTime);
						if(button<256)
							setbit(nbm,button,False);
					}
				}
				XSync(d,False);
				grabButtons(d,0);
			}
		}
		XSync(d,False);
		modMods(d,km,xmk,True,0);
		XSync(d,False);
		if(wap->delay)
		{
			Printf("doWA: nextkey delay=%d\n",wap->delay);
			delay(wap->delay);
		}
		if(!wap->reps) // auto-repeat
		{
			if(autodelay)
				delay(autodelay);
			if(inputWaiting(d,(XEvent*)e))
			{
				if(useFifo)
				{
					getInput(d, (XEvent*)e, &xmk, km);
					if(e->type!=ButtonPress || e->button!=wap->button)
						return;
				}
				else if(e->type==ButtonRelease && 
						buttons[buttonIndex(e->button)]==wap->button)
				{
					XPutBackEvent(d,(XEvent*)e);
					e->button=buttons[buttonIndex(e->button)];
					return;
				}
			}
		}
	}
}
/*----------------------------------------------------------------------------*/

void modMods(Display *d, char *km, XModifierKeymap *xmk, Bool on, int delay_time)
{
	int i;

	for(i=0;i<32*8;i++)
		if(getbit(km,i) && isMod(xmk,i)>-1)
		{
			Printf("XTestFakeKeyEvent 0x%x=%s delay=%d\n",i,(on?"On":"Off"),delay_time);
			XTestFakeKeyEvent(d,i,on,delay_time);
		}
}

/*----------------------------------------------------------------------------*/

void flushFifo()
{
	char junk;
	
	closeFifo();
	if((fifofd=open(fifoName, O_RDONLY|O_NONBLOCK))<0)
	{
		perror(fifoName);
		exit(1);
	}
	errno=0;
	while(read(fifofd,&junk,1) && !errno)
		Printf("Flushed : 0x%02x\n",junk);
	close(fifofd);
}

/*----------------------------------------------------------------------------*/

void closeFifo()
{
	if(fifofd<0)
		return;
	close(fifofd);
	fifofd=-1;
}

/*----------------------------------------------------------------------------*/

void openFifo() //also will close,flush, and reopen if fifo is already open!
{
	// Clear stick (reset state)
	stick.x=stick.y=0;
	// preflush, or else!
	flushFifo();
	// open for real now!
	if((fifofd=open(fifoName, O_RDONLY))<0)
	{
		perror(fifoName);
		exit(1);
	}
	Printf("Reopened Wheel FIFO.\n");
}

/*----------------------------------------------------------------------------*/
/* vim:ts=4:shiftwidth=4
"*/
