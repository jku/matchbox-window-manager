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

#include "wm.h"
#include "config.h"

#ifdef USE_XSETTINGS
static void wm_xsettings_notify_cb (const char       *name,
				    XSettingsAction   action,
				    XSettingsSetting *setting,
				    void             *data);
#endif

#ifdef USE_LIBSN
static void wm_sn_timeout_check (Wm *w);

static void wm_sn_exec(Wm *w, char* name, char* bin_name, char *desc);

static void wm_sn_monitor_event_func (SnMonitorEvent *event,
				      void           *user_data);

static void wm_sn_cycle_update_root_prop(Wm *w);

static SnCycle *wm_sn_cycle_new(Wm *w, const char *bin_name);

static void wm_sn_cycle_add(Wm *w, const char *bin_name);
#endif

Wm*
wm_new(int argc, char **argv)
{
   XSetWindowAttributes sattr; /* for root win */

   Wm *w = NULL;
   XColor dummy_col;

   w = malloc(sizeof(Wm));
   memset(w, 0, sizeof(Wm));

   w->flags = STARTUP_FLAG;

   wm_load_config(w, &argc, argv);
   
   XSetErrorHandler(handle_xerror); 

   w->screen         = DefaultScreen(w->dpy);
   w->root           = RootWindow(w->dpy, w->screen);
   w->dpy_width      = DisplayWidth(w->dpy, w->screen);
   w->dpy_height     = DisplayHeight(w->dpy, w->screen);

   w->n_active_ping_clients    = 0;
   w->next_click_is_not_double = True;

   sattr.event_mask =  SubstructureRedirectMask
                       |SubstructureNotifyMask
                       |StructureNotifyMask
                       |PropertyChangeMask;

   /* Tell root win we wanna be wm */

   XChangeWindowAttributes(w->dpy, w->root, CWEventMask, &sattr);

   XSelectInput(w->dpy, w->root, sattr.event_mask);

   /* Use this 'dull' color for 'base' window backgrounds and such. 
      'Appears' to actually reduce flicker                           */
   XAllocNamedColor(w->dpy, 
		    DefaultColormap(w->dpy, w->screen), 
		    "grey", 
		    &w->grey_col, &dummy_col);

#if defined(USE_GCONF) || defined(USE_PANGO)
   g_type_init() ;
#endif 

#ifdef USE_GCONF
   w->gconf_client  = gconf_client_get_default();
   w->gconf_context = g_main_context_default ();

   if (w->gconf_client != NULL)
     {
       gconf_client_add_dir(w->gconf_client,
			    "/apps/matchbox",
			    /* GCONF_CLIENT_PRELOAD_NONE */
			    GCONF_CLIENT_PRELOAD_RECURSIVE,
			    NULL);

       gconf_client_notify_add(w->gconf_client, 
			       "/apps/matchbox",
			       gconf_key_changed_callback,
			       w,
			       NULL, 
			       NULL);
     }
   else fprintf(stderr, "matchbox: failed to initialise gconf client\n");
#endif

#ifdef USE_XSETTINGS
   w->xsettings_client = xsettings_client_new (w->dpy, w->screen,
					       wm_xsettings_notify_cb,
					       NULL, (void *)w );
#endif 

#ifndef STANDALONE
   w->pb = mb_pixbuf_new(w->dpy, w->screen); 

   if (w->config->use_icons)
   {
     w->img_generic_icon = mb_pixbuf_img_new_from_file(w->pb, GENERIC_ICON);
     if (w->img_generic_icon == NULL)
       {
	 fprintf(stderr,"matchbox: WARNING: failed to load %s . Disabling icons.\n", GENERIC_ICON); 
	 w->config->use_icons = 0;
       }
     else misc_scale_wm_app_icon(w);
   }
#endif

#ifndef NO_KBD
   keys_init(w);
#endif

   ewmh_init(w);

#ifdef USE_PANGO
   w->pgo = pango_xft_get_context (w->dpy, w->screen);
   w->pgo_fontmap = pango_xft_get_font_map (w->dpy, w->screen);
#endif

   comp_engine_init (w);

   mbtheme_init(w, w->config->theme);

   ewmh_init_props(w);

   wm_set_cursor_visibility(w, !w->config->no_cursor);

   w->curs_busy = XCreateFontCursor(w->dpy, XC_watch);
   w->curs_drag = XCreateFontCursor(w->dpy, XC_fleur);

#ifdef USE_LIBSN
   w->sn_display = sn_display_new (w->dpy, NULL, NULL); /*XXX error callbacks*/

   w->sn_context = sn_monitor_context_new (w->sn_display, 
					   DefaultScreen (w->dpy),
					   wm_sn_monitor_event_func,
					   (void *)w, NULL);
   w->sn_busy_cnt     = 0;
   w->sn_cycles       = NULL;
   w->sn_mapping_list = NULL;
#endif

   /* Panel/Dock in titlebar stuff */
   w->have_titlebar_panel = NULL;

   w->flags ^= STARTUP_FLAG; 	/* Remove startup flag */

   return w;
}


void
wm_usage(char *progname)
{
   printf("usage: %s %s [options ...]\n", progname, VERSION);
   printf("\t-display          <string> \n");
   printf("\t-theme            <string> \n");
   printf("\t-use_titlebar     <yes|no>\n");
   printf("\t-use_cursor       <yes|no>\n");
#ifndef USE_COMPOSITE
   printf("\t-use_lowlight     <yes|no>\n");
#endif
   printf("\t-use_dialog_mode  <free|static|const-horiz>\n");
   printf("\t-use_desktop_mode <decorated|plain>\n");
   printf("\t-force_dialogs    <comma seperated list of window titles>\n");
   /*
   printf("\t-ping_handler     <string>\n");
   */
#ifdef STANDALONE
   printf("\t-titlebar_panel   <x11 geometry>\n");
#endif
   printf("\n");
   printf("Compile time options:\n");
#ifdef DEBUG
   printf("\tdebug build                      yes\n");
#else
   printf("\tdebug build                      no\n");
#endif

#if defined(USE_XFT) || defined(MB_HAVE_XFT)
   printf("\tXFT support                      yes\n");
#else
   printf("\tXFT support                      no\n");
#endif

#ifdef USE_LIBSN
   printf("\tStartup Notification support     yes\n");
#else
   printf("\tStartup Notification support     no\n");
#endif

#ifdef USE_EXPAT
   printf("\tExpat support                    yes\n");
#else
   printf("\tExpat support                    no\n");
#endif

#ifdef USE_XSETTINGS
   printf("\tXSettings support                yes\n");
#else
   printf("\tXSettings support                no\n");
#endif

#ifdef MB_HAVE_PNG
   printf("\tPNG support                      yes\n");
#else
   printf("\tPNG support                      no\n");
#endif

#ifdef MB_HAVE_JPEG
   printf("\tJPG support                      yes\n");
#else
   printf("\tJPG support                      no\n");
#endif

#ifndef STANDALONE
   printf("\tTheme support                    yes\n");
#else
   printf("\tTheme support                    no\n");
#endif

#ifdef USE_GCONF
   printf("\tgconf support                    yes\n");
#else
   printf("\tgconf support                    no\n");
#endif

#ifdef MB_HAVE_PANGO
   printf("\tpango support                    yes\n");
#else
   printf("\tpango support                    no\n");
#endif

#ifdef USE_COMPOSITE
   printf("\tcomposite support                yes\n");
#else
   printf("\tcomposite support                no\n");
#endif

#ifndef NO_PING
   printf("\tping protocol support            yes\n");
#else
   printf("\tping protocol support            no\n");
#endif

   printf("\nVisit http://matchbox.handhelds.org for more info.\n");
   printf("(c) 2004 OpenedHand Ltd\n");
   exit(0);
}


void
wm_load_config (Wm   *w, 
		int  *argc, 
		char *argv[])
{
   static XrmDatabase rDB, cmdlnDB, srDB;
   char              *type;
   XrmValue          value;
   
   static int opTableEntries = 9;
   static XrmOptionDescRec opTable[] = {
      {"-theme",       ".theme",           XrmoptionSepArg, (XPointer) NULL},
      {"-use_titlebar",".titlebar",        XrmoptionSepArg, (XPointer) NULL},
      {"-display",     ".display",         XrmoptionSepArg, (XPointer) NULL},
      {"-use_cursor",  ".cursor",          XrmoptionSepArg, (XPointer) NULL},
      {"-use_lowlight",    ".lowlight",    XrmoptionSepArg, (XPointer) NULL},
      {"-use_dialog_mode", ".dialog",      XrmoptionSepArg, (XPointer) NULL},
      {"-use_desktop_mode",".desktop",     XrmoptionSepArg, (XPointer) NULL},
      {"-titlebar_panel",  ".titlebarpanel", XrmoptionSepArg, (XPointer) NULL},
      {"-force_dialogs",  ".forcedialogs", XrmoptionSepArg, (XPointer) NULL},
   };

   XrmInitialize();
   rDB = XrmGetFileDatabase(CONFDEFAULTS);   

   XrmParseCommand(&cmdlnDB, opTable, opTableEntries, "matchbox", argc, argv); 
   if (*argc != 1) wm_usage(argv[0]);

   XrmCombineDatabase(cmdlnDB, &rDB, True);

   w->config = malloc(sizeof(Wm_config));

   /* config defaults */
   w->config->use_title        = True;
   w->config->display_name[0]  = '\0';
   w->config->dbl_click_time   = 200;
   w->config->use_icons        = 16;
   w->config->no_cursor        = False;
   w->config->dialog_shade     = False;   
   w->config->dialog_stratergy = WM_DIALOGS_STRATERGY_CONSTRAINED;
   w->config->ping_handler     = NULL;

   if (XrmGetResource(rDB, "matchbox.display",
		      "Matchbox.Display",
		      &type, &value) == True)
   {
      strncpy(w->config->display_name, value.addr, (int) value.size);
      w->config->display_name[value.size] = '\0';
   } else {
      if (getenv("DISPLAY") != NULL)
	 strcpy(w->config->display_name, (char *)getenv("DISPLAY"));
   }

   if ((w->dpy = XOpenDisplay(w->config->display_name)) == NULL) {
      fprintf(stderr, "matchbox: can't open display! check your DISPLAY variable.\n");
      exit(1);
   }

   if (XResourceManagerString(w->dpy) != NULL)
     {
       srDB = XrmGetStringDatabase(XResourceManagerString(w->dpy));
       if (srDB) XrmCombineDatabase(srDB, &rDB, False);

     }

   if (XrmGetResource(rDB, "matchbox.theme",
		      "Matchbox.Theme",
		      &type, &value) == True)
   {
#ifdef STANDALONE
     fprintf(stderr, 
	     "matchbox: This matchbox build does not support themeing\n");
#else
     w->config->theme = (char *)malloc(sizeof(char)*(value.size+1));
     strncpy(w->config->theme, value.addr, (int) value.size);
     w->config->theme[value.size] = '\0';
     dbg("%s() got theme :%s ", __func__, w->config->theme);
#endif
   } else {
     w->config->theme = NULL;
   }

   if (XrmGetResource(rDB, "matchbox.forcedialogs",
		      "Matchbox.ForceDialogs",
		      &type, &value) == True)
   {
     w->config->force_dialogs = (char *)malloc(sizeof(char)*(value.size+1));
     strncpy(w->config->force_dialogs, value.addr, (int) value.size);
     w->config->force_dialogs[value.size] = '\0';
     dbg("%s() got theme :%s ", __func__, w->config->force_dialogs);
   }
   
   if (XrmGetResource(rDB, "matchbox.titlebar", "Matchbox.Titlebar",
		      &type, &value) == True)
   {
      if(strncmp(value.addr, "no", (int) value.size) == 0)
      {
	 dbg("%s() TURNING TITLE OFF\n", __func__);
	 w->config->use_title = False;
      }
   }

   if (XrmGetResource (rDB, "matchbox.cursor", "Matchbox.Cursor",
		       &type, &value) == True)
     {
       if (strncmp (value.addr, "no", (int) value.size) == 0)
	 {
	   dbg("%s() TURNING CURSOR OFF\n", __func__);
	   w->config->no_cursor = True;
	 }
     }   

   /* 
    *  Composite matchbox always uses lowlighting 
    */
#ifndef USE_COMPOSITE
   if (XrmGetResource (rDB, "matchbox.lowlight", "Matchbox.Lowlight",
		       &type, &value) == True)
     {
       if (strncmp (value.addr, "yes", (int) value.size) == 0)
	 {
#endif
	   dbg("%s() TURNING LOWLIGHT ON\n", __func__);
	   w->config->dialog_shade = True;   

	   /* values below now set in theme */
	   w->config->lowlight_params[0] = 0; 
	   w->config->lowlight_params[1] = 0; 
	   w->config->lowlight_params[2] = 0; 
	   w->config->lowlight_params[3] = 100; 
#ifndef USE_COMPOSITE
     	 }
     }
#endif

   if (XrmGetResource (rDB, "matchbox.dialog", "Matchbox.Dialog",
		       &type, &value) == True)
     {
       if (strncmp (value.addr, "free", (int) value.size) == 0)
	 {
	   w->config->dialog_stratergy = WM_DIALOGS_STRATERGY_FREE;
	 }
       else if (strncmp (value.addr, "const-horiz", (int) value.size) == 0)
	 {
	   w->config->dialog_stratergy 
	     = WM_DIALOGS_STRATERGY_CONSTRAINED_HORIZ;
	 }
       else if (strncmp (value.addr, "static", (int) value.size) == 0)
	 {
	   w->config->dialog_stratergy 
	     = WM_DIALOGS_STRATERGY_STATIC;
	 }
       else wm_usage("matchbox");
     } 

   if (XrmGetResource (rDB, "matchbox.desktop", "Matchbox.Desktop",
		       &type, &value) == True)
     {
       if (strncmp (value.addr, "decorated", (int) value.size) == 0)
	 {
	   w->flags |= DESKTOP_DECOR_FLAG;
	 }
     } 

#ifdef STANDALONE
   if (XrmGetResource(rDB, "matchbox.titlebarpanel", "Matchbox.Titlebarpanel",
		      &type, &value) == True)
   {
     int flags = 0;
     char *geom = (char *)malloc(sizeof(char)*(value.size+1));
     strncpy(geom, value.addr, (int) value.size);
     geom[value.size] = '\0';

     flags = XParseGeometry(geom, &w->toolbar_panel_x,
			    &w->toolbar_panel_y,
			    &w->toolbar_panel_w,
			    &w->toolbar_panel_h) ;

     if ((flags & XValue) && (flags & YValue) 
	 && (flags & WidthValue) && (flags & HeightValue))
       w->have_toolbar_panel = True;
     else
       fprintf(stderr, "matchbox: titlebar panel geometry string invalid\n");
     
     free(geom);
   }
#endif

   if (getenv("MB_AWT_WORKAROUND"))
     w->config->awt_workaround = True;
   else
     w->config->awt_workaround = False;
}


void
wm_init_existing(Wm *w)
{
   unsigned int nwins, i;
   Window dummyw1, dummyw2, *wins;
   XWindowAttributes attr;
   Client *c;
   
   XQueryTree(w->dpy, w->root, &dummyw1, &dummyw2, &wins, &nwins);
   for (i = 0; i < nwins; i++) {
      XGetWindowAttributes(w->dpy, wins[i], &attr);
      if (!attr.override_redirect && attr.map_state == IsViewable)
      {
	 c = wm_make_new_client(w, wins[i]);
#ifdef USE_COMPOSITE
	 if (c) c->ignore_unmap = 2; /* comp seems to unmap twice ? */
#else
	 if (c) c->ignore_unmap++;
#endif
      }
   }
   XFree(wins);
}


Client*
wm_find_client(Wm *w, Window win, int mode)
{
    Client *c = NULL;

    if (stack_empty(w)) return NULL;

    if (mode == FRAME) 
      {
	stack_enumerate_reverse(w, c)
	  if (c->frame == win || c->title_frame == win) 
	    return c;
      } 
    else 
      {
	stack_enumerate_reverse(w, c)
	  if (c->window == win) 
	    return c;
      }    
    
    return NULL;
}

/* Grab an X Event - block but With a timeout */
static Bool
get_xevent_timed(Display        *dpy, 
		 XEvent         *event_return, 
		 struct timeval *tv)
{
  if (tv->tv_usec == 0 && tv->tv_sec == 0)
    {
      XNextEvent(dpy, event_return);
      return True;
    }

  XFlush(dpy);

  if (XPending(dpy) == 0) 
    {
      int fd = ConnectionNumber(dpy);
      fd_set readset;
      FD_ZERO(&readset);
      FD_SET(fd, &readset);

      if (select(fd+1, &readset, NULL, NULL, tv) == 0) 
	return False;
      else {
	XNextEvent(dpy, event_return);
	return True;
      }

    } else {
      XNextEvent(dpy, event_return);
      return True;
    }
}

#ifdef USE_COMPOSITE

/*  For the compositing engine we need to track overide redirect  
 *  windows so the compositor can paint them. 
 *
 *  What we do is make a 'lightweight' client object. 
 *
 *  TODO: base_client_new() should really be able to do this. 
 *        and avoid any extra code. 
 */
void 
wm_handle_map_notify(Wm *w, Window win)
{
  XWindowAttributes attr;
  Client *new_client = NULL;

  /* Do we already have it ? */
  if (wm_find_client(w, win, WINDOW)) return;
  if (wm_find_client(w, win, FRAME)) return;

  dbg("%s() called for unknown window\n", __func__);

  misc_trap_xerrors();

  XGetWindowAttributes(w->dpy, win, &attr);

  if (misc_untrap_xerrors()) return; /* safety on */

  if (attr.override_redirect)
    {
      dbg("%s() making new overide redirect window\n", __func__);

      new_client = malloc(sizeof(Client));
      memset(new_client, 0, sizeof(Client));

      new_client->x      = attr.x;
      new_client->y      = attr.y;
      new_client->width  = attr.width;
      new_client->height = attr.height;
      new_client->visual = attr.visual;
      
      new_client->want_shadow = True;

      new_client->type   = MBCLIENT_TYPE_OVERRIDE;
      new_client->frame  = new_client->window = win;
      new_client->mapped = True;
      new_client->name   = strdup("");
      new_client->wm     = w;

      /* Set up the 'methods' - expect to be overidden */
      base_client_set_funcs(new_client);

      stack_append_top(new_client);

      dbg("%s() client frame is %li\n", __func__, new_client->frame);

      comp_engine_client_init(w, new_client);

      comp_engine_client_show(w, new_client);
    }

}
#endif

/* Main event loop, timeout for polling stuff */
void
wm_event_loop(Wm* w)
{
  XEvent ev;
  int hung_app_timer = 0;
  struct timeval tvt;

  for (;;) 
    {

      tvt.tv_usec = 0;
      tvt.tv_sec  = 0;

#ifdef USE_LIBSN
      if (w->sn_busy_cnt)
	tvt.tv_sec = 1;
#endif      

#ifdef USE_GCONF
      if (w->gconf_client != NULL)
	tvt.tv_sec = 1;
#endif

#ifndef NO_PING
      if (w->n_active_ping_clients)
	tvt.tv_sec = 1;
#endif

      if (get_xevent_timed(w->dpy, &ev, &tvt))
	{

	  switch (ev.type) 
	  {
#ifdef USE_COMPOSITE
	  case MapNotify:
	    wm_handle_map_notify(w, ev.xmap.window);
	    break;
#endif
	  case ButtonPress:
	    wm_handle_button_event(w, &ev.xbutton); break;
	  case MapRequest:
	    wm_handle_map_request(w, &ev.xmaprequest); break;
	  case UnmapNotify:
	    wm_handle_unmap_event(w, &ev.xunmap); break;
	  case Expose:
	    wm_handle_expose_event(w, &ev.xexpose); break;
	  case DestroyNotify:
	    wm_handle_destroy_event(w, &ev.xdestroywindow); break;
	  case ConfigureRequest:
	    wm_handle_configure_request(w, &ev.xconfigurerequest); break;
	  case ConfigureNotify:
	    wm_handle_configure_notify(w, &ev.xconfigure); break;
	  case ClientMessage:
	    wm_handle_client_message(w, &ev.xclient); break;
	  case KeyPress:
	    wm_handle_keypress(w, &ev.xkey); break;
	  case PropertyNotify:
	    wm_handle_property_change(w, &ev.xproperty); break;
#ifndef NO_KBD
	  case MappingNotify:
	    dbg("%s() got MappingNotify\n", __func__);
	    XRefreshKeyboardMapping(&ev.xmapping);
	    break;
#endif
	  default:
	    dbg("%s() ignoring event->type : %d\n", __func__, ev.type);
	    break;
	  }

	comp_engine_handle_events(w, &ev);

#ifdef USE_XSETTINGS
	if (w->xsettings_client != NULL)
	  xsettings_client_process_event(w->xsettings_client, &ev);
#endif

#ifdef USE_LIBSN
	sn_display_process_event (w->sn_display, &ev);
#endif

      } else {

	/* No X event poll checks here */
#ifdef USE_LIBSN
	if (w->sn_busy_cnt)
	    wm_sn_timeout_check (w);
#endif      

#ifdef USE_GCONF
	if (w->gconf_client != NULL)
	  g_main_context_iteration (w->gconf_context, FALSE);
#endif

#ifndef NO_PING
	/* check for hung apps every two seconds - they dont last long.. */
	if (++hung_app_timer > 2 && w->n_active_ping_clients)
	  {
	    hung_app_timer = 0;
	    ewmh_hung_app_check(w);
	  }
#endif
         }

#ifdef USE_COMPOSITE
      if (w->all_damage)
      	{
	  comp_engine_render(w, w->all_damage);
	  XFixesDestroyRegion (w->dpy, w->all_damage);
	  w->all_damage = None;
	}
#endif
    }

}

void
wm_handle_button_event(Wm *w, XButtonEvent *e)
{
   Client *p = NULL;
   Client *c = wm_find_client(w, e->window, WINDOW);

   dbg("%s() called", __func__);

   /* Raise dialogs, set focus if needed  */

   if (c)
     {
       /* Click was on window rather than decorations */

       if (c->type == MBCLIENT_TYPE_DIALOG 
	   && w->config->dialog_stratergy != WM_DIALOGS_STRATERGY_STATIC)
	 {
	   /* raise the dialog up, handle focus etc */
	   wm_activate_client(c);
	 }
       else
	 {
	   client_set_focus(c);

	   if (c->type & (MBCLIENT_TYPE_DESKTOP|MBCLIENT_TYPE_APP))
	     c->next_focused_client = NULL;
	 }

       XAllowEvents(w->dpy, ReplayPointer, CurrentTime);
       /* forward grabbed events */

     }

   c = wm_find_client(w, e->window, FRAME);

   /* remove task menu if its up */
   if (w->flags & MENU_FLAG)
   {
      if (c && c->type == MBCLIENT_TYPE_TASK_MENU ) c->button_press(c,e);

      stack_enumerate(w, p)
	if (p->type == MBCLIENT_TYPE_TASK_MENU)
	  {
	    select_client_destroy(p); 
	    break;
	  }

      return;
   }

   /* Pass the event on to the window class */
   if (c) c->button_press(c,e);
   
}


void
wm_handle_keypress(Wm *w, XKeyEvent *e)
{
#ifndef NO_KBD
  MBConfigKbdEntry *entry =  w->config->kb->entrys;
  Client *p = NULL;

#ifdef USE_LIBSN
  Bool found = False;
  SnCycle *current_cycle = w->sn_cycles;
#endif 

   dbg("%s() called\n", __func__ );

   if(w->flags & MENU_FLAG)
     {
       stack_enumerate(w, p)
	 if ( p->type == MBCLIENT_TYPE_TASK_MENU) break;
       
       if (p->type == MBCLIENT_TYPE_TASK_MENU)
	 {
	   select_client_event_loop( p, NULL );
	   select_client_destroy (p);
	   return;
	 }
       }
   
   while (entry != NULL)
     {
       if (XKeycodeToKeysym(w->dpy,e->keycode,0) == entry->key
	   && e->state == entry->ModifierMask )
	{
	  switch (entry->action) 
	    {
	    case KEY_ACTN_EXEC:
	      fork_exec(entry->sdata);
	      break;
#ifdef USE_LIBSN
	    case KEY_ACTN_EXEC_SINGLE:
	      
	      if (current_cycle != NULL)
		{
		  while(current_cycle != NULL)
		    {
		      dbg("%s(): checking %s\n", __func__, 
			  current_cycle->bin_name);
		      if (!strcmp(current_cycle->bin_name, entry->sdata)
			  && current_cycle->xid == None)
			{
			  dbg("%s() %s is already starting\n", __func__,
			      entry->sdata);
			  return;	/* entry is in process of starting  */
			}
		      current_cycle = current_cycle->next;
		    }
		}

	      current_cycle = w->sn_cycles;

	      if (!stack_empty(w))
		{
		  while(current_cycle != NULL)
		    {
		      if (current_cycle->xid != None
			  && !strcmp(current_cycle->bin_name, entry->sdata))
			{
			  p = wm_find_client(w, current_cycle->xid, WINDOW);
			  if (p)
			    {
			      p->show(p);
			      found = True;
			    }
			}
		      current_cycle = current_cycle->next;
		    }
		}

	      if (!found)
		wm_sn_exec(w, entry->sdata, entry->sdata, entry->sdata);

	      break;

	    case KEY_ACTN_EXEC_SN:
	      wm_sn_exec(w, entry->sdata, entry->sdata, entry->sdata);
	      break;
#endif
	    case KEY_ACTN_NEXT_CLIENT:
	      wm_activate_client(stack_cycle_backward(w, MBCLIENT_TYPE_APP));
	      break;
	    case KEY_ACTN_PREV_CLIENT:
	      wm_activate_client(stack_cycle_forward(w, MBCLIENT_TYPE_APP));
	      break;
	    case KEY_ACTN_CLOSE_CLIENT:
	      if (w->stack_top_app)
		client_deliver_delete(w->stack_top_app);
	      break;
	    case KEY_ACTN_TOGGLE_DESKTOP:
	      wm_toggle_desktop(w);
	      break;
	    case KEY_ACTN_TASK_MENU_ACTIVATE:
	      select_client_new(w);
	      break;
	    case KEY_ACTN_HIDE_TITLEBAR:
	      if (w->stack_top_app) 
		main_client_toggle_title_bar(w->stack_top_app);
	      break;
	    case KEY_ACTN_FULLSCREEN:
	      if (w->stack_top_app) 
		main_client_toggle_fullscreen(w->stack_top_app);
	      break;
	    }
	}
      entry = entry->next_entry;
    }
#endif
}


void
wm_handle_configure_notify(Wm *w, XConfigureEvent *e)
{
   Client *p, *cdesktop = NULL;
   Client *ctitledock   = NULL;
   int     height_diff, width_diff;
   
   dbg("%s() called\n", __func__);

   if (e->window == w->root) /* screen rotation */
   {
     /* TODO:
      * It would probably be cleaner ( though add a dep )
      * to use randr here to get more info about the rotation     
      * ( eg. direction )
      */
      dbg("%s() configure notify event called on root", __func__ );
      if (e->width  != w->dpy_width ||
	  e->height != w->dpy_height)
      {
	height_diff   = e->height - w->dpy_height;
	width_diff    = e->width  - w->dpy_width;
	w->dpy_width  = e->width; 
	w->dpy_height = e->height;
	
	if (stack_empty(w)) return;
	
	XGrabServer(w->dpy);

	/* Clear any caches so decorations get redrawn */
	theme_img_cache_clear( w->mbtheme, FRAME_MAIN );

	stack_enumerate(w, p)
	 {
	   switch (p->type)
	     {
	     case MBCLIENT_TYPE_APP :
	       p->width += width_diff;
	       p->height += height_diff;
	       p->have_cache = False;
	       
	       break;
	     case MBCLIENT_TYPE_TOOLBAR :
	       p->width += width_diff;
	       p->y += height_diff;
	       break;
	     case MBCLIENT_TYPE_PANEL :
	       if (p->flags & CLIENT_DOCK_WEST)
		 {
		   p->height += height_diff;
		 }
	       else if (p->flags & CLIENT_DOCK_EAST)
		 {
		   p->height += height_diff;
		   p->x      += width_diff;
		 }
	       else if (p->flags & CLIENT_DOCK_SOUTH)
		 {
		   p->width += width_diff;
		   p->y += height_diff;
		 }
	       else if (p->flags & CLIENT_DOCK_NORTH)
		 {
		   p->width += width_diff;
		 }
	       else if (p->flags & CLIENT_DOCK_TITLEBAR)
		 {
		   ctitledock = p;
		 }
	       break;
	     case MBCLIENT_TYPE_DIALOG :
	       dialog_client_configure(p);
	       break;
	     case MBCLIENT_TYPE_DESKTOP:
	       p->width += width_diff;
	       p->height += height_diff;
	       cdesktop = p;
	       break;
	     default:
	       break;
	     }

	   /* we leave desktop/titlebar dock till last */
	   if (p != cdesktop && p != ctitledock) 	
	     {
	       p->move_resize(p);
	       /* destroy buttons so they get reposioned */
	       client_buttons_delete_all(p);
	       p->redraw(p, False);
	       client_deliver_config(p);
	     }

	   comp_engine_client_repair (w, p);

	 }


	 if (cdesktop)
	   {
	     cdesktop->move_resize(cdesktop);
	     client_deliver_config(cdesktop);
	   }

	 if (ctitledock)
	   {
	     dockbar_client_configure(ctitledock);
	     ctitledock->move_resize(ctitledock);
	     client_deliver_config(ctitledock);
	   }

	 comp_engine_destroy_root_buffer(w);
	 comp_engine_render(w, None);

	 ewmh_update_rects(w);

	 wm_activate_client(wm_get_visible_main_client(w));

	 XSync(w->dpy, False);

	 XUngrabServer(w->dpy);
      }
   }
}


void
wm_handle_configure_request (Wm *w, XConfigureRequestEvent *e )
{
   Client         *c = wm_find_client(w, e->window, WINDOW);
   XWindowChanges  xwc;
   Bool            need_comp_update = False;
   Bool            no_configure     = False;
   unsigned long   value_mask       = 0;

   if (!c ) 
     {
       dbg("%s() could find source client %ix%i\n", __func__,
	   e->width, e->height);
       xwc.x = e->x;
       xwc.y = e->y;
       xwc.width = e->width;
       xwc.height = e->height;
       xwc.sibling = e->above;
       xwc.stack_mode = e->detail;
       XConfigureWindow(w->dpy, e->window, e->value_mask, &xwc);
       return;
     }
   
   dbg("%s() for win %s - have w: %i vs %i, h: %i" 
       "vs %i, x: %i vs %i, y: %i vs %i,\n", 
       __func__, c->name, c->height, e->height, c->width, 
       e->width, c->x, e->x, c->y, e->y );

   /* Defualts, main clients will likely end up with this */

   xwc.width        = c->width;
   xwc.height       = c->height;
   xwc.x            = c->x;
   xwc.y            = c->y;
   xwc.border_width = 0;
   xwc.sibling      = e->above;
   xwc.stack_mode   = e->detail;
   value_mask       = e->value_mask;

   /* Deal with raising - needs work, not sure if anything really
    * relies on this / or how it fits with mb. 
    */
   if (value_mask & (CWSibling|CWStackMode))
     {
       /* e->above  is sibling 
        * e->detail is stack_mode 
	*/
#ifdef DEBUG

       Client *sibling = wm_find_client(w, e->window, WINDOW);

       if (sibling)
	 {
	   switch (e->detail)
	     {
	     case Above:
	       dbg("%s() (CWSibling|CWStackMode) above %s\n",
		   __func__, sibling->name);
	       break;
	     case Below:
	       dbg("%s() (CWSibling|CWStackMode) below %s\n",
		   __func__, sibling->name);
	       break;
	     case TopIf:    	/* What do these mean ? */
	     case BottomIf: 
	     case Opposite:
	     default:
	       dbg("%s() (CWSibling|CWStackMode) uh? %s\n",
		   __func__, sibling->name);
	       break;

	     }
	 }

#endif       
       /* Just clear the flags now to be safe */
       value_mask &= ~(CWSibling|CWStackMode);
     }

   
   if (c->type == MBCLIENT_TYPE_PANEL) 	/* Docks can move */
     {
       if ( c->height != e->height || c->width != e->width
	    || c->x != e->x || c->y != e->y )
	 {
	   Window win_tmp   = c->window;
	   xwc.width        = e->width;
	   xwc.height       = e->height;
	   xwc.x            = e->x;
	   xwc.y            = e->y;
	   xwc.border_width = 0;
	   xwc.sibling      = 0;
	   xwc.stack_mode   = e->detail;
	   
	   XConfigureWindow(w->dpy, e->window, value_mask, &xwc);

	   client_deliver_config(c);
	   client_set_state(c, WithdrawnState);

	   /* Now we destroy the window and re-birth it */

	   XReparentWindow(w->dpy, c->window, w->root, e->x, e->y); 
	   c->destroy(c);
	   
	   c = wm_make_new_client(w, win_tmp);
	   if (c) c->ignore_unmap++;
	   
        } 
       return;
     } 

   if (c->type == MBCLIENT_TYPE_TOOLBAR) 	/* can change height */
     {
       if ((e->value_mask & CWHeight) && e->height 
	   && e->height != c->height && !(c->flags & CLIENT_IS_MINIMIZED))
	 {
	   int change_amount = c->height - e->height;
	   c->y += change_amount;
	   c->height = e->height;
	   c->move_resize(c);
	   client_deliver_config(c);
	   wm_update_layout(w, c, change_amount); 
	   return;
	 }
     }

   if (c->type == MBCLIENT_TYPE_DIALOG)
     {
       int req_x = e->x, req_y = e->y, req_w = e->width, req_h = e->height;

       /* Process exactly what changes have been reuested */
       if (!(e->value_mask & CWWidth))  req_w = c->width;
       if (!(e->value_mask & CWHeight)) req_h = c->height;

       if (e->x <= 0 || !(e->value_mask & CWX)) req_x = c->x;
       if (e->y <= 0 || !(e->value_mask & CWY)) req_y = c->y;

       /* Update the size the dialog is trying to get too eventually
        * - eg toolbar/panel/input windows may dissapear and make
        *      more space available. 
        */
       if (e->width && (e->value_mask & CWWidth) 
	   && e->width != c->width && e->width != c->init_width)
	 c->init_width = e->width;

       if (e->height && (e->value_mask & CWHeight) 
	   && e->height != c->height && e->height != c->init_height)
	 c->init_height = e->height;

       /* If any changes, now make them fit it into avaialable area. */
       if (req_x != c->x || req_y != c->y 
	   || req_w != c->width || req_h != c->height)
	 {
	   dialog_check_geometry(c, &req_x, &req_y, &req_w, &req_h);

	   /* make sure buttons get repositioned */
	   if (c->width != req_w)
	     client_buttons_delete_all(c);

	   comp_engine_client_hide(c->wm, c);

	   xwc.width  = c->width  = req_w;
	   xwc.height = c->height = req_h;
	   xwc.x      = c->x      = req_x;
	   xwc.y      = c->y      = req_y; 

	   /* for below - kind of dumb */
	   no_configure = True; 

	   dialog_client_move_resize(c);

	   client_deliver_config(c);

	   /* Make sure we get the damage before the move.. */

	   need_comp_update = True;
	 }
     }


   if (!no_configure) 
     {

       /* 
        * For some reason awt ( kaffe ) apps dont like getting anything
        * other than what they've asked for in there *first* configure_request
        * response ( they dont paint there UI otherwise ).
        * This is a quick fix which sends 2 configure differering events  
        * which seems to make things better. Im not sure what the best fix is. 
        *
        * Set MB_AWT_WORKAROUND env var to enable this. 
        *
        * 10/11/2004 TODO: dont think this is needed anymore. 
        *                  seems to have magically fixed itself.
        */

       if (w->config->awt_workaround)
	 {
	   xwc.width  = e->width;
	   xwc.height = e->height;

	   XConfigureWindow(w->dpy, e->window, e->value_mask, &xwc);

	   xwc.width  = c->width;
	   xwc.height = c->height;

	 }

       XConfigureWindow(w->dpy, e->window, value_mask, &xwc);

       client_deliver_config(c);
     }


   /* make sure composite does any needed updates */
   if (need_comp_update == True)
     {
       comp_engine_client_configure(w, c);
       comp_engine_client_show(w, c);
     }
}


void
wm_handle_map_request(Wm *w, XMapRequestEvent *e)
{
   Client *c = wm_find_client(w, e->window, WINDOW);

   if (!c) {
      wm_make_new_client(w, e->window);
   } else {
      dbg("%s() Honoring map request for %s\n", __func__, c->name);
      wm_activate_client(c);
   }
}

void
wm_handle_unmap_event(Wm *w, XUnmapEvent *e)
{
   XEvent ev;
   Client *c = wm_find_client(w, e->window, WINDOW);
   if (!c) return;

   dbg("%s() for client %s\n", __func__, c->name);
   if (c->ignore_unmap)
     {
       c->ignore_unmap--;
       dbg("%s() ignoring .. \n", __func__ );
     }
   else
     {
       if (!c->mapped) return;

       XGrabServer(w->dpy);

       XUnmapWindow(w->dpy, c->frame);
       XSync(w->dpy, False);

       if (XCheckTypedWindowEvent(c->wm->dpy, c->frame, DestroyNotify, &ev)) 
	 {
	   dbg("%s() destroy on its way....\n", __func__ );
	   wm_handle_destroy_event(w, &ev.xdestroywindow);
	 } 
       else
	 {
	   dbg("%s() calling client destroy\n", __func__);
	   Window win_old;

	   client_set_state(c, WithdrawnState);
	   win_old = c->window;
	   c->destroy(c);
	   XReparentWindow(w->dpy, win_old, w->root, 0, 0); 

	 }

       XUngrabServer(w->dpy);
     }
}

void
wm_handle_expose_event(Wm *w, XExposeEvent *e)
{
   Client *c = wm_find_client(w, e->window, FRAME);

   if (c)
   {
     XEvent ev;

     /* Compress expose events */
     while (XCheckTypedWindowEvent(w->dpy, e->window, Expose, &ev));

     dbg("%s() for %s\n", __func__, c->name);    

     c->redraw(c, True); /* redraw title from cache - prolly a no-op */
   }
}


void
wm_handle_destroy_event(Wm *w, XDestroyWindowEvent *e)
{
    Client *c = wm_find_client(w, e->window, WINDOW);
    if (!c) return; 

    dbg("%s for %s\n", __func__, c->name);

    wm_remove_client(w, c);
}


void
wm_handle_client_message(Wm *w, XClientMessageEvent *e)
{
   Client *c = wm_find_client(w, e->window, WINDOW);

   dbg("%s() messgae type is %li\n", __func__, e->message_type);

   /* Handle messages from mbcontrol */
   if (e->message_type == w->atoms[MB_COMMAND])
     {				
       dbg("%s() mb command requested %li\n", __func__, e->data.l[0] );
       switch (e->data.l[0])
	 {
#ifndef STANDALONE
	 case MB_CMD_SET_THEME :
	   {
	     Atom          realType;
	     unsigned long n;
	     unsigned long extra;
	     int           format;
	     int           status;
	     char         *value = NULL;

	     dbg("%s() atempting to switch theme\n", __func__ );

	     status = XGetWindowProperty(w->dpy, w->root,
					 w->atoms[_MB_THEME], 0L, 512L, False,
					 AnyPropertyType, &realType,
					 &format, &n, &extra,
					 (unsigned char **) &value);
	     
	     if (status == Success && value != NULL)
	       {
		 dbg("%s() switching theme to %s\n", __func__, value);
		 mbtheme_switch(w, value);
	       }

	     if (value) 
	       XFree(value);

	     return;
	   }
#endif
	 case MB_CMD_EXIT      :
	   exit(0);
	 case MB_CMD_NEXT      :
	   wm_activate_client(stack_cycle_backward(w, MBCLIENT_TYPE_APP));
	   break;
	 case MB_CMD_PREV      :
	   wm_activate_client(stack_cycle_forward(w, MBCLIENT_TYPE_APP));
	   break;
	 case MB_CMD_DESKTOP   :
	   wm_toggle_desktop(w);
	   break;
	 case MB_CMD_MISC:  /* This is used for random testing stuff */
#ifdef DEBUG
	   /* comp_engine_time(w); Not used atm XXX DO_TIMINGS */
	   dbg("*** Toggling composite visual debugging ***\n");
	   w->flags ^= DEBUG_COMPOSITE_VISIBLE_FLAG;
#endif
	   break;
	 }
       return;
     }
	
   if (e->message_type == w->atoms[WM_CHANGE_STATE])
     {
       dbg("%s() messagae type is change state\n", __func__ );
       if (c && e->format == 32 && e->data.l[0] == IconicState)
	 {
	   c->iconize(c);
	 }
       return;
     }

   /* Also handle any EWMH messages */
   ewmh_handle_root_message(w, e);   
}


void
wm_handle_property_change(Wm *w, XPropertyEvent *e)
{
  Bool update_titlebar = False;

  Client *c = wm_find_client(w, e->window, WINDOW);

  if (!c) return; 

  if (c->type == MBCLIENT_TYPE_OVERRIDE) return;

  dbg("%s() on %s, atom is %li\n", __func__, c->name, e->atom );
   
  /* Window Name changes */
  if (e->atom == XA_WM_NAME && !c->name_is_utf8)
    {
      if (c->name) XFree(c->name);
      XFetchName(w->dpy, c->window, (char **)&c->name);
      base_client_process_name(c);
      dbg("%s() XA_WM_NAME change, name is %s\n", __func__, c->name);
      update_titlebar = True;
    }
  else if (e->atom == w->atoms[MB_WIN_SUB_NAME])
    {
      if (c->subname) XFree(c->subname);
      c->subname =ewmh_get_utf8_prop(w, c->window, w->atoms[MB_WIN_SUB_NAME]);
      update_titlebar = True;
    }
  else if (e->atom == w->atoms[_NET_WM_NAME])
    {
      if (c->name) 
	XFree(c->name);

      c->name = ewmh_get_utf8_prop(w, c->window, w->atoms[_NET_WM_NAME]);

      if (c->name)
	c->name_is_utf8 = True;
      else
	{
	  c->name_is_utf8 = False;
	  XFetchName(w->dpy, c->window, (char **)&c->name);
	}
      base_client_process_name(c);
      dbg("%s() NET_WM_NAME change, name is %s\n", __func__, c->name);
      update_titlebar = True;
    }
  else  if (e->atom == w->atoms[WM_CHANGE_STATE])
    {
      dbg("%s() state change, name is %s\n", __func__, c->name);
      if(client_get_state(c) == WithdrawnState)
	c->destroy(c);
    }
  else if (e->atom == w->atoms[CM_TRANSLUCENCY])
    {
      comp_engine_client_get_trans_prop(w, c);
      comp_engine_client_repair(w, c);
    }
  
  if (update_titlebar)  c->redraw(c, False);
}

/* If configured force a app window be treated as a dialog */
Bool
wm_win_force_dialog(Wm *w, Window win)
{
  char *win_title = NULL;
  Bool  result    = False;

  if (!w->config->force_dialogs)
    return result;

  if (XFetchName(w->dpy, win, (char **)&win_title))
    if (strstr(w->config->force_dialogs, win_title)) /* TODO: Improve search */
      result = True;

  if (win_title) XFree(win_title);

  return result;
}

Client*
wm_make_new_client(Wm *w, Window win)
{
   Window        trans_win;
   Atom          realType, *value = NULL;
   unsigned long n, extra;
   int           format, status;
   Client       *c = NULL, *t = NULL;
   XWMHints     *wmhints;
   int           mwm_flags = 0;

   XGrabServer(w->dpy);

   dbg("%s() initiated\n", __func__);

   if (wm_win_force_dialog(w, win))
     {
       /* Hackiness to allow app wins to be forced into dialogs   
        * ( see -froce_dialogs switch ). 
        * Much better to fix the app people!
        */
       c = dialog_client_new(w, win, NULL);
       if (c == NULL) goto end;
     }
   else
     {
       /* Use EWMH Window Type Hint to figure out window type */

       status = XGetWindowProperty(w->dpy, win, w->atoms[WINDOW_TYPE], 
				   0L, 1000000L, 0, XA_ATOM, 
				   &realType, &format,
				   &n, &extra, (unsigned char **) &value);
       if (status == Success)
	 {
	   if (realType == XA_ATOM && format == 32)
	     {
	       dbg("%s() got type atom\n", __func__);
	       if (value[0] == w->atoms[WINDOW_TYPE_DOCK])
		 {
		   dbg("%s() got dock atom\n", __func__ );
		   c = dockbar_client_new(w, win);
		   if (c == NULL) goto end;
		 }
	       else if (value[0] == w->atoms[WINDOW_TYPE_TOOLBAR]
			|| value[0] == w->atoms[WINDOW_TYPE_INPUT])
		 {
		   dbg("%s() got toolbar atom\n", __func__ );
		   c = toolbar_client_new(w, win);
		   if (c == NULL) goto end;
		 }
	       else if (value[0] == w->atoms[WINDOW_TYPE_DESKTOP])
		 {
		   dbg("%s() got desktop atom\n", __func__ );
		   c = desktop_client_new(w, win);
		   if (c == NULL) goto end;
		 }
	       
	       else if (value[0] == w->atoms[WINDOW_TYPE_SPLASH])
		 {
		   dbg("%s() got splash atom\n", __func__ );
		   c = dialog_client_new(w, win, NULL);
		   if (c == NULL) goto end;
		   c->flags ^= CLIENT_TITLE_HIDDEN_FLAG;
		 }
	       else if (value[0] == w->atoms[WINDOW_TYPE_DIALOG])
		 {
		   dbg("%s() got type dialog atom\n", __func__ );
		   c = dialog_client_new(w, win, NULL);
		   if (c == NULL) goto end;
		 }
	     } 
	 }
       
       if (value) XFree(value);
     }

   if ((mwm_flags = mwm_get_decoration_flags(w, win)))
     { 
       /* for now, treat just like a splash  */
       if (c == NULL) 
	 c = dialog_client_new(w, win, NULL);

       if (c) 
	 c->flags ^= mwm_flags;

       dbg("%s() got MWM flags: %i\n", __func__, c->flags );
     }

   /* check for transient - ie detect if its a dialog */

   XGetTransientForHint(w->dpy, win, &trans_win);
   
   if (trans_win && (trans_win != win))
   {
      dbg("%s() Transient hint set\n", __func__);

      t = wm_find_client(w, trans_win, WINDOW);

      if (t == NULL)
      {
	/* Its transient for something, but not managed by us .. 
         * We check for win groups in last act of desperation.
	 */

	 Client *p = NULL;

	 dbg("%s() transient window not managed\n", __func__);

	 if ((wmhints = XGetWMHints(w->dpy, win)) != NULL)
	 {
	    if (wmhints->window_group && !stack_empty(w))
	    {
	       stack_enumerate(w, p)
		 if (wmhints->window_group == p->window)
		   { 
		     t = p; 
		     break; 
		   }
	    }
	 }
      }

      dbg("%s() Transient ( %s) looks good, creating dialog\n", 
	  __func__, t ? t->name : "none" );

      if (c == NULL)  
	{
	  /* if t is is NULL (from above), dialog will always be visible */
	  c = dialog_client_new(w, win, t);
	}
      else if (c->type == MBCLIENT_TYPE_DIALOG) /* already exists, update  */
	c->trans = t;  		/* TODO: what about other types 
				         being transient for things ?*/
   }
   
   if (c == NULL) /* Noting else found, default to a main client */
   {
     c = main_client_new(w, win);

     if (c == NULL) /* Something has gone wrong - prolly win dissapeared */
       {
	 dbg("%s() client dissapeared\n", __func__);
	 goto end;
       }
   }

   /* We do this now as really needs to know window type */

   base_client_process_name(c);

   dbg("%s() calling configure method for new client\n", __func__);
   
   c->configure(c); 		/* Size up the client */

   comp_engine_client_init(w, c);

   dbg("%s() reparenting new client\n", __func__ );
   
   c->reparent(c);             	/* reparent it to frames and decor */

   dbg("%s() move/resizing  new client\n", __func__);
   
   c->move_resize(c);          	/* set pos + size */

   /* TODO:
    *
    * Its likely the size we given the new client, is not what it requested. 
    * We've by now told the app its new size, but we need to give it a 
    * chance to repaint itself at the new size or other wise we get horrible
    * flicker on mapping ( as remenants of old size are seen ).
    *
    * A possible solutions could be to implement the new _NET_WM_SYNC_REQUEST 
    * stuff, as this is for resizes and we are resizing after all. 
    *
    *  XUngrabServer(w->dpy);
    *  XSync(w->dpy, False);
    *  XGrabServer(w->dpy);
    *
    * Note, this seems worst on GTK apps.
    */

   dbg("%s() showing new client\n", __func__);

   c->redraw(c, False);		/* draw the decorations */

   wm_activate_client(c);       /* Map it into stack, ( will call show()) */

   /* below is probably now mostly uneeded ? */

   XGrabButton(c->wm->dpy, Button1, 0, c->window, True, ButtonPressMask,
	       GrabModeSync, GrabModeSync, None, None);

   /* Let window know were all done */

   ewmh_state_set(c);

   client_set_state(c, NormalState);

 end:

   XUngrabServer(w->dpy);

   XFlush(w->dpy);

   return c;
}


void
wm_remove_client(Wm *w, Client *c)
{
  dbg("%s() called for %s\n", __func__, c->name);

  XGrabServer(c->wm->dpy);
  c->destroy(c);
  XUngrabServer(w->dpy);
}

/* wm_update_layout() is called in the presence of a panel/toolbar
 * changing its size / appearing. It re-layouts all windows for it
 * to fit. 
 */
void
wm_update_layout(Wm         *w, 
		 Client     *client_changed, 
		 signed int  change_amount) /* XXX Change to relayout */
{
 Client *p = NULL;

 XGrabServer(w->dpy);

 stack_enumerate(w,p)
   {
     if (p == client_changed)
       continue;

     dbg("%s() restacking, comparing %i is less than %i for %s\n",
	 __func__, p->y, client_changed->y, p->name);
     if (client_changed->type == MBCLIENT_TYPE_PANEL 
	 && client_changed->flags & CLIENT_DOCK_WEST)
       {
	 if (p->x >= client_changed->x) 
	   {
	     switch (p->type)
	       {
	       case MBCLIENT_TYPE_APP :
		 p->width += change_amount;
		 p->x     -= change_amount;
		 p->move_resize(p);
		 theme_img_cache_clear( w->mbtheme, FRAME_MAIN );
		 client_deliver_config(p);
		 client_buttons_delete_all(p);
		 main_client_redraw(p, False); /* force title redraw */
		 break;
	       case MBCLIENT_TYPE_TOOLBAR :
	       case MBCLIENT_TYPE_PANEL    :
		 if (p->flags & CLIENT_DOCK_EAST)
		   break;
		 if (p->flags & CLIENT_DOCK_TITLEBAR)
		   {
		     /* See notes below on this */

		     if (change_amount > 0)
		       {
			 XRectangle rect;

			 mbtheme_get_titlebar_panel_rect(p->wm->mbtheme, 
							 &rect, client_changed);
			 p->x      = rect.x + wm_get_offsets_size(p->wm, WEST, client_changed, True); 
			 p->width  = rect.width  ;
		       }
		     else p->configure(p);
		   }
		 else
		   {
		     p->width += change_amount;
		     p->x     -= change_amount;
		   }

		 p->move_resize(p);
		 client_deliver_config(p);

	       default:
		 break;
	       }
	   }
       }
     else if (client_changed->type == MBCLIENT_TYPE_PANEL 
	      && client_changed->flags & CLIENT_DOCK_EAST)
       {
	 if (p->x <= client_changed->x) 
	   {
	     switch (p->type)
	       {
	       case MBCLIENT_TYPE_APP :
		 p->width += change_amount;
		 p->move_resize(p);
		 client_deliver_config(p);
		 theme_img_cache_clear( w->mbtheme, FRAME_MAIN );
		 client_buttons_delete_all(p);
		 main_client_redraw(p, False); /* force title redraw */
		 break;
	       case MBCLIENT_TYPE_TOOLBAR :
	       case MBCLIENT_TYPE_PANEL   :
		 if (p->flags & CLIENT_DOCK_WEST)
		   break;
		 if (p->flags & CLIENT_DOCK_TITLEBAR)
		   {
		     /* 
			The usual configure call takes into account the
			just removed dock ( +ive change amount ), so 
			we have to basically do own configure() call 
			ignoring this. 
		     */

		     if (change_amount > 0)
		       {
			 XRectangle rect;

			 mbtheme_get_titlebar_panel_rect(p->wm->mbtheme, 
							 &rect, client_changed);
			 p->x      = rect.x + wm_get_offsets_size(p->wm, WEST, client_changed, True); 
			 p->width  = rect.width  ;
		       }
		     else
		       p->configure(p); 
		   }
		 else
		   {
		     p->width += change_amount;
		   }
		 p->move_resize(p);
		 client_deliver_config(p);

	       default:
		 break;
	       }
	   }
       }
     else if (client_changed->type == MBCLIENT_TYPE_PANEL 
	      && client_changed->flags & CLIENT_DOCK_NORTH)
       {
	 if (p->y >= client_changed->y) 
	   {
	     switch (p->type)
	       {
	       case MBCLIENT_TYPE_APP :
		 p->height += change_amount;
		 p->y      -= change_amount;
		 p->move_resize(p);
		 theme_img_cache_clear( w->mbtheme, FRAME_MAIN );
		 client_deliver_config(p);
		 main_client_redraw(p, False); /* force title redraw */
		 break;
	       case MBCLIENT_TYPE_PANEL :
		 if (p->flags & CLIENT_DOCK_NORTH
		     || p->flags & CLIENT_DOCK_TITLEBAR)
		   {
		     p->y -= change_amount;
		     p->move_resize(p);
		     client_deliver_config(p);
		   }
		 break;
	       default:
		 break;
	       }
	   }
       }
     else
       {
	 dbg("%s(): restack NORMAL comparing %i <= %i for %s\n",
	     __func__, p->y, client_changed->y, p->name);
	 if ( (p->y <= client_changed->y) 
	      || (client_changed->type == MBCLIENT_TYPE_PANEL 
		  && p->type == MBCLIENT_TYPE_TOOLBAR))
	   {
	     dbg("%s() restacking ( NORMAL )%s", __func__, p->name);
	     switch (p->type)
	       {
	       case MBCLIENT_TYPE_APP :
		 p->height += change_amount;
		 p->move_resize(p);
		 theme_img_cache_clear( w->mbtheme, FRAME_MAIN );
		 client_deliver_config(p);
		 main_client_redraw(p, False); /* force title redraw */
		 break;
	       case MBCLIENT_TYPE_TOOLBAR :
		 p->y += change_amount;
		 p->move_resize(p);
		 client_deliver_config(p);
		 break;
	       case MBCLIENT_TYPE_PANEL :
		 if (p->flags & CLIENT_DOCK_SOUTH)
		   {
		     p->y += change_amount;
		     p->move_resize(p);
		     client_deliver_config(p);
		   }
		 break;
	       case MBCLIENT_TYPE_DIALOG :
	       default:
		 break;
	       }
	   }
      }
   }


 /* Handle dialogs */

 stack_enumerate(w, p)
   {
     if (p->type == MBCLIENT_TYPE_DIALOG) 
       {
	 int req_x = p->x, req_y = p->y, req_w = p->width, req_h = p->height;

	 if (!dialog_check_geometry(p, &req_x, &req_y, &req_w, &req_h))
	   {
	     p->x = req_x; p->y = req_y; p->width = req_w; p->height = req_h;
	     p->move_resize(p);
	     client_deliver_config(p);
	   }
       }
   }

 XSync(w->dpy, False);
 XUngrabServer(w->dpy);

 ewmh_update_rects(w);
}

/* wm_activate_client() is called to 'activate', eg raise or show
 * a client stack wise.
 */
void 
wm_activate_client(Client *c)
{
  Wm     *w;
  Client *client_to_focus = c;

  if (c == NULL) return; /* its possible? for this to happen */

  w = c->wm;

  dbg("%s() called for %s\n", __func__, c->name);

  XGrabServer(w->dpy);

  c->show(c); /* Set 'relative' pos in stack, map windows
		 if needed etc                           */

  dbg("%s() DESKTOP_RAISED_FLAG is %i\n", 
      __func__, (w->flags & DESKTOP_RAISED_FLAG));

  if (c->type == MBCLIENT_TYPE_APP || c->type == MBCLIENT_TYPE_DESKTOP) 
    {
      /* As matchbox works around 'main' windows ( apps/main and desktop wins).
	 We need to sync extra stuff up when displaying a new one.
       */

      /* save focus state for transient dialogs of prev showing main win */

      if (w->stack_top_app && w->stack_top_app != c)
	w->stack_top_app->hide(w->stack_top_app); 

      /* If this client has a saved dialog focus state, load it */

      if (c->next_focused_client 
	  && c->next_focused_client->type == MBCLIENT_TYPE_DIALOG)
	client_to_focus = c->next_focused_client;

      /* If transient for root dialog currently has focus then keep it
         that way.                                                      */

      if (w->focused_client 
	  && w->focused_client->type == MBCLIENT_TYPE_DIALOG
	  && w->focused_client->trans == NULL)
	client_to_focus = w->focused_client;

      /* Raise panel + toolbars just above app but below app dialogs */

      if (!(c->flags & CLIENT_FULLSCREEN_FLAG))
	stack_move_type_above_client(MBCLIENT_TYPE_PANEL, c);

      stack_move_type_above_client(MBCLIENT_TYPE_TOOLBAR, c);

      /* Move transient dialogs to top */

      stack_move_transients_to_top(w, c, 0);

      /* Move transient for root dialogs to very top */

      stack_move_transients_to_top(w, NULL, 0);

      /* Urgent dialogs go above transient for root  */

      stack_move_transients_to_top(w, c, CLIENT_HAS_URGENCY_FLAG);

      /* Now move transient for root messages to top */

      stack_move_transients_to_top(w, NULL, CLIENT_HAS_URGENCY_FLAG);

      /* Deal with desktop flag etc */
      if (c->type != MBCLIENT_TYPE_DESKTOP)
	{
	  w->flags &= ~DESKTOP_RAISED_FLAG;
	  w->stack_top_app = c;      
	}
      else
	{
	  /* Desktop being activated */

	  w->flags |= DESKTOP_RAISED_FLAG;

	  /* Make sure embedded titlebar panels arn't visible for desktop */
	  if (w->have_titlebar_panel 
	      && mbtheme_has_titlebar_panel(w->mbtheme)
	      && !(w->have_titlebar_panel->flags & CLIENT_DOCK_TITLEBAR_SHOW_ON_DESKTOP))
	    {
	      stack_move_below_client(w->have_titlebar_panel, c);
	    }
	}

      /* We only set active for main clients - this could be wrong, 
         what about transient for root 'apps' - maybe it should just
         follow focused client ? */
      ewmh_set_active(w);

    }
  else if (c->type == MBCLIENT_TYPE_DIALOG)
    {
      /* A Little insurance - on mapping, a dialog can end up below 
       * panels and toolbars. May be a cleaner way than this.
       */

      if (!w->flags & DESKTOP_RAISED_FLAG)
	{

	  /*
	  if (c->trans)
	    {
	      Client *client_above = c->trans;

	      while (client_above->trans) 
		client_above = client_above->trans;

	      stack_move_type_below_client(MBCLIENT_TYPE_TOOLBAR
					   |MBCLIENT_TYPE_PANEL, client_above);
	    }
	  else 
	  */
	  if (wm_get_visible_main_client(w))
	    {
	      /* Move above main app win, therefore below dialogs */
	      stack_move_type_above_client(MBCLIENT_TYPE_TOOLBAR
					   |MBCLIENT_TYPE_PANEL, 
					   wm_get_visible_main_client(w));
	    }
	  else
	    stack_move_type_below_client(MBCLIENT_TYPE_TOOLBAR
					 |MBCLIENT_TYPE_PANEL, c);

	}
    }
  else if (c->type == MBCLIENT_TYPE_PANEL)
    {
      /* Make sure embedded titlebar panels arn't visible for desktop 
       */
      if (c == w->have_titlebar_panel 
	  && w->flags & DESKTOP_RAISED_FLAG
	  && mbtheme_has_titlebar_panel(w->mbtheme)
	  && !(w->have_titlebar_panel->flags & CLIENT_DOCK_TITLEBAR_SHOW_ON_DESKTOP))
	{
	  stack_move_below_client(c, w->client_desktop);
	}
    }
    
  ewmh_update(w);

  client_set_focus(client_to_focus); /* set focus if needed and ewmh active */

  stack_sync_to_display(w);

  comp_engine_client_show(w, c);

  XSync(w->dpy, False);	    

  XUngrabServer(w->dpy);
}

/* Returns either desktop or main app client */
Client*  
wm_get_visible_main_client(Wm *w)
{
  if (w->flags & DESKTOP_RAISED_FLAG)
    {
      dbg("%s() returning desktop - %p\n", __func__, wm_get_desktop(w)); 
      return wm_get_desktop(w);
    }

  if (w->stack_top_app) 
    {
      dbg("%s() returning stack top : %p\n", __func__, w->stack_top_app); 
      return w->stack_top_app;
    }
 
  dbg("%s() returning NULL\n", __func__); 

  return NULL;
}

/* Get area taken up on an edge by panels, toolbars */
int
wm_get_offsets_size(Wm*     w, 
		    int     wanted_direction,
		    Client* ignore_client, 
		    Bool    include_toolbars
		    )
{
  Client *p;
  int     x, y, width, height, result = 0;

  if (stack_empty(w)) return 0;

  dbg("%s() called\n", __func__);

  stack_enumerate(w, p)
     {
       if ((ignore_client && p == ignore_client) || p->mapped == False) 
	 continue;

       switch(wanted_direction)
	 {
	 case NORTH:
	   if (p->type == MBCLIENT_TYPE_PANEL && p->flags & CLIENT_DOCK_NORTH)
	     {
	       p->get_coverage(p, &x, &y, &width, &height);
	       result += height;
	     }
	   break;
	 case SOUTH:
	   if ((p->type == MBCLIENT_TYPE_PANEL && p->flags & CLIENT_DOCK_SOUTH)
	       || (p->type == MBCLIENT_TYPE_TOOLBAR && include_toolbars) )
	     {
	       p->get_coverage(p, &x, &y, &width, &height);
	       result += height;
	     }
	   break;
	 case EAST:
	   if (p->type == MBCLIENT_TYPE_PANEL && p->flags & CLIENT_DOCK_EAST)
	   {
	       p->get_coverage(p, &x, &y, &width, &height);
	       result += width;
	   }
	   break;
	 case WEST:
	   if (p->type == MBCLIENT_TYPE_PANEL && p->flags & CLIENT_DOCK_WEST)
	   {
	       p->get_coverage(p, &x, &y, &width, &height);
	       result += width;
	   }
	   break;
	 }
     }

   return result;
}


void
wm_toggle_desktop(Wm *w)
{
  dbg("%s() called desktop flag is : %i \n", __func__, 
      (w->flags & DESKTOP_RAISED_FLAG));

   if (!wm_get_desktop(w)) 
     {
       dbg("%s() couldn't find desktop \n", __func__ );
       return;
     }

   if (w->flags & DESKTOP_RAISED_FLAG)
     {
       dbg("%s() hiding desktop\n", __func__);
       wm_activate_client(w->stack_top_app);
     }
   else
     {
       dbg("%s() showing desktop\n", __func__);
       wm_activate_client(wm_get_desktop(w));

     }
}

void
wm_set_cursor_visibility(Wm *w, Bool visible)
{
  /* TODO: do we need to free the cursors ? */
  if (visible)
    {
      w->config->no_cursor = False;
      w->curs = XCreateFontCursor(w->dpy, XC_right_ptr);
    }
  else
    {
      Pixmap pix = XCreatePixmap (w->dpy, w->root, 1, 1, 1);
      XColor col;
      memset (&col, 0, sizeof (col));
      w->blank_curs = XCreatePixmapCursor (w->dpy, pix, pix, &col, &col, 1, 1);
      w->curs = w->blank_curs;
      XFreePixmap (w->dpy, pix);
      w->config->no_cursor = True;
    }     
   XDefineCursor(w->dpy, w->root, w->curs);
}

Client *
wm_get_desktop(Wm *w)
{
  return w->client_desktop;
}


#ifdef USE_XSETTINGS

#define XSET_UNKNOWN    0
#define XSET_THEME      1
#define XSET_CURSOR     2
#define XSET_LOWLIGHT   3
#define XSET_TITLEBARS  4
#define XSET_COMPOSITE  5

static void
wm_xsettings_notify_cb (const char       *name,
			XSettingsAction   action,
			XSettingsSetting *setting,
			void             *data)
{
  Wm *w = (Wm *)data;
  int i = 0;
  int key = XSET_UNKNOWN;
  
  struct _mb_xsettings { char *name; int value; } mb_xsettings[] = {
    { "Net/ThemeName",      XSET_THEME     },
    { "MATCHBOX/THEME",     XSET_THEME     },
    { "MATCHBOX/CURSOR",    XSET_CURSOR    },
    { "MATCHBOX/TITLEBARS", XSET_TITLEBARS }, /* XXX Not implemeted */
    { "MATCHBOX/COMPOSITE", XSET_COMPOSITE },
    { NULL,       -1 } 
  };

  while(  mb_xsettings[i].name != NULL )
    {
      if (!strcmp(name, mb_xsettings[i].name)
	  && setting != NULL 	/* XXX set to NULL when action deleted */
	  && setting->type == XSETTINGS_TYPE_STRING )
	{
	  key = mb_xsettings[i].value;
	  break;
	}
      i++;
    }
    
  if (key == XSET_UNKNOWN) return;

  switch (action)
    {
    case XSETTINGS_ACTION_NEW:
    case XSETTINGS_ACTION_CHANGED:
      switch (key)
	{
	case XSET_COMPOSITE:
	  if (!strcasecmp("off", setting->data.v_string)
	      || !strcasecmp("false", setting->data.v_string))
	    {
	      comp_engine_deinit(w);
	    }
	  else
	    { 
	      comp_engine_reinit(w);
	    }
	  break;
	case XSET_THEME:
	  if (w->flags & STARTUP_FLAG)
	      w->config->theme = strdup(setting->data.v_string);
	  else
	      mbtheme_switch(w, setting->data.v_string);
	  break;
	case XSET_CURSOR:
	  if (!strcasecmp("true", setting->data.v_string))
	    wm_set_cursor_visibility(w, True);
	  else 
	    wm_set_cursor_visibility(w, False);
	  break;
	case XSET_TITLEBARS:
	  /* XXX todo */
	  break;

	}
    case XSETTINGS_ACTION_DELETED:
      /* Do nothing for now */
      break;
    }
}

#endif

#ifdef USE_LIBSN

/* Various stuff for startup notification libs. 
 * Mb uses it for both busy startup cursor and single instancing apps.
 * Seems alot of code for just that ....
 */
static void 
wm_sn_exec(Wm *w, char* name, char* bin_name, char *desc)
{
  SnLauncherContext *context = NULL;
  pid_t child_pid = 0;

  context = sn_launcher_context_new (w->sn_display, DefaultScreen (w->dpy));
  
  if (name)     sn_launcher_context_set_name (context, name);
  if (desc)     sn_launcher_context_set_description (context, desc);
  if (bin_name) sn_launcher_context_set_binary_name (context, bin_name);
  
  sn_launcher_context_initiate (context, "Matchbox-kb-shortcut", bin_name,
				CurrentTime);

  switch ((child_pid = fork ()))
    {
    case -1:
      fprintf (stderr, "Fork failed\n" );
      break;
    case 0:
      sn_launcher_context_setup_child_process (context);
      execlp(bin_name, bin_name, NULL);
      fprintf (stderr, "Failed to exec %s \n", bin_name);
      _exit (1);
      break;
    }
  sn_launcher_context_unref (context);
}

static void 
wm_sn_timeout_check (Wm *w)
{
  time_t now ;

  dbg("%s() called\n", __func__);

  if (!w->sn_busy_cnt) return;
  
  now = time(NULL);
  if ((now - w->sn_init_time) > MB_SN_APP_TIMEOUT) 
    {
      w->sn_busy_cnt--;
      w->sn_init_time = time(NULL);
    }

  if (w->sn_busy_cnt)
    XDefineCursor(w->dpy, w->root, w->curs_busy);
  else
    {
      XDefineCursor(w->dpy, w->root, w->curs);
      XDeleteProperty(w->dpy, w->root, w->atoms[MB_CLIENT_STARTUP_LIST]);
    }
}

static void 
wm_sn_cycle_update_root_prop(Wm *w)
{
  SnCycle *current_cycle = w->sn_cycles;
  char *prop_str = NULL;
  int prop_str_len = 0;

  ewmh_update_lists(w);

  if (current_cycle == NULL)
    {
      XDeleteProperty(w->dpy, w->root, w->atoms[MB_CLIENT_STARTUP_LIST]);
      XFlush(w->dpy);
      return;
    }

  XGrabServer(w->dpy);

  while(current_cycle != NULL)
    {
	  dbg("%s() looping on %s, %li\n", __func__, 
	      current_cycle->bin_name, current_cycle->xid );

      if (current_cycle->xid == None)
	{
	  dbg("%s() adding %s, %li\n", __func__, 
	      current_cycle->bin_name, current_cycle->xid );
	  prop_str_len += (strlen(current_cycle->bin_name) + 1);
	}
      current_cycle = current_cycle->next;
    }

  if (prop_str_len > 1)
    {
      prop_str = malloc(sizeof(char)*(prop_str_len+1));
      memset(prop_str, 0, prop_str_len+1);

      current_cycle = w->sn_cycles;
      while(current_cycle != NULL)
	{
	  if (current_cycle->xid == None)
	    {
	      strcat(prop_str, current_cycle->bin_name);
	      strcat(prop_str, "|");
	    }
	  current_cycle = current_cycle->next;
	}
      
      dbg("%s() Setting MB_CLIENT_STARTUP_LIST to %s\n", __func__, prop_str);

      if (prop_str)
	{
	  XChangeProperty(w->dpy, w->root, w->atoms[MB_CLIENT_STARTUP_LIST] ,
			  XA_STRING, 8, PropModeReplace,
			  (unsigned char *)prop_str, strlen(prop_str)
			  );
	  
	  free(prop_str);
	}
    }
  else
    {
      dbg("%s() Deleting MB_CLIENT_STARTUP_LIST \n", __func__ );
      XDeleteProperty(w->dpy, w->root, w->atoms[MB_CLIENT_STARTUP_LIST]);
    }
  XFlush(w->dpy);

  XUngrabServer(w->dpy);
}

static SnCycle * 
wm_sn_cycle_new(Wm *w, const char *bin_name)
{
  SnCycle *new_cycle = malloc(sizeof(SnCycle));
  memset(new_cycle, 0, sizeof(SnCycle));
  new_cycle->bin_name = strdup(bin_name);
  new_cycle->xid      = None;
  new_cycle->next     = NULL;
  return new_cycle;
}

static void 
wm_sn_cycle_add(Wm *w, const char *bin_name)
{
  SnCycle *current_cycle;
  dbg("%s() called with %s\n", __func__, bin_name);
  if (w->sn_cycles == NULL)
    {
      w->sn_cycles = wm_sn_cycle_new(w, bin_name);
    }
  else
    {
      current_cycle = w->sn_cycles;
      if (!strcmp(current_cycle->bin_name, bin_name)
	  && current_cycle->xid == None)
	{
	  dbg("%s() already have %s\n", __func__, bin_name);
	  return; 		/* already have it */
	}

      while (current_cycle->next != NULL)
	{
	  if (!strcmp(current_cycle->bin_name, bin_name)
	      && current_cycle->xid == None )
	    {
	      dbg("%s() already have %s\n", __func__, bin_name);
	      return; 		/* already have it */
	    }
	  current_cycle = current_cycle->next;
	}
      current_cycle->next = wm_sn_cycle_new(w, bin_name);
    }
  wm_sn_cycle_update_root_prop(w);
}

void
wm_sn_cycle_remove(Wm *w, Window xid)
{
  SnCycle *current_cycle = w->sn_cycles, *prev_cycle = NULL;

  while(current_cycle != NULL)
    {
      if (current_cycle->xid == xid)
	{
	  if (current_cycle == w->sn_cycles)
	    {
	      w->sn_cycles = current_cycle->next; 
	      dbg("%s(): removed, w->sn_cycles is now %p\n", __func__, 
		  w->sn_cycles);
	    }
	  else
	    {
	      prev_cycle->next = current_cycle->next;
	    }
	  free(current_cycle->bin_name);
	  free(current_cycle);
	  wm_sn_cycle_update_root_prop(w);
	  return;
	}
      prev_cycle = current_cycle;
      current_cycle = current_cycle->next;
    }
  wm_sn_cycle_update_root_prop(w);
}

static void
wm_sn_cycle_update_xid(Wm *w, const char *bin_name, Window xid)
{
  /* find first where xid is None, and update */

  /* in above check, check another dont exist _without_ xid */

  /* destroy must call sn_cycle remove ? - unless theme switch flag is on */

  SnCycle *current_cycle = w->sn_cycles;

  dbg("%s() called with %s, %li\n", __func__, bin_name, xid);

  while(current_cycle != NULL)
    {
      if (!strcmp(current_cycle->bin_name, bin_name)
	  && current_cycle->xid == None)
	{
	  dbg("%s() got match for %s, setting xid = %li\n", 
	      __func__, bin_name, xid);
	  current_cycle->xid = xid;
	  wm_sn_cycle_update_root_prop(w);
	  return;
	}
      current_cycle = current_cycle->next;
    }
  dbg("%s() match failed\n", __func__);
  wm_sn_cycle_update_root_prop(w);
}

static void 
wm_sn_monitor_event_func (SnMonitorEvent *event,
			  void            *user_data)
{
  SnStartupSequence *sequence;
  Wm *w = (Wm *)user_data;
  const char *seq_id = NULL, *bin_name = NULL;
  Client *p;

  dbg("%s() called\n", __func__);

  sequence = sn_monitor_event_get_startup_sequence (event);

  if (sequence == NULL)
    {
      dbg("%s() failed, context / sequence is NULL\n", __func__);
      return;
    }

  seq_id   = sn_startup_sequence_get_id (sequence);
  bin_name = sn_startup_sequence_get_binary_name (sequence);

  if (seq_id == NULL || bin_name == NULL)
    {
      dbg("%s() failed, seq_id or bin_name NULL \n", __func__ );
      return;
    }

  switch (sn_monitor_event_get_type (event))
    {
    case SN_MONITOR_EVENT_INITIATED:
      dbg("%s() SN_MONITOR_EVENT_INITIATED\n", __func__);
      w->sn_busy_cnt++;
      w->sn_init_time = time(NULL);
      wm_sn_cycle_add(w, bin_name);
      break;
    case SN_MONITOR_EVENT_CHANGED:
      dbg("%s() SN_MONITOR_EVENT_CHANGED\n", __func__);
      break;
    case SN_MONITOR_EVENT_COMPLETED:
      dbg("%s() SN_MONITOR_EVENT_COMPLETED\n", __func__ );

      if (!stack_empty(w))
	{

	  stack_enumerate(w, p)
	    {
	      if (p->startup_id && !strcmp(p->startup_id, seq_id))
		{
		  dbg("%s() found startup_id match ( %s ) for %s \n", 
		      __func__, seq_id, p->name );
		  
		  wm_sn_cycle_update_xid(w, bin_name, p->window);
		  wm_sn_cycle_update_root_prop(w);
		  w->sn_busy_cnt--;
		  break;
		}
	    }
	}
      else w->sn_busy_cnt--;
      break;
    case SN_MONITOR_EVENT_CANCELED:
      /* wm_sn_cycle_remove(w, bin_name); */
      w->sn_busy_cnt--;
      break;
    }

  if (w->sn_busy_cnt)
    XDefineCursor(w->dpy, w->root, w->curs_busy);
  else
    XDefineCursor(w->dpy, w->root, w->curs);
}

#endif

/* Hacky way of dimming windows when no composite - not recommended */
#ifndef USE_COMPOSITE
void
wm_lowlight(Wm *w, Client *c)
{
#ifndef STANDALONE
  MBPixbufImage *img;
  int x, y;
  Pixmap pxm_tmp;
  XSetWindowAttributes attr;

  attr.override_redirect = True;
  attr.event_mask = ChildMask|ButtonPressMask|ExposureMask;
       
  c->frame = XCreateWindow(w->dpy, w->root, 0, 0,
			   w->dpy_width, w->dpy_height, 0,
			   CopyFromParent, 
			   CopyFromParent, 
			   CopyFromParent,
			   CWOverrideRedirect|CWEventMask,
			   &attr);

  pxm_tmp = XCreatePixmap(c->wm->dpy, c->window,  
			  w->dpy_width, 
			  w->dpy_height ,
			  w->pb->depth);
  
  img = mb_pixbuf_img_new_from_x_drawable(c->wm->pb, w->root, 
					  None, 0, 0,
					  w->dpy_width, 
					  w->dpy_height,
					  True);

  XMapWindow(w->dpy, c->frame);  

  for (x = 0; x < w->dpy_width; x++)
    for (y = 0; y < w->dpy_height; y++)
      mb_pixbuf_img_plot_pixel_with_alpha(c->wm->pb,
					  img, x, y, 
					  w->config->lowlight_params[0],
					  w->config->lowlight_params[1],
					  w->config->lowlight_params[2],
					  w->config->lowlight_params[3] 
					  );

  /* Striped pattern diabled. 
    if ( (y % 6) > 2 )
    { mb_pixbuf_img_composite_pixel(img, x, y, 0, 0, 0, 150); }
    else
    { mb_pixbuf_img_composite_pixel(img, x, y, 0, 0, 0, 100); }
  */

  mb_pixbuf_img_render_to_drawable(w->pb, img, pxm_tmp, 0, 0);
  
  XSetWindowBackgroundPixmap(w->dpy, c->frame, pxm_tmp);
  XClearWindow(w->dpy, c->frame);
  
  mb_pixbuf_img_free(w->pb, img);
  XFreePixmap(w->dpy, pxm_tmp);

#endif
}
#endif

#ifdef USE_GCONF
void
gconf_key_changed_callback (GConfClient *client,
			    guint        cnxn_id,
			    GConfEntry  *entry,
			    gpointer    user_data)
{
  Wm *w = (Wm *)user_data;

  GConfValue *value = NULL;
  char       *key   = NULL;

  dbg("%s() called\n", __func__ );

  value = gconf_entry_get_value(entry);
  key   = (char *)gconf_entry_get_key(entry);

  if (value && key)
    {
      dbg("%s() key is %s\n", __func__, key );

      switch (value->type)
	{
	case GCONF_VALUE_STRING:
	  dbg("%s() value is string : %s\n", __func__, 
	      gconf_value_get_string(value) );
	  /* On a keychange, we just reload the whole config :/ */
	  if (strstr(key, "keybindings"))
	    {
	      dbg("%s() calling keys_reinit\n", __func__ );
	      keys_reinit(w); 
	    }
	  else if (!strcmp(key, "/apps/matchbox/general/theme"))
	    {

#ifndef USE_XSETTINGS
	      char *theme 
		= gconf_client_get_string(w->gconf_client, 
					  "/apps/matchbox/general/theme", 
					  NULL);
	      if (w->flags & STARTUP_FLAG)
		w->config->theme = strdup(theme);
	      else
		mbtheme_switch(w, theme);
#else
	      if (w->xsettings_client == NULL)
		{
		  char *theme 
		    = gconf_client_get_string(w->gconf_client, 
					      "/apps/matchbox/general/theme", 
					      NULL);
		  if (w->flags & STARTUP_FLAG)
		    w->config->theme = strdup(theme);
		  else
		    mbtheme_switch(w, theme);
		}
#endif
	    }
	  break;
	case GCONF_VALUE_BOOL:
	  dbg("%s() value is boolean : %s\n", __func__, 
	      (gconf_value_get_bool(value)) ? "True" : "False" );
	  break;
	case GCONF_VALUE_INT:
	  dbg("%s() value is int : %i\n", __func__, 
	      gconf_value_get_int(value));
	  break;
	default :
	  dbg("%s() value is useless to me...\n", __func__ );
	}
    } 
}

#endif

