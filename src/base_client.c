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

/*
  $Id: base_client.c,v 1.1 2004/02/03 14:56:33 mallum Exp $
*/


#include "base_client.h"

Client* 
base_client_new(Wm *w, Window win)
{
   XWindowAttributes attr;

   long icccm_mask;
   Client *c = NULL;
   int i = 0, n = 0, format;
   XWMHints *wmhints = NULL;
   Atom *protocols   = NULL;
   XTextProperty  text_prop;

   Atom type;
   long bytes_after, n_items;
   long *data = NULL;

   dbg("%s() called  \n", __func__);


    
   c = malloc(sizeof(Client));
   memset(c, 0, sizeof(Client));
   
   c->type    = mainwin; /* start off with common case */
   c->buttons = NULL;
   c->name    = NULL;

   XGetWindowAttributes(w->dpy, win, &attr);
  
   c->window = win;
   c->frame       = None;
   c->title_frame = None;
   c->have_set_bg = False;

   c->wm = w;
   c->ignore_unmap = 0;
   
#if defined (USE_XFT) || defined (USE_PANGO)
   if ((c->name = ewmh_get_utf8_prop(w, win, w->atoms[_NET_WM_NAME])) != NULL)
     c->name_is_utf8 = True;
#endif    
   base_client_process_name(c);
   
   c->x = attr.x;
   c->y = attr.y;

   c->have_cache = False;
   
   c->cmap   = attr.colormap;
   c->visual = attr.visual;

   if (attr.win_gravity != NorthWestGravity)
     {
       XSetWindowAttributes set_attrs;

       set_attrs.win_gravity = NorthWestGravity;

       XChangeWindowAttributes (w->dpy,
				c->window,
				CWWinGravity,
				&set_attrs);
     }

   /* Get size hints */
   c->size = XAllocSizeHints();

   if ( !XGetWMNormalHints(w->dpy, c->window, c->size, &icccm_mask) )
   {
      c->width = attr.width;
      c->height = attr.height;
   } else {
      if (c->size->flags & PBaseSize) {
	 c->width  = c->size->base_width;
	 c->height = c->size->base_height;
       /* - tk windows set this to 1x1 which currently causes problems 
      } else if (c->size->flags &  PMinSize) {
	 c->width  = c->size->min_width;
	 c->height = c->size->min_height;
	 dbg("got min window size");
       */
      } else {
	 c->width = attr.width;
	 c->height = attr.height;
      }
   }

   if (c->x < 0) c->x = (w->dpy_width + c->x - c->width);
       
   if (c->y < 0) c->y = (w->dpy_height + c->y - c->height);

   if ((wmhints = XGetWMHints(w->dpy, c->window)) != NULL)
   {
      if (wmhints->window_group)
	 c->win_group = wmhints->window_group;
      else
	 c->win_group = 0;
   }
   dbg("%s() window group %li\n", __func__, c->win_group);

   c->startup_id = ewmh_get_utf8_prop(w, win, w->atoms[_NET_STARTUP_ID]);

   if (c->startup_id == NULL && c->win_group)
     c->startup_id = ewmh_get_utf8_prop(w, c->win_group, 
					w->atoms[_NET_STARTUP_ID]);

   c->bin_name = NULL;
     
   XFree(wmhints);

   /* Set up the 'methods' - expect to be overidden */
   base_client_set_funcs(c);

   /* Add the client to the circular list */
   if (w->head_client == NULL)
   {
      c->next = c;
      c->prev = c;
      w->head_client = c;
   }
   else
   {
      if (w->main_client)
      {
	 c->prev = w->main_client;
	 c->next = w->main_client->next;
      } else {
	 c->prev = w->head_client;
	 c->next = w->head_client->next;
       }	    
      c->prev->next = c;
      c->next->prev = c;
   }

#ifdef STANDALONE
   c->backing      = None;
#else
   c->backing      = NULL;
#endif

   for (i=0; i<MSK_COUNT; i++)
     c->backing_masks[i] = None;

#if defined (USE_XFT) || defined (USE_PANGO)
   c->xftdraw = NULL;
#endif

   c->flags     = 0;
   c->icon      = None;
   c->icon_mask = None;


#ifndef REDUCE_BLOAT
  c->icon_rgba_data = NULL;
#endif

  c->host_machine = NULL;
  if (XGetWMClientMachine(c->wm->dpy, c->window, &text_prop))
  {
    c->host_machine = strdup((char *) text_prop.value);
    XFree((char *) text_prop.value);
    dbg("%s() got host machine for ewmh : %s\n", __func__, c->host_machine);
  }
  
  c->pid = 0;
  if (XGetWindowProperty (w->dpy, win, 
			  w->atoms[_NET_WM_PID],
			  0, 2L,
			  False, XA_CARDINAL,
			  &type, &format, &n_items,
			  &bytes_after, (unsigned char **)&data) == Success
      && n_items && data != NULL)
    {
      c->pid = *data;
      dbg("%s() got ewmh pid : %i\n", __func__, c->pid);
    }
  else
    {
      dbg("%s() got ewmh pid : FAILED\n", __func__ );
    }

  if (data) XFree(data);

  c->has_ping_protocol = False;


  /* We detect any errors here to check the window hasn't dissapeared half
   * way through. A bit hacky ...
   */
   misc_trap_xerrors();

   /* Check for 'special' extra button/ping protocols */
   if (XGetWMProtocols(c->wm->dpy, c->window, &protocols, &n)) 
     {
       for (i=0; i<n; i++)
	 {
	   if (protocols[i] == c->wm->atoms[_NET_WM_CONTEXT_HELP])
	     {
	       dbg("%s() got _NET_WM_CONTEXT_HELP protocol\n", __func__ );
	       c->flags |= CLIENT_HELP_BUTTON_FLAG;
	     }
	   else if (protocols[i] == c->wm->atoms[_NET_WM_CONTEXT_ACCEPT])
	     {
	       dbg("%s() got _NET_WM_CONTEXT_ACCEPT protocol\n", __func__ );
	       c->flags |= CLIENT_ACCEPT_BUTTON_FLAG;
	     }
	   else if (protocols[i] == c->wm->atoms[_NET_WM_CONTEXT_CUSTOM])
	     {
	       dbg("%s() got _NET_WM_CONTEXT_CUSTOM protocol\n", __func__ );
	       c->flags |= CLIENT_CUSTOM_BUTTON_FLAG;
	     }
	   else if (protocols[i] == c->wm->atoms[_NET_WM_PING]
		    && c->host_machine && c->pid)
	     {
	       dbg("%s() has PING ewmh\n", __func__);
	       c->has_ping_protocol = True;
	     }
	 }
       XFree(protocols);
    }


   c->pings_pending = 0;

   client_set_state(c, WithdrawnState);

   if (misc_untrap_xerrors()) 	/* An X error occured */
     {				/* Likely client died */
       c->frame = None;
       base_client_destroy(c);
       return NULL;
     }

   return c;
}

void 
base_client_process_name(Client *c)
{
  Wm *w = c->wm;
  XTextProperty text_prop;
  Client *p; int i, max = 0; char *tmp_name = NULL;

   dbg("%s() called, name is %s\n", __func__, c->name);

  if (c->name == NULL)
    {
      c->name_is_utf8 = False;
      if (XGetWMName(w->dpy, c->window, &text_prop) != 0)
	{
	  dbg("%s() name is from XGetWMName\n", __func__ );
	  c->name = strdup((char *) text_prop.value);
	  XFree((char *) text_prop.value);
	}
      else
	{
	  XFetchName(w->dpy, c->window, (char **)&c->name);
	  if (c->name == NULL) 
	    {
	      XStoreName(w->dpy, c->window, "<unnamed>");
	      XFetchName(w->dpy, c->window, (char **) &c->name);
	      if (c->name == NULL) 
		{
		  /* something is seriously wrong if we get here */
		  dbg("%s() WARNING, name is still null after store/fetch\n",
		      __func__ );
		  return;
		}
	    }
	}
    }
  
  /* If window name already exists, rename, adding <%i> to it */
  if (w->head_client)
    {
      START_CLIENT_LOOP(w,p);
      
      if (strncmp(p->name, c->name, strlen(c->name)) == 0
	  && p != c)
	{
	  if (strcmp(p->name, c->name) == 0)
	    {
	      if (!max) max = 1;
	    } else {
	      i = atoi(p->name+strlen(c->name)+2);
	      if (i > max) max = i;
	    }
	}
      END_CLIENT_LOOP(w,p);
      
      if (max)
	{
	  tmp_name = alloca(sizeof(char) * (strlen(c->name) + 7));
	  sprintf(tmp_name, "%s <%i>", c->name, ++max);
	  free(c->name);

	  XStoreName(w->dpy, c->window, tmp_name);
	  XFetchName(w->dpy, c->window, (char **)&c->name);
	}
    }

   dbg("%s() end, name is now %s\n", __func__, c->name);
}

void
base_client_set_funcs(Client *c)
{
   c->configure    = &base_client_configure;
   c->reparent     = &base_client_reparent;
   c->redraw       = &base_client_redraw;
   c->button_press = &base_client_button_press;
   c->get_coverage = &base_client_get_coverage;
   c->move_resize  = &base_client_move_resize;
   c->hide         = &base_client_hide;
   c->show         = &base_client_show;
   c->destroy      = &base_client_destroy;
   c->iconize      = &base_client_iconize;
}

/* This will set the window attributes to what _we_ want */
void
base_client_configure(Client *c)
{
   ;
}

/* Frame the window if needed */
void
base_client_reparent(Client *c)
{
   ;
}


/* redraw the clients frame */
void
base_client_redraw(Client *c, Bool use_cache)
{
   ;
}


/* button press on frame */
void
base_client_button_press(Client *c, XButtonEvent *e)
{
   ;
}

void
base_client_iconize(Client *c)
{
  client_set_state(c,IconicState);
  c->hide(c);
}


/* move and resize the window */
void
base_client_move_resize(Client *c)
{
  int i;

#ifdef STANDALONE
   if (c->backing != None)
     {
       XFreePixmap(c->wm->dpy, c->backing);
       c->backing = None;
     }
#else
   if (c->backing != NULL)
     {
       mb_drawable_unref(c->backing);
       c->backing = NULL;
     }
#endif

   for (i=0; i<MSK_COUNT; i++)
     if (c->backing_masks[i] != None)
       {
	 XFreePixmap(c->wm->dpy, c->backing_masks[i]);
	 c->backing_masks[i] = None;
       }
}


/* return the 'area' covered by the window. Including the frame
   Would return 0 for an unmapped window
*/
void
base_client_get_coverage(Client *c, int *x, int *y, int *w, int *h)
{
  *x = c->x; *y = c->y;*w = c->width;*h = c->height;   
}

void
base_client_hide_transients(Client *c)
{
   Client *t;
   if (c == NULL) return;

   for(t = c->prev; t != c; t = t->prev)
     if (t->type == dialog && t->trans == c && t->mapped)
       t->hide(t);

   comp_engine_client_hide(c->wm, c);
}


void
base_client_hide(Client *c)
{
  base_client_hide_transients(c);

  comp_engine_client_hide(c->wm, c);
}

void
base_client_show(Client *c)
{

   Client *t, *client_msg = NULL;

   for(t = c->prev; t != c; t = t->prev)
   {
      switch (t->type)
      {
	 case dialog:    /* raise any transients */
	    dbg("%s() Transient found, name: %s mapped: %i\n", __func__, t->name, t->mapped);
	    if ((t->trans == c || t->trans == NULL) && t->mapped)
	    {
	       dbg("%s() raising transient %s\n", __func__, t->name);
	       t->show(t);
#ifdef USE_MSG_WIN
	       if (t->flags & CLIENT_IS_MESSAGE_DIALOG)
		 {
		   client_msg = t;
		 }
#endif
	    }
	    /* raise any toolbar transients */
	    else if (c->type != toolbar
		     && t->trans != NULL
		     && t->trans->type == toolbar
		     && client_get_state(t->trans) == NormalState)
	       t->show(t);
	    break;
      default:
	    break;
      }
   }

   /* Make sure message windows are _really_ on top */
   if (client_msg) client_msg->show(client_msg);

   comp_engine_client_show(c->wm, c);

   ewmh_set_active(c->wm);
}

void /* cb for this needed, or let wm handle it */
base_client_destroy(Client *c)
{
  int i = 0;
   /* Free its memory + remove from list */

   Client *t = NULL, *prev_client = NULL;
   dbg("%s() called\n", __func__);

   /* destroy any transients */
   for(t = c->prev; t != c; t = prev_client)
     {
       prev_client = t->prev;
       if (t->type == dialog && t->trans == c)
	 t->destroy(t);
     }

#ifdef USE_LIBSN
   wm_sn_cycle_remove(c->wm, c->window);
#endif       

   /* remove from circular list */
   if (c->prev != c)
     {
       if (c->wm->head_client == c) 
	 c->wm->head_client = c->next;
       if (c->prev != NULL) c->prev->next = c->next;
       if (c->next != NULL) c->next->prev = c->prev;
    } else {
      c->wm->head_client = NULL;
      if (c->type == mainwin) c->wm->main_client = NULL;
    }

   if (c->type != MBCLIENT_TYPE_OVERRIDE)
     {
       client_buttons_delete_all(c);
       
       ewmh_update(c->wm);
   
#if defined (USE_XFT) || defined (USE_PANGO)
       if (c->xftdraw != NULL) XftDrawDestroy(c->xftdraw);
#endif

       if (c->frame && c->frame != c->window) 
	 XDestroyWindow(c->wm->dpy, c->frame);
       if (c->title_frame != c->window 
	   && c->title_frame != c->frame
	   && c->title_frame != None) 
	 XDestroyWindow(c->wm->dpy, c->title_frame);

#ifdef STANDALONE
       if (c->backing != None)
	   XFreePixmap(c->wm->dpy, c->backing);
#else
       if (c->backing != NULL)
	   mb_drawable_unref(c->backing);
#endif

       for (i=0; i<MSK_COUNT; i++)
	 if (c->backing_masks[i] != None)
	   XFreePixmap(c->wm->dpy, c->backing_masks[i]);
       
       if (c->icon != None && c->icon != c->wm->generic_icon)
	 XFreePixmap(c->wm->dpy, c->icon);
       if (c->icon_mask != None && c->icon_mask != c->wm->generic_icon_mask)
	 XFreePixmap(c->wm->dpy, c->icon_mask);
       
#ifndef REDUCE_BLOAT
       if (c->icon_rgba_data) XFree(c->icon_rgba_data);
#endif
     }    

    if (c->name) XFree(c->name);
    if (c->startup_id) XFree(c->startup_id);
    if (c->size) XFree(c->size);
    if (c->host_machine) free(c->host_machine);

    comp_engine_client_hide(c->wm, c);
    comp_engine_client_destroy(c->wm, c);


    free(c);
}



