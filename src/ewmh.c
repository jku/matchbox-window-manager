/* matchbox - a lightweight window manager

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

#include "ewmh.h"

static void set_supported(Wm *w);
static void set_compliant(Wm *w);

void
ewmh_init(Wm *w)
{
  /*  
      The list below *MUST* be kept in the same order as the corresponding
      emun in structs.h or *everything* will break.
      Doing it like this avoids a mass of round trips on startup.
  */

  char *atom_names[] = {
    "WM_STATE",
    "WM_CHANGE_STATE",
    "WM_PROTOCOLS",
    "WM_DELETE_WINDOW",
    "WM_COLORMAP_WINDOWS",
    "_MB_THEME",
    "_MB_THEME_NAME",

    "_NET_WM_WINDOW_TYPE_TOOLBAR",
    "_NET_WM_WINDOW_TYPE_DOCK",
    "_NET_WM_WINDOW_TYPE_DIALOG",
    "_NET_WM_WINDOW_TYPE_SPLASH",
    "_NET_WM_WINDOW_TYPE_DESKTOP",
  
    "_MB_COMMAND",

    "_MB_CLIENT_EXEC_MAP",
    "_MB_CLIENT_STARTUP_LIST",
  
    "_NET_WM_STATE",
    "_NET_WM_STATE_FULLSCREEN",
    "_NET_WM_STATE_MODAL",
  
    "_NET_SUPPORTED",
    "_NET_CLIENT_LIST",
    "_NET_NUMBER_OF_DESKTOPS",
    "_NET_ACTIVE_WINDOW",
    "_NET_SUPPORTING_WM_CHECK",
  
    "_NET_CLOSE_WINDOW",
  
    "_NET_WM_NAME",
    "UTF8_STRING",
    "_NET_CLIENT_LIST_STACKING",
    "_NET_CURRENT_DESKTOP",
    "_NET_WM_DESKTOP",
    "_NET_WM_ICON",
    "_NET_DESKTOP_GEOMETRY",
    "_NET_WORKAREA",
  
    "_NET_SHOWING_DESKTOP",
  
    "_MOTIF_WM_HINTS",

    "_NET_WM_CONTEXT_HELP",
    "_NET_WM_CONTEXT_ACCEPT",

    "_NET_WM_ALLOWED_ACTIONS",
    "_NET_WM_ACTION_MOVE",
    "_NET_WM_ACTION_RESIZE",
    "_NET_WM_ACTION_MINIMIZE",
    "_NET_WM_ACTION_SHADE",
    "_NET_WM_ACTION_STICK",
    "_NET_WM_ACTION_MAXIMIZE_HORZ",
    "_NET_WM_ACTION_MAXIMIZE_VERT",
    "_NET_WM_ACTION_FULLSCREEN",
    "_NET_WM_ACTION_CHANGE_DESKTOP",
    "_NET_WM_ACTION_CLOSE",
    
    "_NET_STARTUP_ID",

    "_WIN_SUPPORTING_WM_CHECK",

    "_MB_WM_WINDOW_TYPE_MESSAGE",
    "_MB_WM_WINDOW_TYPE_MESSAGE_TIMEOUT",
    "_MB_WM_WINDOW_TYPE_MESSAGE_STATIC_0", /* Message wins which have the  */
    "_MB_WM_WINDOW_TYPE_MESSAGE_STATIC_1", /* decor but not timing         */

    "_NET_WM_WINDOW_TYPE",

    "_NET_WM_PING",
    "_NET_WM_PID",
    "WM_CLIENT_MACHINE",
    "_MB_WM_STATE_DOCK_TITLEBAR",
    "_NET_WM_CONTEXT_CUSTOM",
    "_NET_WM_WINDOW_TYPE_MENU",
    "CM_TRANSLUCENCY",
    "_MB_DOCK_TITLEBAR_SHOW_ON_DESKTOP",
    "_MB_WIN_SUB_NAME",
    "_MB_GRAB_TRANSFER",
    "_NET_WM_WINDOW_TYPE_INPUT",
    "_NET_WM_STATE_ABOVE",
    "INTEGER" 			/* XXX Needed ? */

  };

  XInternAtoms (w->dpy, atom_names, ATOM_COUNT,
                False, w->atoms);

}

void
ewmh_init_props(Wm *w)
{
  int num_desktops = 1;
  
  set_compliant(w);
  set_supported(w);
  
  XChangeProperty(w->dpy, w->root, w->atoms[_NET_NUMBER_OF_DESKTOPS],
		  XA_CARDINAL, 32, PropModeReplace,
		  (unsigned char *)&num_desktops, 1);
  
  XChangeProperty(w->dpy, w->root, w->atoms[_NET_CURRENT_DESKTOP],
		  XA_CARDINAL, 32, PropModeReplace,
		  (unsigned char *)&num_desktops, 1);
}

int
ewmh_handle_root_message(Wm *w, XClientMessageEvent *e)
{
  /* Handle client messages _sent_ to root window */

   Client *c = NULL;

   dbg("%s() called\n", __func__);

   if (e->message_type == w->atoms[_NET_ACTIVE_WINDOW])
     {
       dbg("%s() got active window message for win %li", __func__, e->window);
       if ((c = wm_find_client(w, e->window, WINDOW)) != NULL)
	 wm_activate_client(c);
       return 1;
     } 
   else if (e->message_type == w->atoms[_NET_CLOSE_WINDOW]) 
     {
       if ((c = wm_find_client(w, e->window, WINDOW)) != NULL)
	 client_deliver_delete(c);
       return 1;
     } 
   else if (e->message_type == w->atoms[WM_PROTOCOLS]
	    && e->data.l[0] == w->atoms[_NET_WM_PING]) 
     {
       if ((c = wm_find_client(w, e->data.l[1], WINDOW)) != NULL)
	 {
	   dbg("%s() pong from %s\n", __func__, c->name);

	   /* We got a response to a ping. stop pinging it now
	      until close button is pressed again. 
	   */
	   if (c->pings_pending > 0) 
	     {
	       c->pings_pending = -1;
	       w->n_active_ping_clients--;
	     }
	 }
     } 
   else if (e->message_type == w->atoms[WINDOW_STATE]) 
     {
       if (e->data.l[1] == w->atoms[WINDOW_STATE_FULLSCREEN]
	   && ((c = wm_find_client(w, e->window, WINDOW)) != NULL)
	   && c->type == MBCLIENT_TYPE_APP)
	 {
	   dbg("got EWMH fullscreen state change\n");
	   switch (e->data.l[0])
	     {
	     case _NET_WM_STATE_REMOVE:
	       if (c->flags & CLIENT_FULLSCREEN_FLAG)
		 main_client_toggle_fullscreen(c);
	       break;
	     case _NET_WM_STATE_ADD:
	       if (!(c->flags & CLIENT_FULLSCREEN_FLAG))
		 main_client_toggle_fullscreen(c);
	       break;
	     case _NET_WM_STATE_TOGGLE:
	       main_client_toggle_fullscreen(c);
	       break;
	     }
	 }
       return 1;
     } 
   else if (e->message_type == w->atoms[_NET_SHOW_DESKTOP]
	    && wm_get_desktop(w) ) 
     {
       dbg("%s() got desktop message\n", __func__);
       if (e->data.l[0] == 1)
	 { 			/* Show the desktop, if not shown */
	   if (!(w->flags & DESKTOP_RAISED_FLAG))
	     wm_toggle_desktop(w);
	 } else {                 /* Hide the desktop, if shown */
	   if (w->flags & DESKTOP_RAISED_FLAG)
	     wm_toggle_desktop(w);
	 }
     }
   
   return 0;
}

void
ewmh_update(Wm *w)
{
  ewmh_update_lists(w);
  ewmh_update_rects(w);
}

void
ewmh_update_lists(Wm *w)
{
   Client        *c = NULL;
   Window        *wins = NULL;
   int            cnt = 0;
   unsigned long  val[1];

   dbg("%s(): called %i\n", __func__, n_stack_items(w)); 

#ifdef USE_LIBSN
   unsigned char *bin_map_str = NULL; 
   int  bin_map_cnt = 0;
   SnCycle *current_cycle = w->sn_cycles;

   dbg("%s() called\n", __func__);

   while(current_cycle != NULL)
     {
       dbg("%s() checking cycle %s\n", __func__, current_cycle->bin_name);
       if (current_cycle->xid != None)
	{
	  dbg("%s() has xid %li\n", __func__, current_cycle->xid);
	  bin_map_cnt += (strlen(current_cycle->bin_name) + 32);
	  dbg("%s(): bin_map_cnt : %i, name: %s\n", __func__, 
	      bin_map_cnt, current_cycle->bin_name) ;
	}
       current_cycle = current_cycle->next;
     }
   
  if (bin_map_cnt)
    {
      bin_map_str = malloc(sizeof(unsigned char)*bin_map_cnt);
      memset( bin_map_str, 0, bin_map_cnt);
      
      current_cycle = w->sn_cycles;
      
      while(current_cycle != NULL)
	{
	  if (current_cycle->xid != None)
	    {
	      char str_tmp[256] = "";
	      sprintf(str_tmp, "%s=%li|", 
		      current_cycle->bin_name, 
		      current_cycle->xid);
	      
	      strcat(bin_map_str, str_tmp);
	      
	      dbg("%s(): bin_map_str : %s , str_tmp: %s \n", 
		  __func__, bin_map_str, str_tmp) ;
	    }
	  current_cycle = current_cycle->next;
	}
      
      if (bin_map_str) 
	{
	  XChangeProperty(w->dpy, w->root, w->atoms[MB_CLIENT_EXEC_MAP] ,
			  XA_STRING, 8, PropModeReplace,
			  (unsigned char *)bin_map_str, strlen(bin_map_str)
			  );
	  free(bin_map_str);
	} else {
	  dbg("%s() deleting MB_CLIENT_EXEC_MAP\n", __func__);
	  XDeleteProperty(w->dpy, w->root, w->atoms[MB_CLIENT_EXEC_MAP]);
	}
      XFlush(w->dpy);
    }
  else
    {
      dbg("%s() deleting MB_CLIENT_EXEC_MAP\n", __func__);
      XDeleteProperty(w->dpy, w->root, w->atoms[MB_CLIENT_EXEC_MAP]);
    }

#endif

  /* Root window client win lists */

   if (!stack_empty(w))
   {
     MBList *item = NULL;

     dbg("%s(): updating ewmh list props %i items\n", 
	 __func__, n_stack_items(w) ) ;   

     wins = malloc(sizeof(Window)*n_stack_items(w));
     
     stack_enumerate(w,c)
       {
	 wins[cnt++] = c->window;
	 dbg("%s() adding %s\n", __func__, c->name);
       }
  
     XChangeProperty(w->dpy, w->root, w->atoms[_NET_CLIENT_LIST_STACKING] ,
		     XA_WINDOW, 32, PropModeReplace,
		     (unsigned char *)wins, n_stack_items(w));

     /* Update _NET_CLIENT_LIST but with 'age' order rathe rthan stacking */

     cnt = 0;

     list_enumerate(w->client_age_list, item)
       {
	 c = (Client*)item->data;
	 wins[cnt++] = c->window;
	 dbg("%s() adding %s\n", __func__, c->name);
       }

     XChangeProperty(w->dpy, w->root, w->atoms[_NET_CLIENT_LIST] ,
		     XA_WINDOW, 32, PropModeReplace,
		     (unsigned char *)wins, n_stack_items(w));
   }

   /* Desktop showing hint */

   val[0] = (w->flags & DESKTOP_RAISED_FLAG) ? 1 : 0;

   XChangeProperty(w->dpy, w->root, w->atoms[_NET_SHOW_DESKTOP],
		   XA_CARDINAL, 32, PropModeReplace, 
		   (unsigned char *)val, 1);
  if (wins)
    free(wins);
}

void
ewmh_update_rects(Wm *w)
{
  CARD32 val[4];

  val[0] = wm_get_offsets_size(w, WEST, NULL, True);
  val[1] = wm_get_offsets_size(w, NORTH, NULL, True);
  val[2] = w->dpy_width - wm_get_offsets_size(w, WEST, NULL, True)
    - wm_get_offsets_size(w, EAST, NULL, True);
  val[3] = w->dpy_height - wm_get_offsets_size(w, NORTH, NULL, True)
    - wm_get_offsets_size(w, SOUTH, NULL, True);

  dbg("%s(): val[3] is %li\n", __func__, val[3] );

  if (w->flags & DESKTOP_DECOR_FLAG)
    {
      /* Desktop is decorated, needs to know frame border sizes  */

      val[0] += theme_frame_defined_width_get(w->mbtheme, 
						   FRAME_MAIN_WEST );
      val[2] -= theme_frame_defined_width_get(w->mbtheme, 
						   FRAME_MAIN_EAST );
      val[3] -= theme_frame_defined_height_get(w->mbtheme, FRAME_MAIN); 
    }

  dbg("%s(): val[3] now is %li\n", __func__, val[3] );

  XChangeProperty(w->dpy, w->root, w->atoms[_NET_WORKAREA],
		  XA_CARDINAL, 32, PropModeReplace, (unsigned char *)val, 4);
  
  XChangeProperty(w->dpy, w->root, w->atoms[_NET_DESKTOP_GEOMETRY],
		  XA_CARDINAL, 32, PropModeReplace, (unsigned char *)val, 4);
}

void 
ewmh_state_set(Client *c)
{
  Wm   *w = c->wm;

  Atom atom_states[2];
  int  n_atom_states = 0;

  /* We need to set ewmh state as some apps need to know when they 
   * return from fullscreen
   */

  if (c->flags & CLIENT_FULLSCREEN_FLAG)
    atom_states[n_atom_states++] = w->atoms[WINDOW_STATE_FULLSCREEN];

  if  (c->flags & CLIENT_IS_MODAL_FLAG)
    atom_states[n_atom_states++] = w->atoms[WINDOW_STATE_MODAL];

  XChangeProperty(w->dpy, c->window, w->atoms[WINDOW_STATE],
		  XA_ATOM, 32, PropModeReplace, 
		  (unsigned char *)atom_states, n_atom_states);
}

Bool 
ewmh_state_check(Client *c, Atom atom_state_wanted)
{
  Wm   *w = c->wm;

  unsigned long n;
  unsigned long extra;
  int           format, status, i;
  Atom          realType, *value = NULL;

  status = XGetWindowProperty(w->dpy, c->window,
			      w->atoms[WINDOW_STATE],
			      0L, 1000000L,
			      0, XA_ATOM, &realType, &format,
			      &n, &extra, (unsigned char **) &value);
  if (status == Success)
    {
      if (realType == XA_ATOM && format == 32 && n > 0)
	{
	  for(i=0; i < n; i++)
	    if (value[i] && value[i] == atom_state_wanted)
	      {
		if (value) XFree(value);
		return True;
	      }
	}
    }
  
  if (value) 
    XFree(value);

   return False;
}

void 
ewmh_set_allowed_actions(Wm *w, Client *c)
{
  int num_actions = 1;

  Atom actions[] = {
    w->atoms[_NET_WM_ACTION_CLOSE],
    0, 0
  }; 

  if (c->type == MBCLIENT_TYPE_APP)
    actions[num_actions++] = w->atoms[_NET_WM_ACTION_FULLSCREEN];

  if (c->type == MBCLIENT_TYPE_PANEL || c->type == MBCLIENT_TYPE_DIALOG)
    actions[num_actions++] = w->atoms[_NET_WM_ACTION_MOVE];

  XChangeProperty(w->dpy, c->window, w->atoms[_NET_WM_ALLOWED_ACTIONS],
		  XA_ATOM, 32, PropModeReplace, (unsigned char *)actions,
		  num_actions);
}

void 
ewmh_set_active(Wm *w)
{
  Client              *c;
  static unsigned long last_active; 
  unsigned long        val[1] = { 0 };

  if ((c = wm_get_visible_main_client(w)) != NULL)
    val[0] = c->window;

  if (last_active == val[0]) 	/* avoid the roundtrip if pos */
    return;

  last_active = val[0]; 

  dbg("%s() setting %li as active\n", __func__, val[0] );

  XChangeProperty(w->dpy, w->root, w->atoms[_NET_ACTIVE_WINDOW] ,
		  XA_WINDOW, 32, PropModeReplace,
		  (unsigned char *)val, 1);
}


#define PING_PENDING_MAX 2

void
ewmh_hung_app_check(Wm *w)
{
#ifndef NO_PING

  Client *c = NULL;

  if (stack_empty(w)) return;

  dbg("%s() called\n", __func__ );

  stack_enumerate(w, c)
    {
      if (c->has_ping_protocol && c->pings_pending != -1)
	{
	  XEvent e;
	  
	  c->pings_pending++;

	  dbg("%s() pinging %s\n", __func__, c->name);

	  e.type = ClientMessage;
	  e.xclient.window = c->window;
	  e.xclient.message_type = w->atoms[WM_PROTOCOLS];
	  e.xclient.format = 32;
	  e.xclient.data.l[0] = w->atoms[_NET_WM_PING];

	  /* To save a load of code bloat, we just set the timestamp
	     to the client window ID. This could be slightly evil but
	     makes things much more compact. 
	  */
	  e.xclient.data.l[1] = c->window;
	  XSendEvent(w->dpy, c->window, False, 0, &e);

	  if (c->pings_pending >= PING_PENDING_MAX)
	    {
	      client_obliterate(c);
	    }
	}
    }

#endif
}

static void set_supported(Wm *w) /*  */
{
  int num_supported = 0;

  Atom supported[] = {
    w->atoms[WINDOW_TYPE_TOOLBAR],
    w->atoms[WINDOW_TYPE_DOCK],
    w->atoms[WINDOW_TYPE_DIALOG],
    w->atoms[WINDOW_TYPE_DESKTOP],
    w->atoms[WINDOW_TYPE_SPLASH],
    w->atoms[WINDOW_TYPE_MENU],
    w->atoms[WINDOW_STATE],
    w->atoms[WINDOW_STATE_FULLSCREEN],
    w->atoms[WINDOW_STATE_MODAL],
    w->atoms[_NET_SUPPORTED],
    w->atoms[_NET_CLIENT_LIST],
    w->atoms[_NET_NUMBER_OF_DESKTOPS],
    w->atoms[_NET_ACTIVE_WINDOW],
    w->atoms[_NET_SUPPORTING_WM_CHECK],
    w->atoms[_NET_CLOSE_WINDOW],
    w->atoms[_NET_CURRENT_DESKTOP],
    w->atoms[_NET_CLIENT_LIST_STACKING],
    w->atoms[_NET_CURRENT_DESKTOP],
    w->atoms[_NET_SHOW_DESKTOP],
    w->atoms[_NET_WM_NAME],
    w->atoms[_NET_WM_ICON],
    w->atoms[_NET_WM_ALLOWED_ACTIONS],
    w->atoms[_NET_WM_ACTION_MOVE],
    w->atoms[_NET_WM_ACTION_FULLSCREEN],
    w->atoms[_NET_WM_ACTION_CLOSE],
    w->atoms[_NET_STARTUP_ID],
    w->atoms[_NET_WM_PING],
    w->atoms[_NET_WORKAREA],
    w->atoms[_NET_DESKTOP_GEOMETRY],
    w->atoms[_NET_WM_PING],
    w->atoms[_NET_WM_PID],
    w->atoms[CM_TRANSLUCENCY],
    0, 0
   };

  num_supported = sizeof(supported)/sizeof(Atom) - 2;

  /* Check to see if the theme supports help / accept buttons */
  if (( theme_frame_supports_button_type(w->mbtheme, FRAME_MAIN, 
					 BUTTON_ACTION_ACCEPT)
	|| w->config->use_title == False )
      && theme_frame_supports_button_type(w->mbtheme, FRAME_DIALOG, 
					  BUTTON_ACTION_ACCEPT))
    supported[num_supported++] = w->atoms[_NET_WM_CONTEXT_ACCEPT];

  if (( theme_frame_supports_button_type(w->mbtheme, FRAME_MAIN, 
					 BUTTON_ACTION_HELP)
	|| w->config->use_title == False )
      && theme_frame_supports_button_type(w->mbtheme, FRAME_DIALOG, 
					  BUTTON_ACTION_HELP))
    supported[num_supported++] = w->atoms[_NET_WM_CONTEXT_HELP];

  XChangeProperty(w->dpy, w->root, w->atoms[_NET_SUPPORTED],
		  XA_ATOM, 32, PropModeReplace, (unsigned char *)supported,
		  num_supported);
}

unsigned char *
ewmh_get_utf8_prop(Wm *w, Window win, Atom req_atom)
{
  Atom           type;
  int            format, result;
  long           bytes_after, n_items;
  unsigned char *str = NULL;

  result =  XGetWindowProperty (w->dpy, win, req_atom,
				0, 1024L,
				False, w->atoms[UTF8_STRING],
				&type, &format, &n_items,
				&bytes_after, (unsigned char **)&str);

  if (result != Success || str == NULL)
    {
      if (str) XFree (str);
      return NULL;
    }

  if (type != w->atoms[UTF8_STRING] || format != 8 || n_items == 0)
    {
      XFree (str);
      return NULL;
    }

  if (ewmh_utf8_validate(str, n_items))
    {
      dbg("%s() got %s\n", __func__, str);
      return str;
    }

  fprintf(stderr, "matchbox: UTF8 encoding bad on win %li\n", win);

  return NULL;  /* XXX check this value is correctly Xfree'd  */
}

#ifndef REDUCE_BLOAT

int*
ewmh_get_icon_prop_data(Wm *w, Window win)
{
  Atom           type;
  int            format, result;
  long           bytes_after, n_items;
  unsigned char *data = NULL;

  result =  XGetWindowProperty (w->dpy, win, w->atoms[_NET_WM_ICON],
				0, 100000L,
				False, XA_CARDINAL,
				&type, &format, &n_items,
				&bytes_after, (unsigned char **)&data);

  if (result != Success || data == NULL)
    {
      if (data) XFree (data);
      return NULL;
    }

  return (int *)data;
}

#endif

static void set_compliant(Wm *w) /* lets clients know were compliant (ish) */
{
   unsigned long  val[1];
   Window  win;
   XSetWindowAttributes attr;
   char *app_name = "matchbox";
   
   attr.override_redirect = True;
   win = XCreateWindow(w->dpy, w->root,
		       -200, -200, 5, 5, 0,
		       CopyFromParent,
		       CopyFromParent,
		       CopyFromParent,
		       CWOverrideRedirect, &attr );

   val[0] = win;
   
   /* Crack Needed to stop gnome session hanging ? */
   XChangeProperty(w->dpy, w->root,w->atoms[_WIN_SUPPORTING_WM_CHECK],
		   XA_WINDOW, 32, PropModeReplace, (unsigned char *)val,
		   1);

   XChangeProperty(w->dpy, win, w->atoms[_WIN_SUPPORTING_WM_CHECK],
		   XA_WINDOW, 32, PropModeReplace,
		   (unsigned char *)val, 1);

   /* Correct way of doing it */
   XChangeProperty(w->dpy, w->root, w->atoms[_NET_SUPPORTING_WM_CHECK],
		   XA_WINDOW, 32, PropModeReplace, (unsigned char *)val,
		   1);

   XChangeProperty(w->dpy, win, w->atoms[_NET_SUPPORTING_WM_CHECK],
		   XA_WINDOW, 32, PropModeReplace,
		   (unsigned char *)val, 1);


   /* set utf8 name  */
   XChangeProperty(w->dpy, win,
		   w->atoms[_NET_WM_NAME],
		   w->atoms[UTF8_STRING],
		   8, PropModeReplace,
                   (unsigned char *)app_name, strlen(app_name)+1);

   XStoreName(w->dpy, win, app_name);   
}

/* UTF8 - borrowed from glib. XXX fontconfig may actually provide these calls*/

#define UTF8_COMPUTE(Char, Mask, Len)                                         \
  if (Char < 128)                                                             \
    {                                                                         \
      Len = 1;                                                                \
      Mask = 0x7f;                                                            \
    }                                                                         \
  else if ((Char & 0xe0) == 0xc0)                                             \
    {                                                                         \
      Len = 2;                                                                \
      Mask = 0x1f;                                                            \
    }                                                                         \
  else if ((Char & 0xf0) == 0xe0)                                             \
    {                                                                         \
      Len = 3;                                                                \
      Mask = 0x0f;                                                            \
    }                                                                         \
  else if ((Char & 0xf8) == 0xf0)                                             \
    {                                                                         \
      Len = 4;                                                                \
      Mask = 0x07;                                                            \
    }                                                                         \
  else if ((Char & 0xfc) == 0xf8)                                             \
    {                                                                         \
      Len = 5;                                                                \
      Mask = 0x03;                                                            \
    }                                                                         \
  else if ((Char & 0xfe) == 0xfc)                                             \
    {                                                                         \
      Len = 6;                                                                \
      Mask = 0x01;                                                            \
    }                                                                         \
  else                                                                        \
    Len = -1;

#define UTF8_LENGTH(Char)              \
  ((Char) < 0x80 ? 1 :                 \
   ((Char) < 0x800 ? 2 :               \
    ((Char) < 0x10000 ? 3 :            \
     ((Char) < 0x200000 ? 4 :          \
      ((Char) < 0x4000000 ? 5 : 6)))))

#define UTF8_GET(Result, Chars, Count, Mask, Len)                             \
  (Result) = (Chars)[0] & (Mask);                                             \
  for ((Count) = 1; (Count) < (Len); ++(Count))                               \
    {                                                                         \
      if (((Chars)[(Count)] & 0xc0) != 0x80)                                  \
        {                                                                     \
          (Result) = -1;                                                      \
          break;                                                              \
        }                                                                     \
      (Result) <<= 6;                                                         \
      (Result) |= ((Chars)[(Count)] & 0x3f);                                  \
    }

#define UNICODE_VALID(Char)                   \
    ((Char) < 0x110000 &&                     \
     ((Char) < 0xD800 || (Char) >= 0xE000) && \
     (Char) != 0xFFFE && (Char) != 0xFFFF)

int
ewmh_utf8_len(unsigned char *str) /* Only parse _validated_ utf8 */
{
  unsigned char *p = str;

  int mask, len, result = 0;
  while (*p != '\0')
    {
      UTF8_COMPUTE(*p, mask, len);
      p += len;
      result++;
    }

  return result;
}

int
ewmh_utf8_get_byte_cnt(unsigned char *str, int num_chars)
{
  unsigned char *p = str;

  int mask, len, result = 0;
  while (*p != '\0' && num_chars-- > 0)
    {
      UTF8_COMPUTE(*p, mask, len);
      p += len;
      result += len;
    }
  return result;
}


Bool 
ewmh_utf8_validate(unsigned char *str, int max_len)
{
  unsigned char *p;

  if (str == NULL) return False;

  p = str;

  while ((max_len < 0 || (p - str) < max_len) && *p)
    {
      int i, mask = 0, len;
      unsigned int result;
      unsigned char c = (unsigned char) *p;

      UTF8_COMPUTE (c, mask, len);

      if (len == -1)
        break;

      /* check that the expected number of bytes exists in str */
      if (max_len >= 0 &&
          ((max_len - (p - str)) < len))
        break;

      UTF8_GET (result, p, i, mask, len);

      if (UTF8_LENGTH (result) != len) /* Check for overlong UTF-8 */
        break;

      if (result == -1)
        break;

      if (!UNICODE_VALID (result))
        break;

      p += len;
    }

  if (max_len >= 0 &&
      p != (str + max_len))
    return False;
  else if (max_len < 0 &&
           *p != '\0')
    return False;
  else
    return True;

}


