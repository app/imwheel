#ifndef JAX_H
#define JAX_H

#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>

typedef struct JAX_PARAMS
{
	char *name1, *name2, **str;
} Jax_Params;

struct JAX;

typedef struct JAX_EVENTS
{
	long event;
	int type;
	int (*function)(struct JAX*);
} Jax_Events;

typedef struct JAX
{
	Display *d;						/* 92 */
	char *dname;
	int s;							/* DefaultScreen() */

	Window w;						/* 164 */
	char *geometry;
	XSetWindowAttributes xswa;		/* 155-7 */
	XWindowAttributes xwa;			/* 158-9 */
		/* 153 */
	XTextProperty wname;
	XTextProperty iname;
	XSizeHints *xsh;				/* 93, 148-9 */
	XWMHints *xwmh;					/* 154 */
	XClassHint *xch; 				/* 152 */

	int (*EH)(struct JAX*);			/* event handler */
	XEvent xe;						/* 96, 98, 129 */
	Jax_Events *events;				/* 124-9, 159-60 */

	XGCValues xgcv;					/* 96 */
	unsigned long gc_mask;
	GC gc;							/* 96, 238-41 */
	XFontStruct *xfs;				/* 92,279 */

	Colormap cmap;					/* 313- */

	Jax_Params *options;			/* 90 */
	int num_options;
	Jax_Params *resources;			/* 91 */
	int num_resources;

	struct JAX *next;
} Jax;

int JAXusage(Jax*, char*, char*);			/* jax,argv[0],usage(no options) */
int JAXshiftopts(int, int, int*, char**);	/* base,shift,argc,argv */
int JAXgetopts(Jax*, int*, char**);			/* jax,argc,argv */
int JAXgetrdb(Jax*);						/* jax */
Jax *JAXnewjax(void);

Jax *JAXinit(int*, char**, Jax_Params*, Jax_Params*);
											/* argc,argv,rdb,options */
int JAXexit(Jax *);							/* jax */

int JAXopenrootwin(Jax*);					/* jax */
int JAXdefaultGC(Jax*);						/* jax */
int JAXcreatewin(Jax*,int,int,int,int,char*,char*,int,char**);
											/* jax, x,y,w,h, winName, iconName,
											   argc,argv */
int JAXuseGeometry(Jax*,int);				/* jax, bits set not to allow
											   changes of sizes/positions
											   using the same convention
											   returned by XParseGeometry */
int JAXmapWin(Jax*,int);					/* jax, raised */

int JAXaddevents(Jax*, Jax_Events*);		/* jax, events */
int JAXeventhandler(Jax*);					/* jax */
int JAXeventnow(Jax*);						/* jax */
int JAXwaitforevent(Jax*);					/* jax */
int JAXeventloop(Jax*);						/* jax */

/* M A C R O S */

#define JAXmapsubwin(jax)					XMapSubWindows(jax->d,jax->w)
#define JAXmapwin(jax)						JAXmapWin(jax,0)
#define JAXmapraised(jax)					JAXmapWin(jax,1)
#define JAXunmapwin(jax)					XUnmapWindow(jax->d,jax->w)
#define JAXclosedisplay(jax)				XCloseDisplay(jax->d)
#define JAXraisewin(jax)					XRaiseWindow(jax->d,jax->w)
#define JAXlowerwin(jax)					XLowerWindow(jax->d,jax->w)
#define JAXmovewin(jax,dx,dy)				XMoveWindow(jax->d,jax->w,dx,dy)
#define JAXmoveresizewin(jax,dx,dy,dw,dh)	XMoveResizeWindow(jax->d,jax->w, \
												dx,dy,dw,dh)
#define JAXgetxwa(jax)						XGetWindowAttributes(jax->d, \
												jax->w, &jax->xwa)
#define JAXdefaultrootwin(jax)				DefaultRootWindow(jax->d)
#define JAXgetcmap(jax)						jax->cmap=DefaultColormap(jax->d,jax->s)

#define JAXcreategc(jax)					jax->gc=XCreateGC(jax->d,jax->w, \
												jax->gc_mask,&jax->xgcv)
#define JAXcleararea(jax,x,y,wd,h,e)		XClearArea(jax->d,jax->w, \
												x,y,wd,h,e)
#define JAXclearwindow(jax)					XClearWindow(jax->d,jax->w)
#define JAXdrawpoint(jax,x,y)				XDrawPoint(jax->d,jax->w,jax->gc, \
												x,y)
#define JAXfillrectangle(jax,x,y,wd,h)		XFillRectangle(jax->d,jax->w, \
												jax->gc,x,y,wd,h)
#define JAXline(jax,x,y,x2,y2)				XDrawLine(jax->d,jax->w, \
												jax->gc,x,y,x2,y2)
#define JAXblack(jax)						BlackPixel(jax->d,jax->s)
#define JAXwhite(jax)						WhitePixel(jax->d,jax->s)
#define JAXsetfg(jax,c)						XSetForeground(jax->d,jax->gc,c)
#define JAXsetbg(jax,c)						XSetBackground(jax->d,jax->gc,c)
#define JAXreadbitmapfile(jax,fname,width,height,bitmap,hot_x,hot_y) \
											XReadBitmapFile(jax->d, jax->w, \
												fname, \
												width, height, bitmap, \
												hot_x, hot_y)
#define JAXputbm(jax,bm,x,y,width,height)	XCopyPlane(jax->d,bm,jax->w,\
												jax->gc,\
												0,0,width,height,x,y,1)
#define JAXgetimage(jax,x,y,wd,h)			XGetImage(jax->d, jax->w, \
												x, y, wd, h, AllPlanes, ZPixmap)
#define JAXloadfont(jax,fontname)			XLoadFont(jax->d, fontname);
#define JAXloadqueryfont(jax,fontname)		jax->xfs=XLoadQueryFont(jax->d, fontname);
#define JAXqueryfont(jax)					jax->xfs=XQueryFont(jax->d, jax->xgcv.font);
#define JAXsetGCfont(jax)					XSetFont(jax->d,jax->gc,\
																jax->xfs->fid)
#define JAXdrawstring(jax,x,y,str)			XDrawString(jax->d,jax->w,jax->gc,\
															x,y,str,strlen(str))
#define JAXstringwidth(jax,str)				XTextWidth(jax->xfs,str,strlen(str))
#define JAXstringheight(jax)				(jax->xfs->ascent)
#define JAXstringfullheight(jax)			(jax->xfs->ascent+jax->xfs->descent)
#define JAXfontdescent(jax)					(jax->xfs->descent)

#define JAXsync(jax,discard)				XSync(jax->d,discard)

#define JAXsetEH(jax,eh)					jax->EH=eh
#define JAXeventsqueued(jax)				XEventsQueued(jax->d, \
												QueuedAfterReading)
#define JAXpending(jax)						XPending(jax->d)
#define JAXquerypointer(jax,root,child,rx,ry,wx,wy,keys_buttons) \
											XQueryPointer(jax->d, jax->w, \
												root, child, \
												rx, ry, wx, wy, keys_buttons)

#endif
