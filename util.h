/* IMWheel Utility Definitions
 * Copylefted under the Linux GNU Public License
 * Author : Jonathan Atkins <jcatki@jonatkins.org>
 * PLEASE: contact me if you have any improvements, I will gladly code good ones
 */
#ifndef UTIL_H
#define UTIL_H
#include <config.h>
#include <getopt.h>

#define PIDFILE PIDDIR"/imwheel.pid"

#define NUM_BUTTONS 6
#define STATE_MASK (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask)
#define NUM_STATES 3
#define MAX_MASKTRANS 8
#define ABS(a) ((a)<0?(-a):(a))

/* commands */
enum {
	UNGRAB  =100,
	REPEAT  =200,
	GRAB    =400,
	PRIORITY=800
};
#define MIN_COMMAND UNGRAB

struct Trans
{
	char *name;
	int val;
};
struct WinAction
{
	int pri;	//used to determine priority of some conflicting matches
	char *id;	//window identifier (for regex match)
	int button;	//mouse button number or command
	char **in;	//keysyms in mask
	char **out;	//keysyms out
	int reps;	//number of repetitions
	int delay;	//microsecond delay until next keypress
	int delayup;//microsecond delay while key down
};

extern int buttons[NUM_BUTTONS+1];
extern int statebits[STATE_MASK+1];
extern int debug;
extern struct WinAction *wa;
extern int num_wa;
extern struct Trans masktrans[MAX_MASKTRANS];
extern const int reps[1<<NUM_STATES];
extern const char *keys[NUM_BUTTONS][1<<NUM_STATES];
extern char *wname;
extern XClassHint xch;
extern const char *button_names[];
extern Atom ATOM_NET_WM_NAME, ATOM_UTF8_STRING, ATOM_WM_NAME, ATOM_STRING;

void getOptions(int,char**,char*,const struct option*);
char *windowName(Display*, Window);
void Printf(char *,...);
void printUsage(char*, const struct option[], const char*[][2]);
void printXEvent(XEvent*);
void delay(unsigned long);
void printXModifierKeymap(Display*, XModifierKeymap*);
void printKeymap(Display *d, char[32]);
int getbit(char*, int);
void setbit(char*, int, Bool);
void setupstatebits(void);
int isMod(XModifierKeymap*, int);
unsigned int makeModMask(XModifierKeymap*, char[32]);
unsigned int makeKeysymModMask(Display*,XModifierKeymap*, char**);
void printfState(int);
void exitString(char*, char*);
struct WinAction *getRC(void);
int wacmp(const void*, const void*);
char **getPipeArray(char*);
void printWA(struct WinAction*);
void writeRC(struct WinAction*);
struct WinAction *findWA(Display*,int,char*,char*,char*,XModifierKeymap*,char[32]);
void doWA(Display*,XButtonEvent*,XModifierKeymap*,char[32],struct WinAction*);
void modMods(Display*, char*, XModifierKeymap*, Bool, int);
void flushFifo(void);
void closeFifo(void);
void openFifo(void);
void KillIMWheel(void);
void WritePID(void);
int isUsedButton(int b);
int buttonIndex(int b);

#endif
