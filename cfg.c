/* Intellimouse Wheel Thinger Configuration Helper
 * Copylefted under the GNU Public License
 * Author : Jonathan Atkins <jcatki@jonatkins.org>
 * PLEASE: contact me if you have any improvements, I will gladly code good ones
 */
#include <config.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include <jax.h>
#include <X11/cursorfont.h>
#include "util.h"
#include "imwheel.h"

#define MAX_SCALE_W	200
#define MAX_SCALE_H	200
#define GUI_W		200
#define GUI_H		MAX_SCALE_H
#define TRIPLE(c)	c,c,c
#define MAX(a,b)	(a<b?b:a)
#define FONT "-adobe-helvetica-bold-r-*-*-10-*-*-*-*-*-*-*"

/******************************************************************************/

typedef enum
{
	BUTTON=1,
	LABEL
} Type;
typedef enum
{
//BUTTON
	REST=1<<1,
	ACTIVE=1<<2,
	DIRTY=1<<3,
// LABEL
	CENTER=1<<4,
	LEFT=1<<5,
	RIGHT=1<<6
} State;
typedef struct HOTSPOT
{
	int id;
	Type type;
	int x,y,w,h;
	char *label;
	State state;
	int (*buttonfunc)(Jax*,struct HOTSPOT*);
	void (*drawfunc)(Jax*,struct HOTSPOT*);
} Hotspot;

/******************************************************************************/

int handle_expose(Jax*);
int handle_button(Jax*);
int handle_destroy(Jax*,Hotspot*);
int handle_reload(Jax*,Hotspot*);
int GrabWheel(Jax*,Hotspot*);
void PickWindow(Jax*);
void GrabWindowImage(Jax*);
Hotspot *getHS(int);
int deleteHS(int);
int newHS(int x, int y, int w, int h, int state, int type, char* label,
			   int (*buttonfunc)(Jax*,struct HOTSPOT*),
			   void (*drawfunc)(Jax*,struct HOTSPOT*));
int doHS(Jax*, Hotspot*);
void drawHS(Jax*, Hotspot*);
int inHS(Hotspot*, int, int); //x,y
int HSeventhandler(Jax*);
void drawBox(Jax*, int,int,int,int,int,int); //x,y,w,h,light,dark

/******************************************************************************/

Jax_Events je[]=
{
	{ExposureMask, Expose, handle_expose},
	{ButtonPressMask, ButtonPress, handle_button},
	{ButtonReleaseMask, ButtonRelease, HSeventhandler},
	{ButtonMotionMask, MotionNotify, HSeventhandler},
	//{StructureNotifyMask, DestroyNotify, handle_destroy},
	{0,0,NULL}
};
int changed,depth;
unsigned long *scaleimgdata;
Window grabwin;
XImage *scaleimg;
XColor grays[]=
{
	{0,TRIPLE(65535/8*2),0,0},
	{0,TRIPLE(65535/8*4),0,0},
	{1,TRIPLE(65535/8*6),0,0},
	{1,TRIPLE(65535/8*7),0,0}
};
Hotspot *hs=NULL;
int wname_hs=0, class_hs=0, res_hs=0, *grabw_hs=0;
int numhs=0, hsid=1, numgrabw;
int save=0;
int name_w=0;

/******************************************************************************/

#ifndef HAVE_STRDUP
char* strdup(char *a)
{
	char *b;
	malloc(strlen(a)+1);
	strcpy(b,a);
	return(b);
}
#endif

/******************************************************************************/

int handle_reload(Jax *jax,Hotspot *hs)
{
	changed=1;
	return(True);
}

/******************************************************************************/

int handle_destroy(Jax *jax, Hotspot *hs)
{
	freeAllX(&xch,NULL,&wname);
	return(True);
}

/******************************************************************************/

int handle_button(Jax *jax)
{
	int rv;

	if((rv=HSeventhandler(jax)))
		return(rv);
	if(!grabwin)
	{
		int junk;//, nkids=0;
		Window junkwin,focuswin;//,parentwin=0,*kidwin=NULL;

		XQueryPointer(jax->d,JAXdefaultrootwin(jax),&junkwin,&grabwin,&junk,&junk,&junk,&junk,&junk);
		if(!grabwin)
			grabwin=junkwin;
		Printf("grabwin=0x%08x\n",(unsigned)grabwin);
		if(grabwin!=JAXdefaultrootwin(jax))
		{
			XGetInputFocus(jax->d,&focuswin,&junk);
			Printf("focuswin=0x%08x\n",(unsigned)focuswin);
			/*XQueryTree(jax->d,parentwin,&junkwin,&parentwin,&kidwin,&nkids);
			if(kidwin && nkids)
				XFree(kidwin);
			Printf("parentwin=0x%08x\n",(unsigned)parentwin);
			if(grabwin==parentwin)*/
				grabwin=focuswin;
		}
		else
			Printf("grabwin=root window\n");
		Printf("grabbed 0x%08x\n",(unsigned)grabwin);
		return(1);
	}
	else
	{
		if(jax->xe.xbutton.button==1 && jax->xe.xbutton.x<MAX_SCALE_W)
		{
			drawBox(jax,0,0,MAX_SCALE_W,MAX_SCALE_H,grays[0].pixel,grays[2].pixel);
			drawBox(jax,1,1,MAX_SCALE_W-2,MAX_SCALE_H-2,grays[0].pixel,grays[2].pixel);
			PickWindow(jax);
			GrabWindowImage(jax);
		}
	}
	return(save);
}

/******************************************************************************/

int handle_expose(Jax *jax)
{
	static Region clip=NULL;
	static XRectangle rect;

	jax->xe.xany.type=Expose; //force for HS expose handler
	if (!clip)
		clip=XCreateRegion();
	rect.x=jax->xe.xexpose.x;
	rect.y=jax->xe.xexpose.y;
	rect.width=jax->xe.xexpose.width;
	rect.height=jax->xe.xexpose.height;
	//printf("Expose %d (%d,%d,%d,%d)\n",jax->xe.xexpose.count,rect.x,rect.y,rect.width,rect.height);
	XUnionRectWithRegion(&rect, clip, clip);
	if(jax->xe.xexpose.count>0)
		return(0);
	XSetRegion(jax->d,jax->gc,clip);
	XDestroyRegion(clip);
	clip=NULL;

	JAXsetfg(jax,grays[1].pixel);
	JAXfillrectangle(jax,0,0,MAX_SCALE_W+GUI_W,MAX(GUI_H,MAX_SCALE_H));

	HSeventhandler(jax);

	XPutImage(jax->d,jax->w,jax->gc,scaleimg,0,0,0,0,scaleimg->width,scaleimg->height);
	drawBox(jax,0,0,MAX_SCALE_W,MAX_SCALE_H,grays[2].pixel,grays[0].pixel);
	drawBox(jax,1,1,MAX_SCALE_W-2,MAX_SCALE_H-2,grays[2].pixel,grays[0].pixel);
	XSetClipMask(jax->d,jax->gc,None);
	return(0);
}

/******************************************************************************/
/* the first thing called... */
int cfg(int fd)
{
	Jax *jax;
	pid_t pid;
	int i;

	changed=0;
	save=0;
	if((pid=fork()))
	{
		int status;
		waitpid(pid,&status,0);
		if(WIFSIGNALED(status))
			fprintf(stderr,"Configuration terminated by signal %d\n",WTERMSIG(status));
		Printf("cfg:status=%d\n",status);
		Printf("cfg:WIFSIGNALED(status)=%d\n",WIFSIGNALED(status));
		Printf("cfg:WEXITSTATUS(status)=%d\n",WEXITSTATUS(status));
		return(!WIFSIGNALED(status)?WEXITSTATUS(status):0);
	}
	// Init JAX
	jax=JAXinit(NULL,NULL,NULL,NULL);

	//Setup Window
	JAXcreatewin(jax,
			0,0,
			MAX_SCALE_W+GUI_W,MAX(MAX_SCALE_H,GUI_H),
			"IMWheel Configuration Helper",
			"IMWheel Configuration Helper",
			0,NULL);
	if(!XGetWindowAttributes(jax->d, jax->w, &jax->xwa))
		return(0);
	jax->xsh->flags=(PMinSize | PMaxSize | PResizeInc);
	jax->xsh->min_height=
		jax->xsh->max_height=
		jax->xsh->height=200;
	jax->xsh->min_width=
		jax->xsh->max_width=
		jax->xsh->width=400;
	jax->xsh->width_inc=
		jax->xsh->height_inc=0;
	JAXuseGeometry(jax,0);
	XSetWMProperties(jax->d,jax->w,&jax->wname,&jax->iname,NULL,0,jax->xsh,jax->xwmh,jax->xch);
	JAXdefaultGC(jax);
	JAXloadqueryfont(jax,FONT);
	JAXsetGCfont(jax);
	
	// Setup Colors
	for(i=0;i<sizeof(grays)/sizeof(XColor);i++)
		XAllocColor(jax->d,jax->cmap,&grays[i]);

	// Setup Event Stuff
	JAXaddevents(jax,je);
	JAXsetEH(jax,JAXeventhandler);

	// Grab Root Window
	depth=(jax->xwa.depth==24?32:jax->xwa.depth);
	scaleimgdata=malloc(MAX_SCALE_W*MAX_SCALE_H*depth/8);
	scaleimg=XCreateImage(jax->d,
			jax->xwa.visual,
			jax->xwa.depth,
			ZPixmap,
			0,
			(char*)scaleimgdata,
			MAX_SCALE_W,MAX_SCALE_H,
			depth,
			depth/8*MAX_SCALE_H);
	grabwin=JAXdefaultrootwin(jax);
	freeAllX(&xch,NULL,&wname);
	wname=windowName(jax->d,grabwin);
	XGetClassHint(jax->d,grabwin,&xch);
	GrabWindowImage(jax);

//Hotspot *newHS(int x, int y, int w, int h, int state, int type, char* label, handle(), draw())
	{
		int x,y,h,w,t;

		x=MAX_SCALE_W;
		h=JAXstringfullheight(jax)+2;
		w=JAXstringwidth(jax,"Title: ");
		t=JAXstringwidth(jax,"Resource: ");
		if(w<t) w=t;
		t=JAXstringwidth(jax,"Class: ");
		if(w<t) w=t;
		name_w=w=w+2;

		y=0;
		newHS(x,y,w,h,RIGHT,LABEL,"Title: ",NULL,NULL);
		y+=h;
		newHS(x,y,w,h,RIGHT,LABEL,"Resource: ",NULL,NULL);
		y+=h;
		newHS(x,y,w,h,RIGHT,LABEL,"Class: ",NULL,NULL);

		x+=name_w;
		w=GUI_W-name_w;

		y=0;
		wname_hs=newHS(x,y,w,h,LEFT,LABEL,(wname?wname:".*"),NULL,NULL);
		y+=h;
		res_hs=newHS(x,y,w,h,LEFT,LABEL,xch.res_name,NULL,NULL);
		y+=h;
		class_hs=newHS(x,y,w,h,LEFT,LABEL,xch.res_class,NULL,NULL);
		y+=h;

		x=MAX_SCALE_W;
		h=JAXstringfullheight(jax)+4;
		w=GUI_W;
		// put mouse&modifier stuff here!
		newHS(x,y,w,h,REST,BUTTON,"Grab Wheel Action",GrabWheel,NULL);
		y+=h;
		h=JAXstringfullheight(jax)+2;
		for(i=0;y+h<GUI_H-h;i++)
		{
			grabw_hs=realloc(grabw_hs,sizeof(int)*(numgrabw+1));
			grabw_hs[numgrabw]=newHS(x,y,w,h,LEFT,LABEL," ",NULL,NULL);
			numgrabw++;
			y+=h;
		}
		
		x=MAX_SCALE_W;
		h=GUI_H-y;
		//y=GUI_H-h;
		w=GUI_W/2;

		newHS(x,y,w,h,REST,BUTTON,"Reload",handle_reload,NULL);
		x+=w;
		newHS(x,y,w,h,REST,BUTTON,"Cancel",handle_destroy,NULL);
	}

	// Show Window
	JAXmapraised(jax);
	
	// Run config!
	JAXeventloop(jax);
	
	// return whether configuration has changed
	Printf("cfg(2):changed=%d\n",changed);
	exit(changed);
}

/******************************************************************************/

void GrabWindowImage(Jax *jax)
{
	int x,y,cx,cy,xx,yy;
	double dx,dy;
	XImage *grabimg;
	XWindowAttributes grabxwa;

	XGetWindowAttributes(jax->d,grabwin,&grabxwa);
	JAXlowerwin(jax);
	XSync(jax->d,False);
	usleep(1000);
	XSync(jax->d,False);
	if(!(grabimg=XGetImage(jax->d, grabwin,
			0, 0,
			grabxwa.width, grabxwa.height,
			AllPlanes, ZPixmap)))
	{
		fprintf(stderr,"Couldn't grab image of window!\nTry again...\n");
		JAXraisewin(jax);
		return;
	}
	//fprintf(stderr,"grabbed %d\n",grabwin);
	JAXraisewin(jax);
	memset(scaleimgdata,0L,MAX_SCALE_W*MAX_SCALE_H*(jax->xwa.depth/8));
	// Precalculation
	dx=grabimg->width/((double)scaleimg->width-5);
	dy=grabimg->height/((double)scaleimg->height-5);
	// keep perspective!
	if(dy<1) dy=1;
	if(dx<1) dx=1;
	if(dx<dy)
		dx=dy;
	else
		dy=dx;
	cx=scaleimg->width/2-grabimg->width/dx/2;
	cy=scaleimg->height/2-grabimg->height/dy/2;
	// Image
	for(x=0;x<scaleimg->width;x++)
		for(y=0;y<scaleimg->height;y++)
		{
			if((int)(y*dy)<grabimg->height && (x*dx)<grabimg->width)
			{
				XPutPixel(scaleimg,
						x+cx,y+cy,
						XGetPixel(grabimg, (int)(x*dx), (int)(y*dy)));
			}
			else
			{
				xx=(x+cx)%scaleimg->width;
				yy=(y+cy)%scaleimg->height;
					XPutPixel(scaleimg,
							xx,yy,
							grays[1].pixel);
			}
		}
	XDestroyImage(grabimg);
	grabimg=NULL;
	jax->xe.xexpose.count=-1;
	jax->xe.xexpose.x=0;
	jax->xe.xexpose.y=0;
	jax->xe.xexpose.width=MAX_SCALE_W;
	jax->xe.xexpose.height=MAX_SCALE_H;
	handle_expose(jax);
}

/******************************************************************************/

int GrabWheel(Jax *jax,Hotspot *hs)
{
	int i,hsi;
	XModifierKeymap *xmk=NULL;
	char km[32],button;
	XEvent e;
	Hotspot *hsp;
	char *str;
	char *wheelstr[]=
	{
		"Button: Left (Button1)",
		"Button: Middle (Button2)",
		"Button: Right (Button3)",
		"Wheel: Up (Button4)",
		"Wheel: Down (Button5)",
		"Wheel: Left (Button6)",
		"Wheel: Right (Button7)",
		"Button: Thumb1 (Button8)",
		"Button: Thumb2 (Button9)"
	};

	if(!useFifo)
	{
		if(!grabbed)
		{
			grabButtons(jax->d,0);
			Printf("grabbed=%s\n",(grabbed?"True":"False"));
		}
	}
	else
		openFifo();
	button=0;
	if(grabbed || useFifo)
		do
		{
			button=getInput(jax->d,&e,&xmk,km);
			if(e.type==ButtonPress)
				Printf("> button=%d\n",button);
		}
		while((grabbed || useFifo) && (button<4 || e.type!=ButtonPress));
	Printf("button=%d\n",button);
	if(!button)
	{
		Printf("Invalid button: grabbed=%d useFifo=%d\n",grabbed,useFifo);
		return(False);
	}
	if(!useFifo && grabbed)
		ungrabButtons(jax->d,0);
	hsp=getHS(grabw_hs[0]);
	jax->xe.xexpose.count=-1;
	jax->xe.xexpose.x=hsp->x;
	jax->xe.xexpose.y=hsp->y;
	jax->xe.xexpose.width=hsp->w;
	jax->xe.xexpose.height=0;
	hsp=getHS(grabw_hs[0]);
	if(hsp->label)
		free(hsp->label);
	if(button<10)
		hsp->label=strdup(wheelstr[(int)button-1]);
	else
		hsp->label=strdup("Unknown Button!");
	jax->xe.xexpose.height+=hsp->h;
	hsi++;
	for(hsi=1,i=0;hsi<numgrabw && i<32*8;i++)
		if(getbit(km,i))
		{
			hsp=getHS(grabw_hs[hsi]);
			if(hsp->label)
				free(hsp->label);
			str=XKeysymToString(XKeycodeToKeysym(jax->d,i,0));
			hsp->label=strdup(str?str:"(null)");
			jax->xe.xexpose.height+=hsp->h;
			hsi++;
		}
	while(hsi<numgrabw)
	{
		hsp=getHS(grabw_hs[hsi]);
		if(hsp->label)
			free(hsp->label);
		hsp->label=strdup(" ");
		jax->xe.xexpose.height+=hsp->h;
		hsi++;
	}
	handle_expose(jax);
	freeAllX(NULL,&xmk,NULL);
	return(False);
}

/******************************************************************************/

void PickWindow(Jax *jax)
{
	Cursor plus_cursor;

	plus_cursor=XCreateFontCursor(jax->d,XC_gumby);
	XGrabButton(jax->d,
			1,
			AnyModifier,
			JAXdefaultrootwin(jax),
			True,
			ButtonPressMask,
			GrabModeAsync,GrabModeAsync,
			None,
			plus_cursor);
	grabwin=0;
	JAXeventloop(jax);
	XUngrabButton(jax->d,
			1,
			AnyModifier,
			JAXdefaultrootwin(jax));
	XFreeCursor(jax->d,plus_cursor);

	Printf("grabwin=%08x\n",(unsigned)grabwin);
	freeAllX(&xch,NULL,&wname);
	wname=windowName(jax->d,grabwin);
	XGetClassHint(jax->d,grabwin,&xch);
	{
		int x,y,h,w;

		h=JAXstringfullheight(jax)+2;
		w=GUI_W-name_w;
		x=MAX_SCALE_W+name_w;

		y=0;
		deleteHS(wname_hs);
		Printf("wname=%s\n",wname);
		wname_hs=newHS(x,y,w,h,LEFT,LABEL,(wname?wname:".*"),NULL,NULL);
		y+=h;
		deleteHS(res_hs);
		Printf("xch.res_name=%s\n",xch.res_name);
		res_hs=newHS(x,y,w,h,LEFT,LABEL,xch.res_name,NULL,NULL);
		y+=h;
		deleteHS(class_hs);
		Printf("xch.res_class=%s\n",xch.res_class);
		class_hs=newHS(x,y,w,h,LEFT,LABEL,xch.res_class,NULL,NULL);

		jax->xe.xexpose.count=-1;
		jax->xe.xexpose.x=x;
		jax->xe.xexpose.y=0;
		jax->xe.xexpose.width=w;
		jax->xe.xexpose.height=y+h;
		handle_expose(jax);
	}
}

/******************************************************************************/

Hotspot *getHS(int id)
{
	int i;

	if(!id)
		return(NULL);
	for(i=0;i<numhs && hs[i].id!=id; i++);
	if(i>=numhs)
		return(NULL);
	return(&hs[i]);
}

/******************************************************************************/

int deleteHS(int id)
{
	int i;

	if(!id)
		return(0);
	for(i=0;i<numhs && hs[i].id!=id; i++);
	if(i>=numhs)
		return(0);
	numhs--;
	memcpy(&hs[i],&hs[i+1],(numhs-i)*sizeof(Hotspot));
	hs=realloc(hs,(numhs)*sizeof(Hotspot));
	return(1);
}

/******************************************************************************/

int newHS(int x, int y, int w, int h, int state, int type, char* label,
			   int (*buttonfunc)(Jax*,Hotspot*),
			   void (*drawfunc)(Jax*,Hotspot*))
{
	hs=realloc(hs,(numhs+1)*sizeof(Hotspot));
	hs[numhs].id=hsid++;
	hs[numhs].x=x;
	hs[numhs].y=y;
	hs[numhs].w=w;
	hs[numhs].h=h;
	hs[numhs].state=state|DIRTY;
	hs[numhs].type=type;
	// label is malloc'd then copied
	if(label)
		hs[numhs].label=strdup(label);
	else
	{
		hs[numhs].label=malloc(1);
		hs[numhs].label[0]=0;
	}
	if(buttonfunc)
		hs[numhs].buttonfunc=buttonfunc;
	else
		hs[numhs].buttonfunc=doHS;
	if(drawfunc)
		hs[numhs].drawfunc=drawfunc;
	else
		hs[numhs].drawfunc=drawHS;
	numhs++;
	return(hs[numhs-1].id);
}

/******************************************************************************/

int inHS(Hotspot *h, int x, int y)
{
	return(x>=h->x && x<=h->x+h->w &&
	   y>=h->y && y<=h->y+h->h);
}

/******************************************************************************/

int HSeventhandler(Jax *jax)
{
	int i;

	for(i=0; i<numhs; i++)
	{
		Printf("HS event: checking '%s'\n",hs[i].label);
		switch(jax->xe.xany.type)
		{
			case ButtonPress:
				if(jax->xe.xbutton.button!=1)
					return(0);
				if(inHS(&hs[i],jax->xe.xbutton.x, jax->xe.xbutton.y))
				{
					doHS(jax,&hs[i]);
					return(0);
				}
				break;
			case ButtonRelease:
				if(jax->xe.xbutton.button!=1)
					return(0);
				if(inHS(&hs[i],jax->xe.xbutton.x, jax->xe.xbutton.y))
					return(doHS(jax,&hs[i]));
				else
					if(hs[i].state&ACTIVE)
					{
						doHS(jax,&hs[i]);
						return(0);
					}
				break;
			case Expose:
				hs[i].state|=DIRTY;
				drawHS(jax,&hs[i]);
				break;
			case MotionNotify:
				if(hs[i].state&ACTIVE)
					return(doHS(jax,&hs[i]));
				break;
		}
	}
	return(0);
}

/******************************************************************************/

int doHS(Jax *jax, Hotspot *h)
{
	int err=0;

	//printf("doHS: '%s'\n",h->label);
	switch(jax->xe.xany.type)
	{
		case ButtonPress:
			switch(h->type)
			{
				case BUTTON:
					h->state=ACTIVE|DIRTY;
					break;
				default:
					break;
			}
			break;
		case MotionNotify:
			switch(h->type)
			{
				case BUTTON:
					if(h->state&ACTIVE)
					{
						if(inHS(h, jax->xe.xbutton.x, jax->xe.xbutton.y))
						{
							if(h->state&REST)
								h->state=DIRTY|ACTIVE;
						}
						else
						{
							if(!(h->state&REST))
								h->state=DIRTY|ACTIVE|REST;
						}
					}
					break;
				default:
					break;
			}
			break;
		case ButtonRelease:
			switch(h->type)
			{
				case BUTTON:
					if(inHS(h, jax->xe.xbutton.x, jax->xe.xbutton.y))
						err=h->buttonfunc(jax,h);
					if(!(h->state&REST))
						h->state=REST|DIRTY;
					else
						h->state=REST;
					break;
				default:
					break;
			}
			break;
		default:
			printf("pushHS:unknown XEvent type: %d\n",jax->xe.xany.type);
			return(0);
	}
	h->drawfunc(jax,h);
	return(err);
}

/******************************************************************************/

void drawBox(Jax *jax, int x,int y,int w,int h,int light,int dark)
{
	JAXsetfg(jax,light);
	JAXline(jax,x		,y		,x+w-1,	y		);
	JAXline(jax,x		,y		,x,		y+h-1	);
	JAXsetfg(jax,dark);
	JAXline(jax,x+w-1	,y		,x+w-1,	y+h-1	);
	JAXline(jax,x		,y+h-1	,x+w-1,	y+h-1	);
}

/******************************************************************************/

void drawHS(Jax *jax, Hotspot *h)
{
	Printf("drawHS:(\"%s\")state=%x\n",h->label,h->state);
	switch(h->type)
	{
		case BUTTON:
			if(!(h->state&DIRTY))
				return;
			JAXsetfg(jax,grays[(h->state&REST?1:3)].pixel);
			JAXfillrectangle(jax,h->x,h->y,h->w,h->h);
			drawBox(jax,h->x,h->y,h->w,h->h,
				grays[(h->state&REST?2:0)].pixel,
				grays[(h->state&REST?0:2)].pixel);
			drawBox(jax,h->x+1,h->y+1,h->w-2,h->h-2,
				grays[(h->state&REST?2:0)].pixel,
				grays[(h->state&REST?0:2)].pixel);
			JAXsetfg(jax,JAXblack(jax));
			JAXdrawstring(jax,
				h->x+h->w/2-JAXstringwidth(jax,h->label)/2,
				h->y+h->h/2+JAXstringfullheight(jax)/2-JAXfontdescent(jax),
				h->label);
			break;
		case LABEL:
			JAXsetfg(jax,grays[1].pixel);
			JAXfillrectangle(jax,h->x,h->y,h->w,h->h);
			JAXsetfg(jax,JAXblack(jax));
			h->state&=~DIRTY;
			switch(h->state)
			{
				case CENTER:
					Printf("(\"%s\")state=%x CENTER\n",h->label,h->state);
					JAXdrawstring(jax,
						h->x+h->w/2-JAXstringwidth(jax,h->label)/2,
						h->y+h->h/2+JAXstringfullheight(jax)/2-JAXfontdescent(jax),
						h->label);
					break;
				case RIGHT:
					Printf("(\"%s\")state=%x RIGHT\n",h->label,h->state);
					JAXdrawstring(jax,
						h->x+h->w-JAXstringwidth(jax,h->label),
						h->y+h->h/2+JAXstringfullheight(jax)/2-JAXfontdescent(jax),
						h->label);
					break;
				case LEFT:
					Printf("(\"%s\")state=%x LEFT\n",h->label,h->state);
					JAXdrawstring(jax,
						h->x,
						h->y+h->h/2+JAXstringfullheight(jax)/2-JAXfontdescent(jax),
						h->label);
					break;
				default:
					break;
			}
	}
	h->state&=~DIRTY;
}
