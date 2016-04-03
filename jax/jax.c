#include <stdio.h>
#include "jax.h"

/*****************************************************************************/
/* Option Handling                                                           */
/*****************************************************************************/

int JAXusage(Jax *jax, char *prog_name, char *usage)
{
	int j;

	fprintf(stderr,"%s",prog_name);
	for(j=0;j<jax->num_options;j++)
	{
		if(*jax->options[j].str)
			fprintf(stderr," [%s|%s <%s>]",jax->options[j].name1,jax->options[j].name2,*jax->options[j].str);
		else
			fprintf(stderr," [%s|%s]",jax->options[j].name1,jax->options[j].name2);
		fflush(stderr);
	}
	fprintf(stderr," %s\n",usage);
	return jax->num_options;
}

int JAXshiftopts(int base, int shift, int *argc, char **argv)
{
	int i;

	for(i=base+shift;i<(*argc);i++)
	{
		argv[i-shift]=argv[i];
	}
	(*argc)=(*argc)-shift;
	return(1);
}

int JAXgetopts(Jax *jax, int *argc, char **argv)
{
	int i,j,c=0;

	if(!argc || !argv)
		return(-1);
	jax->dname=NULL;
	for (i=1;i<(*argc);i++)
	{
		for(j=0;j<jax->num_options && i<(*argc);j++)
		{
			if ((jax->options[j].name1 && !strcmp(jax->options[j].name1, argv[i])) ||
				(jax->options[j].name2 && !strcmp(jax->options[j].name2, argv[i])))
			{
				c++;
				if((int)(*jax->options[j].str)>1)
				{
					if (*argc>i+1)
					{
						*jax->options[j].str=argv[i+1];
						JAXshiftopts(i, 2, argc, argv);
						j=-1;
					}
					else
					{
						fprintf(stderr,"Option \"%s\" needs a value.\n",argv[i]);
						return(0);
					}
				}
				else
				{
					*jax->options[j].str=(char*)(((int)(*jax->options[j].str+1))%2);
					JAXshiftopts(i, 1, argc, argv);
					j=-1;
				}
			}
		}
		if (i+1<(*argc) && !strcmp(argv[i],"-display"))
		{
			jax->dname=argv[i+1];
			JAXshiftopts(i, 2, argc, argv);
		}
		if (i+1<(*argc) && !strcmp(argv[i],"-geometry"))
		{
			jax->geometry=argv[i+1];
			JAXshiftopts(i, 2, argc, argv);
		}
	}
	return(c?c:-1);
}

int JAXgetrdb(Jax *jax) /* 91 */
{
	/*char rfilename[80];
	char *type;
	XrmValue xrmvalue;
	XrmDatabase rdb;*/

	return(1);
}

/*****************************************************************************/
/* Jax main stuff                                                            */
/*****************************************************************************/

Jax *JAXnewjax()
{
	Jax *jax;

	jax=(Jax*)malloc(sizeof(Jax));
	memset(jax,0,sizeof(Jax));
	return(jax);
}

Jax *JAXinit(int *argc, char **argv, Jax_Params *rdb, Jax_Params *options)
{
	Jax *jax;
	int i;

	jax=JAXnewjax();

	jax->resources=rdb;
	if (rdb)
	{
		for(i=0;rdb[i].name1!=NULL;i++);
		jax->num_resources=i;
		JAXgetrdb(jax);
	}

	jax->options=options;
	if (options)
	{
		for(i=0;options[i].name1!=NULL;i++);
		jax->num_options=i;
	}
	if(!JAXgetopts(jax, argc, argv))
	{
		fprintf(stderr,"Argument error\n");
		JAXusage(jax, argv[0], "");
		free(jax);
		return(0);
	}

	if(!(jax->d=XOpenDisplay(jax->dname)))
	{
		printf("Error opening display.\n");
		free(jax);
		return(0);
	}

	jax->s=DefaultScreen(jax->d);
	return(jax);
}

int JAXopenrootwin(Jax *jax)
{
	if(!jax)
		return(0);
	jax->w=DefaultRootWindow(jax->d);
	JAXgetxwa(jax);
	jax->cmap=DefaultColormap(jax->d, DefaultScreen(jax->d));
	return(1);
}

int JAXcreatewin(Jax *jax, int x, int y, int w, int h, char *winName, 
				char *iconName, int argc, char **argv)
{
	if(!jax)
		return(0);
	if(!(jax->w=XCreateSimpleWindow(jax->d, DefaultRootWindow(jax->d), x, y, w, h, 0, JAXwhite(jax), JAXblack(jax))))
	{
		fprintf(stderr,"Error creating window!\n");
		return(0);
	}


	if(!(jax->xch=XAllocClassHint()))
	{
		fprintf(stderr,"Error allocating class hints!\n");
		return(0);
	}
	jax->xch->res_name=(argv?argv[0]:"");
	jax->xch->res_class=(argv?argv[0]:"");

	if(!XStringListToTextProperty(&winName, 1, &jax->wname))
	{
		fprintf(stderr,"Error creating XTextProperty!\n");
		return(0);
	}
	if(!XStringListToTextProperty(&iconName, 1, &jax->iname))
	{
		fprintf(stderr,"Error creating XTextProperty!\n");
		return(0);
	}
	
	if(!(jax->xwmh=XAllocWMHints()))
	{
		fprintf(stderr,"Error allocating window manager hints!\n");
		return(0);
	}
	jax->xwmh->flags=0;

	if(!(jax->xsh=XAllocSizeHints()))
	{
		fprintf(stderr,"Error allocating size hints!\n");
		return(0);
	}
	jax->xsh->flags=(PPosition | PSize | PMinSize | PMaxSize);
	jax->xsh->height=jax->xsh->min_height=jax->xsh->max_height=h;
	jax->xsh->width=jax->xsh->min_width=jax->xsh->max_width=w;
	jax->xsh->x=x;
	jax->xsh->y=y;

	XSetWMProperties(jax->d,jax->w,&jax->wname,&jax->iname,argv,argc,jax->xsh,jax->xwmh,jax->xch);

	jax->cmap=DefaultColormap(jax->d, DefaultScreen(jax->d));

	return(1);
}

int JAXuseGeometry(Jax *jax, int not_allowed_bitmask)
{
	int x,y,w,h,bitmask,gravity;
	char def[80];

	if(!jax->d || !jax->w)
		return(0);
	if (!jax->geometry || strlen(jax->geometry)==0)
		return(1);

	sprintf(def,"%dx%d+%d+%d",jax->xsh->width,jax->xsh->height,jax->xsh->x,jax->xsh->y);
	bitmask = XWMGeometry(jax->d,DefaultScreen(jax->d),jax->geometry,def,0,jax->xsh,&x,&y,&w,&h,&gravity);
	if (XValue&bitmask && !(XValue&not_allowed_bitmask))
	{
		jax->xsh->flags|=USPosition;
		jax->xsh->x=x;
	}
	if (YValue&bitmask && !(YValue&not_allowed_bitmask))
	{
		jax->xsh->flags|=USPosition;
		jax->xsh->y=y;
	}
	if (WidthValue&bitmask && !(WidthValue&not_allowed_bitmask))
	{
		jax->xsh->flags|=USSize;
		jax->xsh->width=w;
	}
	if (HeightValue&bitmask && !(HeightValue&not_allowed_bitmask))
	{
		jax->xsh->flags|=USSize;
		jax->xsh->height=h;
	}
	
	if(jax->xsh->flags&USSize)
		XResizeWindow(jax->d,jax->w,jax->xsh->width,jax->xsh->height);
	if(jax->xsh->flags&USPosition)
		XMoveWindow(jax->d,jax->w,jax->xsh->x,jax->xsh->y);

	XSetWMNormalHints(jax->d,jax->w,jax->xsh);

	return(1);
}

int JAXmapWin(Jax *jax, int raised)
{
	if (!jax || !jax->w)
		return(0);
	if(raised)
		XMapRaised(jax->d,jax->w);
	else
		XMapWindow(jax->d,jax->w);
	XFlush(jax->d);

	if(!JAXgetxwa(jax))
		fprintf(stderr,"Error getting window atributes!\n");
	
	return(1);
}

int JAXdefaultGC(Jax *jax)
{
	jax->xgcv.foreground=JAXwhite(jax);
	jax->xgcv.background=JAXblack(jax);
	jax->gc_mask=GCForeground|GCBackground;
	jax->gc=XCreateGC(jax->d,jax->w,jax->gc_mask,&jax->xgcv);
	return(1);
}

int JAXexit(Jax *jax)
{
	XCloseDisplay(jax->d);
	return(1);
}

/*****************************************************************************/
/* Event Handling                                                            */
/*****************************************************************************/

int JAXaddevents(Jax *jax, Jax_Events *je)
{
	int i;

	if(je)
		jax->events=je;
	if(!jax->events)
		return(0);
	if (!XGetWindowAttributes(jax->d, jax->w, &jax->xwa))
		return(0);
	for(i=0;jax->events[i].event;i++)
		jax->xwa.your_event_mask|=jax->events[i].event;
	jax->xswa.event_mask=jax->xwa.your_event_mask;
	if(XChangeWindowAttributes(jax->d, jax->w, CWEventMask, &jax->xswa))
		return(1);
	/*fprintf(stderr,"Problem setting window event attributes.\n");*/
	return(1); /* should be 0, i don't get it! */
}

int JAXeventnow(Jax *jax)
{
	if(JAXeventsqueued(jax))
	{
		XNextEvent(jax->d, &jax->xe);
		switch(jax->xe.type)
		{
			case MappingNotify:
				XRefreshKeyboardMapping((XMappingEvent*)&jax->xe);
				jax->xe.type=0;
				break;
			default:
				if (jax->EH)
					jax->EH(jax);
				break;
		}
	}
	return(0);
}

int JAXwaitforevent(Jax *jax)
{
	int done=0;

	while(!done)
	{
		XNextEvent(jax->d, &jax->xe);
		switch(jax->xe.type)
		{
			case MappingNotify:
				XRefreshKeyboardMapping((XMappingEvent*)&jax->xe);
				break;
			default:
				done=1;
				break;
		}
	}
	return(0);
}

int JAXeventhandler(Jax *jax)
{
	int i;

	for(i=0;jax->events[i].event;i++)
		if(jax->xe.type==jax->events[i].type)
		{
			return(jax->events[i].function(jax));
		}
	fprintf(stderr,"Event type number %d not handled!\n",jax->xe.type);
	return(0);
}

int JAXeventloop(Jax *jax)
{
	int done=0;

	while(!done)
	{
		JAXwaitforevent(jax);
		if (jax->EH)
			done=jax->EH(jax);
		else
			done=1;
	}
	return(1);
}

/*****************************************************************************/
/* Images                                                                    */
/*****************************************************************************/

