/* IMWheel Header File for more private functions
 * Copylefted under the Linux GNU Public License
 * Author : Jonathan Atkins <jcatki@jonatkins.org>
 * PLEASE: contact me if you have any improvements, I will gladly code good ones
 */
#ifndef IMWHEEL_H
#define IMWHEEL_H

signed char getInput(Display *d, XEvent *e, XModifierKeymap **xmk, signed char km[32]);
void grabButtons(Display *d, Window w);
void ungrabButtons(Display *d, Window w);
void freeAllX(XClassHint *xch, XModifierKeymap **xmk, char **wname);
typedef struct
{
	signed char x,y;
} Stick;

extern const char *optionusage[][2];
extern Bool grabbed;
extern int buttonFlip, useFifo, detach, quit, restart, threshhold,
			focusOverride, sensitivity, transpose, handleFocusGrab, doConfig,
			root_wheeling, autodelay, keystyledefaults;
extern int fifofd;
extern char *fifoName, *displayName;
extern Stick stick;

#endif
