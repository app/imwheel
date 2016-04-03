/* Intellimouse Wheel Thinger
 * Copylefted under the GNU Public License
 * Author : Jonathan Atkins <jcatki@jonatkins.org>
 * PLEASE: contact me if you have any improvements, I will gladly code good ones
 */
#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <time.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <termios.h>
#include "util.h"
#include "cfg.h"
#include "imwheel.h"
#include <X11/Xmu/WinUtil.h>

#define HISTORY_LENGTH 3
#define CONFIG_TIME 4

/*----------------------------------------------------------------------------*/

Display *start(char*);
void endItAll(Display*);
void eventLoop(Display*, char**);
void freeAllX(XClassHint*, XModifierKeymap**, char**);
int *nullXError(Display*,XErrorEvent*);
Stick stick,old_stick;
char *emptystr="(no window)";

/*----------------------------------------------------------------------------*/

char *opts="a:b:c4fgiW:dpDKkRrqh?X:t:vxs:";
const struct option options[]=
{ /*{name,			need/opt_arg,	flag,	val}*/
	{"auto-repeat",		1,			0,		'a'},
	{"buttons",			1,			0,		'b'},
	{"config",			0,			0,		'c'},
	{"debug",			0,			0,		'D'},
	{"detach",			0,			0,		'd'},
	{"display",			1,			0,		'X'},
	{"flip-buttons",	0,			0,		'4'},
	{"focus",			0,			0,		'f'},
	{"focus-events",	0,			0,		'g'},
	{"help",			0,			0,		'h'},
	{"key-defaults",	0,			0,		'K'},
	{"kill",			0,			0,		'k'},
	{"pid",				0,			0,		'p'},
	//{"restart",		0,			0,		'R'}, //not used by users!
	{"root-window",		0,			0,		'r'},
	{"quit",			0,			0,		'q'},
	{"sensitivity",		1,			0,		's'},
	{"threshhold",		1,			0,		't'},
	{"version",			0,			0,		'v'},
	{"wheel-fifo",		2,			0,		'W'},
	{"transpose",		0,			0,		'x'},
	{0,					0,			0,		0}
};
const char *optionusage[][2]=
{ /*{argument name,		usage}*/
	{"delay-rate",		"auto repeat until button release (default=250)"},			//a
	{"grab-buttons",	"Specify up to 6 remappings 0=none (default=456789)"},		//b
	{NULL,				"Open configuration helper window imediately"},				//c
	{NULL,				"Spit out all debugging info (it's a lot!)"},				//D
	{NULL,				"IMWHeel process doesn't detach from terminal"},			//d
	{"display-name",	"Sets X display to use (one per FIFO if FIFO used)"},		//X
	{NULL,				"Swaps buttons 4 and 5 events (same as -b 54"},				//4
	{NULL,				"Use event subwindow instead of XGetInputFocus"},			//f
	{NULL,				"Disable the use of Focus Events for button grabs"},		//g
	{NULL,				"For this help!  Now you know"},							//h
	{NULL,				"Use the old key style default actions"},					//K
	{NULL,				"Kills the running imwheel process"},						//k
	{NULL,				"IMWheel doesn't use or check any pid files"},				//p
	//{NULL,				"RESERVED: used when imwheel reloads itself"},			//R
	{NULL,				"Allow wheeling in the root window (no cfg dialog)"},		//r
	{NULL,				"Don't start imwheel process, after args"},					//q
	{"sum-min",			"Stick devices require this much total movment (w/fifo)"},	//s
	{"stick-min",		"Stick devices require this much pressure (w/fifo)"},		//t
	{NULL,				"Show version info and exit"},								//v
	{"fifo-path",		"Use a GPM fifo instead of XGrabButton"},					//W
	{NULL,				"swap X and Y stick axis (w/fifo)"},						//x
	{NULL,				NULL}
};
int buttonFlip=False, useFifo=False, detach=True, quit=False,
    restart=False, threshhold=0, focusOverride=True, sensitivity=0, transpose=False,
	handleFocusGrab=True, doConfig=False, root_wheeling=False, autodelay=250,
	keystyledefaults=0;
int fifofd=-1;
char *fifoName="/dev/gpmwheel", *displayName=NULL;
Bool grabbed;
Atom ATOM_NET_WM_NAME, ATOM_UTF8_STRING, ATOM_WM_NAME, ATOM_STRING;

/*----------------------------------------------------------------------------*/

int main(int argc, char **argv)
{
	Display *d;

	getOptions(argc,argv,opts,options);
	setupstatebits();
	if(!displayName)
		displayName=XDisplayName(NULL);
	Printf("display=%s\n",displayName);
	d=start(displayName);
	Printf("starting loop...\n");
	eventLoop(d,argv);
	Printf("ending...\n");
	endItAll(d);
	return(0); //not reached...
}

/*----------------------------------------------------------------------------*/

void endItAll(Display *d)
{
	XUngrabButton(d, AnyButton, AnyModifier, DefaultRootWindow(d));
	XCloseDisplay(d);
}

/*----------------------------------------------------------------------------*/

void grabButtons(Display *d, Window w)
{
	int i;

	if(useFifo)
		return;
	Printf("Grab buttons!\n");
	grabbed=True;
	for(i=0;i<NUM_BUTTONS;i++)
	{
		if(buttons[i])
		{
			Printf("Grabbing Button %d...\n",buttons[i]);
			XGrabButton(
				d,
				buttons[i],
				AnyModifier,
				(w?w:DefaultRootWindow(d)),
				False,
				ButtonReleaseMask|ButtonPressMask,
				GrabModeAsync, GrabModeAsync,
				None,
				None
			);
		}
	}
}

/*----------------------------------------------------------------------------*/

void ungrabButtons(Display *d, Window w)
{
	int i;

	if(useFifo)
		return;
	Printf("Ungrab buttons!\n");
	XSync(d,False);
	grabbed=False;
	for(i=0;i<NUM_BUTTONS;i++)
	{
		if(buttons[i])
		{
			Printf("Ungrabbing Button %d...\n",buttons[i]);
			XUngrabButton(
				d,
				buttons[i],
				AnyModifier,
				(w?w:DefaultRootWindow(d))
			);
		}
	}
	XUngrabButton(d, AnyButton, AnyModifier, (w?w:DefaultRootWindow(d)));
	//XAllowEvents(d,AsyncBoth,CurrentTime);
	XSync(d,False);
}

/*----------------------------------------------------------------------------*/

Display *start(char *display)
{
	Display *d;

	wa=getRC();
	d=XOpenDisplay(display);
	if(!d)
	{
		fprintf(stderr,"Could not open display, check shell DISPLAY variable, and export or setenv it!\n");
		exit(1);
	}
	ATOM_NET_WM_NAME = XInternAtom(d, "_NET_WM_NAME", True);
	ATOM_UTF8_STRING = XInternAtom(d, "UTF8_STRING", True);
	ATOM_WM_NAME = XInternAtom(d, "WM_NAME", True);
	ATOM_STRING = XInternAtom(d, "STRING", True);
	XAllowEvents(d,AsyncBoth,CurrentTime);
	//XAllowEvents(d,SyncBoth,CurrentTime);
	if(!useFifo)
	{
		grabButtons(d,0);
		XSelectInput(d,DefaultRootWindow(d),PointerMotionMask);
	}
	return(d);
}

/*----------------------------------------------------------------------------*/
signed char getInput(Display *d, XEvent *e, XModifierKeymap **xmk, signed char km[32])
{
	static int st_sum_x=0, st_sum_y=0;
	signed char button;
	
	button=0;
	e->type=0;
	if(!useFifo)
	{
		//Printf("getInput: XNextEvent...\n");
		XNextEvent(d,e);
		//Printf("getInput: Got XNextEvent.\n");
	}
	else
	{
		Printf("getInput: read fifo...\n");
		memset(e,0,sizeof(XEvent));
		e->xany.display=d;
		if(!(read(fifofd,&button,1)))
		{
			close(fifofd);
			Printf("getInput: Must reopen the GPM fifo...\n");
			openFifo();
			return(0);
		}
		if(button<0x10) // single button
		{
			Printf("getInput: single button=%d\n",button);
			e->xbutton.button=button;
			e->type=ButtonPress;
		}
		else if(button==0x10) // stick x y are next...
		{
			Printf("getInput: stick\n");
			// must do more for stick...this at least emu's up and down...
			Printf("getInput: Stick action, reading x and y values from fifo...\n");
			memcpy(&old_stick, &stick, 2);
			// read in separate bytes, to avoid an incomplete read!
			if(transpose)
			{
				read(fifofd,&stick.y,1);
				read(fifofd,&stick.x,1);
			}
			else
			{
				read(fifofd,&stick.x,1);
				read(fifofd,&stick.y,1);
			}
			Printf("getInput: Stick=(%d,%d)\n",stick.x,stick.y);
			if(sensitivity>0)
			{ // do thresholded summation then test with sensitivity value
				if(ABS(stick.y)>=threshhold||ABS(stick.x)>=threshhold)
				{
					if((stick.x>0 && st_sum_x<0) || (stick.x<0 && st_sum_x>0))
						st_sum_x = 0;
					if((stick.y>0 && st_sum_y<0) || (stick.y<0 && st_sum_y>0))
						st_sum_y = 0;
					st_sum_x += stick.x;
					st_sum_y += stick.y;
					if(ABS(st_sum_x) > sensitivity)
					{
						e->xbutton.button=(stick.x>0?6:7);
						e->type=ButtonPress;
						st_sum_x = 0;
					}
					else if(ABS(st_sum_y) > sensitivity)
					{
						e->xbutton.button=(stick.y>0?4:5);
						e->type=ButtonPress;
						st_sum_y = 0;
					}
				}
			}
			else // do just threshold testing
			{
/* commented sections here would add time as a factor in the testing
 * preventing events from being generated too fast for apps to react
 * to in a feasible manner.  This should also be based on the two
 * delays given in the imwheelrc file for the current application,
 * but that code cannot be here, and must be added to the code after
 * the application has been found in the winaction list using findWA.
				static struct timeval ntv={0,0},otv={0,0};
				static unsigned long dt=0;

#ifdef HAVE_GETTIMEOFDAY
				gettimeofday(&ntv,NULL);
#endif
				if(!otv.tv_sec)
					memcpy(&otv,&ntv,sizeof(struct timeval));
				dt=((ntv.tv_sec-otv.tv_sec)*1000000)+(ntv.tv_usec-otv.tv_usec);
*/
				if(((ABS(old_stick.y)<threshhold&&ABS(old_stick.x)<threshhold &&
					 (ABS(stick.y)>=threshhold || ABS(stick.x)>=threshhold) &&
					(stick.x+stick.y!=0)) ||
				   !threshhold)
						/*|| dt<0 || dt>500000*/)
				{
					//memcpy(&otv,&ntv,sizeof(struct timeval));
					if(ABS(stick.x) <= ABS(stick.y)) /* tend to be up/down... */
					{
						e->xbutton.button=(stick.y>0?4:5);
						e->type=ButtonPress;
					}
					else
					{
						e->xbutton.button=(stick.x<0?6:7);
						e->type=ButtonPress;
					}
				}
			}
		}
	}
	if(e->type==ButtonPress
	   && (isUsedButton(e->xbutton.button) || (!useFifo && grabbed)))
	{
		//e->xbutton.button^=buttonFlip;
		button= buttonIndex(e->xbutton.button);
		if(button<NUM_BUTTONS)
			e->xbutton.button= button= button+4;
		XQueryKeymap(d,km);
		if(debug)
			printKeymap(d,km);
		*xmk=XGetModifierMapping(d);
		if(debug)
			printXModifierKeymap(d,*xmk);
	}
	else
	{
		e->xbutton.button=button=0;
		e->type=None;
	}
	if(handleFocusGrab && e->type==FocusOut && (!useFifo && !grabbed))
	{
		// the focus is leaving the @Exclude´d window,
		// so we want to grab the buttons again
		grabButtons(d,0);
		// we don't need any further events from that window for now
		// we asked for events during ungrab...below
		XSelectInput(d, e->xany.window, NoEventMask);
	}
	if(e->type==ButtonPress)
		Printf("getInput: Button=%d\n",button);
	return(button);
}
/*----------------------------------------------------------------------------*/

void openCfg(Display *d, char **argv, XModifierKeymap **xmk)
{
	int i;

	Printf("Going to configuration...\n");
	if(!useFifo && grabbed)
	{
		ungrabButtons(d,0);
		ungrabButtons(d,0);
	}
	if(cfg(useFifo?fifofd:-1))
	{
		char **nargv=NULL;

		Printf("Configuration changed...\n");
		Printf("Restarting %s...\n",argv[0]);
		freeAllX(&xch,xmk,&wname);
		for(i=0;argv[i];i++)
		{
			nargv=realloc(nargv,sizeof(signed char*)*(i+3));
			if(argv[i][0]=='-' && argv[i][1]!='-')
			{
				char *p;
				while((p=strchr(argv[i],'c')))
					memmove(p,p+1,strlen(p));
			}
			if(!strcmp(argv[i],"--config"))
			{
				int j;
				for(j=i;argv[j];j++)
					argv[j]=argv[j+1];
			}
			nargv[i]=argv[i];
		}
		if(!restart)
			nargv[i++]=strdup("-R");
		nargv[i]=NULL;
		execvp(nargv[0],nargv); // run me over me!
		fprintf(stderr,"%s: Restart failed!\n",nargv[0]);
		perror(argv[0]);
		exit(1);
	}
	if(!useFifo && !grabbed)
		grabButtons(d,0);
	if(useFifo)
		openFifo();
	Printf("No change in configuration\n");
}

/*----------------------------------------------------------------------------*/

void eventLoop(Display *d, char **argv)
{
	XEvent e;
	Window pointer_window=0, pointer_rwindow=0;
	int j;
	XModifierKeymap *xmk=NULL;
	signed char km[32],button;
	struct WinAction *wap=NULL, *ungrabwap=NULL, *grabwap=NULL;
	Window oldw=0;
	int isdiffwin;
	struct {time_t t; int motion;} history[HISTORY_LENGTH];

	XSetErrorHandler((XErrorHandler)nullXError);
	if(doConfig)
		openCfg(d,argv,&xmk);
	memset(history,0,sizeof(history));
	while(True)
	{
		int i;
		if(!useFifo || !wap || wap->reps) // auto-repeat kinda eats fifo events :(
			getInput(d,&e,&xmk,km);
		if(!e.type)
			continue;
		button=e.xbutton.button;
		//get current input window & it's name
		i=CurrentTime;
		if(focusOverride || !e.xbutton.subwindow) //not up to ICCCM standards
			// focusOverride: default is true, so this is the default action
			XGetInputFocus(d,&e.xbutton.subwindow,&i);
		else
		{ /* pulled from xwininfo */
			Window root,window=e.xbutton.subwindow;
			int dummyi;
			unsigned int dummy;

			if (e.xbutton.subwindow && XGetGeometry (d, e.xbutton.subwindow, &root, &dummyi, &dummyi,
						&dummy, &dummy, &dummy, &dummy) && window != root)
			{
				window = XmuClientWindow (d, e.xbutton.subwindow);
			}
			e.xbutton.subwindow = window;
			i=0;
		}

#ifdef DEBUG
		if(debug)
			printXEvent(&e);
#endif
		if(e.xbutton.subwindow)
		{
			Window root=0,win=e.xbutton.subwindow;
			wname=windowName(d,e.xbutton.subwindow);
			if(!wname)
				do
				{
					Window *kids;
					int nkids;
					if(XQueryTree(d,e.xbutton.subwindow,&root,&win,&kids,&nkids))
					{
						if(e.xbutton.subwindow!=win)
						{
							e.xbutton.subwindow=win;
							wname=windowName(d,e.xbutton.subwindow);
#ifdef DEBUG
							if(wname)
								Printf("Found window name in a parent!\n");
#endif
						}
						if(kids)
							XFree(kids);
					}
					Printf("w:%p r:%p ew:%p et:%d\n",win, root, e.xbutton.subwindow, e.type);
				} while(!wname && root!=win && root);
		}
		isdiffwin=(e.xbutton.subwindow!=oldw);
		oldw=e.xbutton.subwindow;
		if(e.xbutton.subwindow)
			XGetClassHint(d,e.xbutton.subwindow,&xch);
		else
		{
			xch.res_name=emptystr;
			xch.res_class=emptystr;
		}
		switch(e.type) // this section preempts unnecessary processing...
		{
			case MotionNotify:
				if(!useFifo && !isdiffwin && !grabbed)
					continue;
			case ButtonPress:
				if(isdiffwin || e.type==ButtonPress)
				{
					Printf("ButtonPress:window=%x(%x)\n",e.xbutton.subwindow, e.xbutton.window);
					if(!e.xbutton.subwindow)
					{
						Printf("(null)\n");
						wname=NULL;
					}
					//Printf("\trevert_to=%d\n",i);
					// get resource and class names
					Printf("resource name        =\"%s\"\n",xch.res_name);
					Printf("class name           =\"%s\"\n",xch.res_class);
				}
				break;
		}
		if (!wname) wname = strdup(emptystr);
		if (!xch.res_name) xch.res_name = strdup(emptystr);
		if (!xch.res_class) xch.res_class = strdup(emptystr);
		ungrabwap=0;
		grabwap=0;
		if(!useFifo)
		{
			if(isdiffwin || e.type==ButtonPress)
			{
				ungrabwap=findWA(d,UNGRAB,
						wname,
						xch.res_name,
						xch.res_class,
						NULL,NULL);
				if(ungrabwap)
					grabwap=findWA(d,GRAB,
							wname,
							xch.res_name,
							xch.res_class,
							NULL,NULL);
				if(grabwap)
				{
					Printf("Action defined for window overrides ungrab command\n");
					ungrabwap=0;
				}
			}
#ifdef DEBUG
			Printf("useFifo=%d\n",useFifo);
			Printf("ungrabwap=%p\n",ungrabwap);
			Printf("grabwap=%p\n",grabwap);
			Printf("grabbed=%d\n",grabbed);
			Printf("e.type=%d\n",e.type);
#endif
			if(ungrabwap && grabbed && (e.type==ButtonPress || e.type==FocusIn || !e.type))
				e.type=MotionNotify;
			// force a regrab try
			if(!ungrabwap && !grabwap && !grabbed && isdiffwin)
				e.type=MotionNotify;
		}
		switch(e.type) // now we actually do something!
		{
			case MotionNotify:
				Printf("MotionNotify\n");
				if(!useFifo)
				{
					if(grabbed && ungrabwap)
					{
						ungrabButtons(d,0);
						if(handleFocusGrab)
						{
							// notify us when the focus leaves this window
							XSelectInput(d, e.xbutton.subwindow,
									FocusChangeMask);
						}
					}
					else
						if(!grabbed)
							grabButtons(d,0);
				}
				break;
			case ButtonPress:
				Printf("ButtonPress\n");
				XQueryPointer(d,DefaultRootWindow(d),&pointer_rwindow,&pointer_window,&i,&i,&i,&i,&i);
				// Update history
				for(i=0;i<HISTORY_LENGTH-1;i++)
				{
					history[i].motion=history[i+1].motion;
					history[i].t=history[i+1].t;
				}
				history[HISTORY_LENGTH-1].motion=button;
				history[HISTORY_LENGTH-1].t     =time(NULL);
				// Configure if in root and toggling wheel
				if(!pointer_window)
				{
					if(root_wheeling)
					{
						wap=findWA(d,button,"(root)","(root)","(root)",xmk,km);
						if(strcmp(wap->id,"\\(root\\)"))
							continue; //no root action defined!
					}
					else
					{
						for(j=1,i=0;j&&i<HISTORY_LENGTH;i++)
							j=(history[i].motion%2==i%2);
						if(j && history[HISTORY_LENGTH-1].t-history[0].t < CONFIG_TIME)
						{
							openCfg(d,argv,&xmk);
							memset(history,0,sizeof(history));
						}
						else
							Printf("No config...\n");
						continue; // No wheel actions needed in root window!
					}
				}
				else
					wap=findWA(d,button,wname,xch.res_name,xch.res_class,xmk,km);
				XTestGrabControl(d,True);
				if(!wap)
				{
					Printf("Taking default action.\n");

					if(!keystyledefaults)
					{
						char* in[1]={0};
						char* out[2]={0,0};
						char bstr[9];
						struct WinAction wa={0,wname,button,in,out,1,0,0};

						snprintf(bstr,9,"Button%-2d",button);
						out[0]=bstr;
						doWA(d,(XButtonEvent*)&e.xbutton,xmk,km,&wa);
					}
					else
					{	/* old key style... */
						char* in[1]={0};
						char* out[2]={0,0};
						struct WinAction wa={0,wname,button,in,out,1,0,0};
						int k;

						j=button-4;
						if(j>=NUM_BUTTONS)
						{
							Printf("No we aren't because j(%d) is >= NUM_BUTTONS(%d)!\n",j,NUM_BUTTONS);
							break;
						}
						k=statebits[makeModMask(xmk,km)&STATE_MASK];
						if(k>=1<<NUM_STATES)
						{
							Printf("No we aren't because k(%d) is >= 1<<NUM_STATES(%d)!\n",k,1<<NUM_STATES);
							break;
						}
						
						out[0]=(char*)keys[j][k];
						wa.reps=reps[k];
						doWA(d,(XButtonEvent*)&e.xbutton,xmk,km,&wa);

						/* old code...
						int kc=XKeysymToKeycode(d, XStringToKeysym(keys[j][k]));
						Printf("keycode=%d\n",kc);
						
						for(i=0;i<reps[k]; i++)
						{
							Printf("rep=%d  key='%s'\n",i,keys[j][k]);
							//XSetInputFocus(d,e.xbutton.subwindow,RevertToParent,CurrentTime);
							modMods(d,km,xmk,False,0);
							XTestFakeKeyEvent(d,kc,True,0);
							XTestFakeKeyEvent(d,kc,False,0);
							XSync(d,False);
							modMods(d,km,xmk,True,0);
							XSync(d,False);
						} // rep loop
						*/
					}
				}
				else
					doWA(d,(XButtonEvent*)&e.xbutton,xmk,km,wap);
				XTestGrabControl(d,False);
				//XQueryPointer(d,DefaultRootWindow(d),&e.xbutton.subwindow,&e.xbutton.subwindow,&i,&i,&i,&i,&i);
				//XSetInputFocus(d,e.xbutton.subwindow,RevertToParent,CurrentTime);
				break;
		}
		freeAllX(&xch,&xmk,&wname);
	} // infinite loop
}

/*----------------------------------------------------------------------------*/
void freeAllX(XClassHint *xch, XModifierKeymap **xmk, char **wname)
{
	if(xch)
	{
		if(xch->res_name && xch->res_name!=emptystr)
		{
			XFree(xch->res_name);
			xch->res_name=NULL;
		}
		if(xch->res_class && xch->res_class!=emptystr)
		{
			XFree(xch->res_class);
			xch->res_class=NULL;
		}
	}
	if(xmk && *xmk)
	{
		XFreeModifiermap(*xmk);
		*xmk=NULL;
	}
	if(wname && *wname)
	{
		XFree(*wname);
		*wname=NULL;
	}
}
/*----------------------------------------------------------------------------*/

int *nullXError(Display *d, XErrorEvent *e)
{
	signed char errorstr[1024];

	grabbed=False;
	Printf("XError: \n");
	Printf("\tserial      : %lu\n",e->serial);
	Printf("\terror_code  : %u\n",e->error_code);
	Printf("\trequest_code: %u\n",e->request_code);
	Printf("\tminor_code  : %u\n",e->minor_code);
	Printf("\tresourceid  : %lu\n",e->resourceid);
	XGetErrorText(d,e->error_code,errorstr,1024);
	Printf("\terror string: %s\n",errorstr);
	return(0);
}

/*----------------------------------------------------------------------------*/
/* vim:ts=4:shiftwidth=4
"*/
