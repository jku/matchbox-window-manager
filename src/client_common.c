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

#include "client_common.h"

/*
   Common functions for use by all client types
*/

void
client_set_state(Client *c, int state)
{
   CARD32 data[2];

   data[0] = state;
   data[1] = None;
   
   XChangeProperty(c->wm->dpy, c->window, 
		   c->wm->atoms[WM_STATE], c->wm->atoms[WM_STATE],
		   32, PropModeReplace, (unsigned char *)data, 2);
}

long
client_get_state(Client *c)
{
    Atom real_type; int real_format;
    unsigned long items_read, items_left;
    long *data = NULL, state = WithdrawnState;

    if (XGetWindowProperty(c->wm->dpy, c->window,
			   c->wm->atoms[WM_STATE], 0L, 2L, False,
			   c->wm->atoms[WM_STATE], &real_type, &real_format,
			   &items_read, &items_left,
			   (unsigned char **) &data) == Success
	&& items_read) {
      state = *data;
    }

    if (data)
      XFree(data);

    return state;
}


void
client_deliver_config(Client *c)
{
   XConfigureEvent ce;
   
   ce.type = ConfigureNotify;
   ce.event = c->window;
   ce.window = c->window;
   
   ce.x = c->x;
   ce.y = c->y;
   
   ce.width  = c->width;
   ce.height = c->height;
   ce.border_width = 0;
   ce.above = None;
   ce.override_redirect = 0;
   
   dbg("%s() to %s  x: %i , y: %i w: %i h: %i \n", 
       __func__, c->name, ce.x, ce.y, ce.width, ce.height);

   XSendEvent(c->wm->dpy, c->window, False,
	      StructureNotifyMask, (XEvent *)&ce);
}

void
client_deliver_wm_protocol(Client *c, Atom delivery)
{
  /* TODO: should call client_deliver_message() */
    XEvent e;
    e.type = ClientMessage;
    e.xclient.window = c->window;
    e.xclient.message_type = c->wm->atoms[WM_PROTOCOLS];
    e.xclient.format = 32;
    e.xclient.data.l[0] = delivery;
    e.xclient.data.l[1] = CurrentTime;
    XSendEvent(c->wm->dpy, c->window, False, 0, &e);
}

void
client_deliver_message(Client       *c, 
		       Atom          delivery_atom,
		       unsigned long data1,
		       unsigned long data2,
		       unsigned long data3,
		       unsigned long data4)
{
  XEvent ev;
  Wm *w = c->wm;

  memset(&ev, 0, sizeof(ev));

  ev.xclient.type = ClientMessage;
  ev.xclient.window = c->window;
  ev.xclient.message_type = delivery_atom;
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = CurrentTime;
  ev.xclient.data.l[1] = data1;
  ev.xclient.data.l[2] = data2;
  ev.xclient.data.l[3] = data3;
  ev.xclient.data.l[4] = data4;
  XSendEvent(w->dpy, c->window, False, NoEventMask, &ev);
  XSync(w->dpy, False);
}


/* 'Really' kill an app if it gives us enough info */
Bool
client_obliterate(Client *c)
{
  char buf[257];
  int sig  = 9;

  if (c->host_machine == NULL || !c->pid)
    return False;

  if (gethostname (buf, sizeof(buf)-1) == 0)
    {
      if (!strcmp (buf, c->host_machine))
	{
	  if (kill (c->pid, sig) < 0)
	    {
	      fprintf(stderr, "matchbox: kill %i on %s failed.\n",
		      sig, c->name);
	      return False;
	    }
	}
      else return False; 	/* on a different host */
    }
  else 
    {
      fprintf(stderr, "matchbox: gethostname failed\n");
      return False;
    }

  return True;
}

void
client_deliver_delete(Client *c)
{
    int i, n, found = 0;
    Atom *protocols;
    
    if (XGetWMProtocols(c->wm->dpy, c->window, &protocols, &n)) {
        for (i=0; i<n; i++)
	   if (protocols[i] == c->wm->atoms[WM_DELETE_WINDOW]) found++;
        XFree(protocols);
    }

    /* Initiate pinging the app - to really kill hung applications */
    if (c->has_ping_protocol && c->pings_pending == -1) 
      {
	c->pings_pending = 0;
	c->wm->n_active_ping_clients++;
      }

    if (found)
      client_deliver_wm_protocol(c, c->wm->atoms[WM_DELETE_WINDOW]);
    else 
      {
	if (!client_obliterate(c))
	  XKillClient(c->wm->dpy, c->window);
      }
}

/* ----- new bits ---------*/

int
client_want_focus(Client *c)
{
   /* TODO: check _WM protocols too ? */
   int ret = 1;
   XWMHints *hints = XGetWMHints(c->wm->dpy, c->window);
   if (hints != NULL)
     if ((hints->flags & InputHint) && (hints->input == False)) ret = 0;
   XFree(hints);

   if (ret) ewmh_set_active(c->wm);
   return ret;
}

Client*
client_get_next(Client* c, MBClientTypeEnum wanted)
{
   Client *p;
   for (p=c->next; p != c; p = p->next)
      if (p->type == wanted && p->mapped) return p;
   return c;
}

Client*
client_get_prev(Client* c, MBClientTypeEnum wanted)
{
   Client *p;
   for (p=c->prev; p != c; p = p->prev)
      if (p->type == wanted && p->mapped) return p;
   return c;
}

void
client_init_backing(Client* c, int width, int height)
{

#ifdef STANDALONE

  if (c->backing != None) XFreePixmap(c->wm->dpy, c->backing);

  c->backing = XCreatePixmap(c->wm->dpy, c->wm->root, width, height ,
			     DefaultDepth(c->wm->dpy, c->wm->screen));

   /* todo check for error if pixmap cant be created */
#if defined (USE_XFT) 

  if (c->xftdraw != NULL) XftDrawDestroy(c->xftdraw);

   c->xftdraw = XftDrawCreate(c->wm->dpy, (Drawable) c->backing, 
			      DefaultVisual(c->wm->dpy, c->wm->screen),
			      DefaultColormap(c->wm->dpy, c->wm->screen));
#endif


#else  /* using libmb for font rendering */

   if (c->backing != NULL) 
     mb_drawable_unref(c->backing);

#ifdef USE_COMPOSITE
   if (c->is_argb32)
     {
       c->backing = mb_drawable_new(c->wm->argb_pb, width, height);
       dbg("%s() XXXXX creating 32bit drawable ( %i, %ix%i ) XXXX\n", 
	   __func__, c->wm->argb_pb->depth, width, height);


     }
   else
#endif
     c->backing = mb_drawable_new(c->wm->pb, width, height);

#endif
   
}

void 
client_init_backing_mask (Client *c, int width, int height, 
			  int height_north, int height_south,
			  int width_east, int width_west )
{
  GC shape_gc;
  int i = 0;

   for (i=0; i<MSK_COUNT; i++)
     if (c->backing_masks[i] != None)
       XFreePixmap(c->wm->dpy, c->backing_masks[i]);

  c->backing_masks[MSK_NORTH] 
    = XCreatePixmap(c->wm->dpy, c->wm->root, width, height_north, 1);

  shape_gc = XCreateGC( c->wm->dpy, c->backing_masks[MSK_NORTH], 0, 0 );

  XSetForeground(c->wm->dpy, shape_gc, 
		 WhitePixel( c->wm->dpy, c->wm->screen ));

  XFillRectangle(c->wm->dpy, c->backing_masks[MSK_NORTH],
		 shape_gc, 0, 0, width, height_north);

  if (height_south)
    {
      c->backing_masks[MSK_SOUTH] 
	= XCreatePixmap(c->wm->dpy, c->wm->root, width, height_south, 1);

      XFillRectangle(c->wm->dpy, c->backing_masks[MSK_SOUTH],
		     shape_gc, 0, 0, width, height_south);
    }

  if (width_east)
    {
      dbg("%s() creating backing mask with %ix%i\n", 
	  __func__, width_east, height );

      c->backing_masks[MSK_EAST] 
	= XCreatePixmap(c->wm->dpy, c->wm->root, width_east, height, 1);

      XFillRectangle(c->wm->dpy, c->backing_masks[MSK_EAST],
		     shape_gc, 0, 0, width_east, height);
    }

  if (width_west)
    {
      c->backing_masks[MSK_WEST] 
	= XCreatePixmap(c->wm->dpy, c->wm->root, width_west, height, 1);

      XFillRectangle(c->wm->dpy, c->backing_masks[MSK_WEST],
		     shape_gc, 0, 0, width_west, height);
    }


  XFreeGC(c->wm->dpy, shape_gc);
}


MBClientButton *
client_button_new(Client *c, Window win_parent, 
		  int x, int y, int w, int h,
		  Bool want_inputonly, void *data )
{
  XSetWindowAttributes attr;
  int class = CopyFromParent;

  MBClientButton *b = malloc(sizeof(MBClientButton));
  memset(b, 0, sizeof(MBClientButton));
  
  attr.override_redirect = True; 
  attr.event_mask = ExposureMask;
  
  if (want_inputonly ) class = InputOnly;	      
  
  b->x = x; b->y = y; b->w = w; b->h = h; b->data = data;
  
  b->win = XCreateWindow(c->wm->dpy, win_parent, x, y, w, h, 0,
			 CopyFromParent, class, CopyFromParent,
			 CWOverrideRedirect|CWEventMask, &attr);

  XMapWindow(c->wm->dpy, b->win);
  return b;
}

void
client_button_remove(Client *c, int button_action)
{
  struct list_item *l = c->buttons;
  MBClientButton *b = NULL;

  while (l != NULL)
    {
      if (l->id == button_action)
	{
	  b = (MBClientButton *)l->data;
	  dbg("%s() destroying a button ( %li ) for %s\n", __func__, 
	      b->win, c->name); 
	  XDestroyWindow(c->wm->dpy, b->win);
	  b->win = None;
	}
      l = l->next;
    }
}

void
client_buttons_delete_all(Client *c)
{
  struct list_item *l = c->buttons, *p = NULL;
  MBClientButton *b = NULL;
  
  while (l != NULL)
    {
      b = (MBClientButton *)l->data;
      dbg("%s() destroying a button\n", __func__); 
      if (b->win != None)
	XDestroyWindow(c->wm->dpy, b->win);
      free(b);
      p = l->next;
      free(l);
      l = p;
    }
  c->buttons = NULL;
}

MBClientButton *
client_get_button_from_event(Client *c, XButtonEvent *e)
{
  struct list_item *l = c->buttons;
  MBClientButton *b = NULL;

  while (l != NULL)
    {
      b = (MBClientButton *)l->data;
      if (b->win == e->subwindow)
	{
	  return b;
	}
      l = l->next;
    }
  return NULL;
}


struct list_item *
client_get_button_list_item_from_event(Client *c, XButtonEvent *e)
{
  struct list_item *l = c->buttons;
  MBClientButton *b = NULL;

  while (l != NULL)
    {
      b = (MBClientButton *)l->data;
      if (b->win == e->subwindow)
	{
	  return l;
	}
      l = l->next;
    }
  return NULL;
}

int
client_button_do_ops(Client *c, XButtonEvent *e, int frame_type, int w, int h)
{
  int button_action;
  struct list_item* button_item = NULL;
  MBClientButton *b = NULL;
  XEvent ev;

  if ((button_item = client_get_button_list_item_from_event(c, e)) != NULL
       && button_item->id != -1 )
   {
     /* XXX hack hack hack - stop dubious 'invisible' text menu button 
                             working when it shouldn't.....            */
     if (frame_type == FRAME_MAIN && c->wm->flags & SINGLE_FLAG
	 && button_item->id == BUTTON_ACTION_MENU
	 && ( !wm_get_desktop(c->wm) || c->wm->flags & DESKTOP_DECOR_FLAG))
       return -1;

     b = (MBClientButton *)button_item->data;

     if (b->press_activates)
       {
	 XUngrabPointer(c->wm->dpy, CurrentTime); 
	 client_deliver_message(c, c->wm->atoms[MB_GRAB_TRANSFER],
				e->subwindow, 0, 0, 0);
	 return button_item->id;
       }

     if (XGrabPointer(c->wm->dpy, e->subwindow, False,
		      ButtonPressMask|ButtonReleaseMask|
		      PointerMotionMask|EnterWindowMask|LeaveWindowMask,
		      GrabModeAsync,
		      GrabModeAsync, None, c->wm->curs, CurrentTime)
	 == GrabSuccess)
       {
	 Bool canceled = False;
	 button_action = button_item->id;

	 theme_frame_button_paint(c->wm->mbtheme, c, button_action,
				  ACTIVE, frame_type, w, h);

	 comp_engine_client_repair (c->wm, c);
	 comp_engine_render(c->wm, c->wm->all_damage);

	 for (;;) 
	 {
	    XMaskEvent(c->wm->dpy,
		       ButtonPressMask|ButtonReleaseMask|
		       PointerMotionMask|EnterWindowMask|LeaveWindowMask
		       ,
		    &ev);
	    switch (ev.type)
	    {
	       case MotionNotify:
		  break;
	       case EnterNotify:
		  theme_frame_button_paint(c->wm->mbtheme, c, button_action,
					   ACTIVE, frame_type, w, h );
		  comp_engine_client_repair (c->wm, c);
		  comp_engine_render(c->wm, c->wm->all_damage);
		  canceled = False;
		  break;
	       case LeaveNotify:
		  theme_frame_button_paint(c->wm->mbtheme, c, button_action,
					   INACTIVE, frame_type, w, h );
		  comp_engine_client_repair (c->wm, c);
		  comp_engine_render(c->wm, c->wm->all_damage);
		  canceled = True;
		  break;
	       case ButtonRelease:
		  theme_frame_button_paint(c->wm->mbtheme, c, button_action,
					   INACTIVE, frame_type, w, h );
		  XUngrabPointer(c->wm->dpy, CurrentTime);
		  if (!canceled)
		  {
		    return button_action;
#if 0		    
		     if (frm->buttons[b]->wants_dbl_click)
		     {
			if (c->wm->flags & DBL_CLICK_FLAG)
			  {
			    if ( b == ACTION_MENU_EXTRA) /* HACK */
			      b = ACTION_MENU;
			    else if (b == ACTION_MAX_EXTRA)
			      b = ACTION_MAX;
			    else if (b == ACTION_MIN_EXTRA)
			      b = ACTION_MIN;
			    return b;
			  }
			else
			   return -1;
		     } else {
		       if ( b == ACTION_MENU_EXTRA) /* HACK */
			 b = ACTION_MENU;
		       else if (b == ACTION_MAX_EXTRA)
			 b = ACTION_MAX;
		       else if (b == ACTION_MIN_EXTRA)
			 b = ACTION_MIN;
		       return b;
		     }
#endif		     
		  }
		  else
		     return -1;  /* cancelled  */
	    }

#ifdef USE_COMPOSITE
      if (c->wm->all_damage)
      	{
	  dbg("%s() adding damage\n", __func__);
	  comp_engine_render(c->wm, c->wm->all_damage);
	  XFixesDestroyRegion (c->wm->dpy, c->wm->all_damage);
	  c->wm->all_damage = None;
	}
#endif

	 }
      }
   }
   return 0;
}


