/* 

   Copyright 2002 Matthew Allum

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MB_CMD_SET_THEME     1
#define MB_CMD_EXIT          2
#define MB_CMD_DESKTOP       3
#define MB_CMD_NEXT          4
#define MB_CMD_PREV          5
#define MB_CMD_SHOW_EXT_MENU 6
#define MB_CMD_MISC          7

#define MB_CMD_PANEL_TOGGLE_VISIBILITY 1
#define MB_CMD_PANEL_SIZE              2
#define MB_CMD_PANEL_ORIENTATION       3

#define MB_PANEL_ORIENTATION_NORTH     1
#define MB_PANEL_ORIENTATION_EAST      2
#define MB_PANEL_ORIENTATION_SOUTH     3
#define MB_PANEL_ORIENTATION_WEST      4

Display* dpy;	

static void
getRootProperty(char * name, Bool delete)
{
   Atom prop;
   Atom realType;
   unsigned long n;
   unsigned long extra;
   int format;
   int status;
   char * value;
	
   prop = XInternAtom(dpy, name, True);
   if (prop == None) {
      fprintf(stderr, "mbcontrol: Unable to find theme name\n");
      return;
   }
	
   status = XGetWindowProperty(dpy, DefaultRootWindow(dpy),
			       prop, 0L, 512L, delete,
			       AnyPropertyType, &realType, &format,
			       &n, &extra, (unsigned char **) &value);
   
   if (status != Success) { // || value == 0 || *value == 0 || n == 0) {
      fprintf(stderr, "Unable to find theme name\n");
      return;
   }

   if (value) printf("%s\n", value);

}


static void
mbcommand(int cmd_id, char *data) {

   XEvent	ev;
   Window	root;
   Atom theme_prop, cmd_prop, desktop_manager_atom;

   desktop_manager_atom = XInternAtom(dpy, "_NET_DESKTOP_MANGER",False);

   root = DefaultRootWindow(dpy);
   
   if (cmd_id == MB_CMD_SET_THEME)
   {
      theme_prop = XInternAtom(dpy, "_MB_THEME", False);
      if (theme_prop == None)
      {
	 fprintf(stderr, "No such property '%s'\n", "_MB_THEME");
	 return;
      }

      XChangeProperty(dpy, root, theme_prop, XA_STRING, 8,
		      PropModeReplace, data, strlen(data));
   }

   if (cmd_id == MB_CMD_DESKTOP)
     {
       /* Check if desktop is running */
       if (!XGetSelectionOwner(dpy, desktop_manager_atom))
	 {
	   fprintf(stderr, "Desktop not running, execing...");
	   switch (fork())
	     {
	     case 0:
	       execvp ("mbdesktop", NULL);
	       break;
	     case -1:
	       fprintf(stderr, "failed to exec mbdesktop");
	       break;
	     }
	   exit(0);
	 }
     }
   
   cmd_prop = XInternAtom(dpy, "_MB_COMMAND", False);
   if (cmd_prop == None) {
      fprintf(stderr, "No such property '%s'\n", "_MB_COMMAND");
      return;
   }
         
   memset(&ev, '\0', sizeof ev);
   ev.xclient.type = ClientMessage;
   ev.xclient.window = root; 	/* we send it _from_ root as we have no win  */
   ev.xclient.message_type = cmd_prop;
   ev.xclient.format = 8;

   ev.xclient.data.l[0] = cmd_id;

   XSendEvent(dpy, root, False, SubstructureRedirectMask|SubstructureNotifyMask, &ev);

}



static void
usage(char *progname)
{
   printf("Usage: %s [options...]\n", progname);
   printf("Options:\n");
   printf("  -t       <matchbox theme name>  switch matchbox theme\n");
   printf("  -r       Print current matchbox theme to stdout \n");
   printf("  -exit    Request matchbox to exit \n");
   printf("  -next    Page to next window \n");
   printf("  -prev    Page to previous window \n");
   printf("  -desktop Toggle desktop visibility\n");


   printf("  -mbmenu  Toggle mbmenu\n");

   printf("  -panel-id   <int>\n");
   printf("  -panel-toggle\n");
   /*
   printf("  -panel-size <int>\n");
   printf("  -panel-orientate <north|east|south|west>\n");
   */
   printf("  -h  this help\n\n");
   exit(1);
}

int main(int argc, char* argv[])
{
  char *display_name = (char *)getenv("DISPLAY");
  int i;
  
  dpy = XOpenDisplay(display_name);
  if (dpy == NULL) {
     printf("Cant connect to display: %s\n", display_name);
     exit(1);
  }
  
  /* pass command line */
  for (i=1; argv[i]; i++) {
     char *arg = argv[i];
     if (*arg=='-') {
	switch (arg[1]) 
	{
	   case 't' :
	      if (argv[i+1] != NULL)
		 mbcommand(MB_CMD_SET_THEME, argv[i+1]);
	      i++;
	      break;
	   case 'r' :
	      getRootProperty("_MB_THEME", False);
	      i++;
	      break;
	   case 'e':
	       mbcommand(MB_CMD_EXIT, NULL);
	       break;
	   case 'd':
	      mbcommand(MB_CMD_DESKTOP, NULL);
	      break;
	   case 'n':
	      mbcommand(MB_CMD_NEXT, NULL);
	      break;
	   case 'p':
	      mbcommand(MB_CMD_PREV, NULL);
	      break;
	   case 'm':
	      mbcommand(MB_CMD_SHOW_EXT_MENU, NULL);
	      break;
	   case 'x':
	      mbcommand(MB_CMD_MISC, NULL);
	      break;
	    

	   default:
	      usage(argv[0]);
	      break;
	}
     }
  }
  XSync(dpy, True);
  XCloseDisplay(dpy);

  return 0;
}    

