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

#include "toolbar_client.h"

Client*
toolbar_client_new(Wm *w, Window win)
{
   Client *c = base_client_new(w, win); 

   if (!c) return NULL;

   /* If theres not room for the toolbar, just make it an app window */
   /* In practise one would expect this to never happen              */
   if ((c->y + c->height) > w->dpy_height - (theme_frame_defined_height_get(w->mbtheme, FRAME_MAIN)*2) )
     {
       base_client_destroy(c);
       return main_client_new(w, win);
     }

   c->type = MBCLIENT_TYPE_TOOLBAR;
   
   c->configure    = &toolbar_client_configure;
   c->reparent     = &toolbar_client_reparent;
   c->button_press = &toolbar_client_button_press;
   c->redraw       = &toolbar_client_redraw;
   c->hide         = &toolbar_client_hide;
   c->iconize      = &toolbar_client_hide;
   c->show         = &toolbar_client_show;
   c->move_resize  = &toolbar_client_move_resize;
   c->get_coverage = &toolbar_client_get_coverage;
   c->destroy      = &toolbar_client_destroy;

   client_set_state(c,WithdrawnState); 	/* set initially to signal show() */
   
   return c;
}

void
toolbar_client_configure(Client *c)
{
  Wm *w = c->wm;

  if (c->flags & CLIENT_IS_MINIMIZED)
    return;

  c->y = w->dpy_height - wm_get_offsets_size(w, SOUTH, c, True)
    - c->height;
  c->x = toolbar_win_offset(c) 
    + wm_get_offsets_size(w, WEST,  NULL, False);
  c->width = w->dpy_width - toolbar_win_offset(c)
    - wm_get_offsets_size(w, WEST,  NULL, False)
    - wm_get_offsets_size(w, EAST,  NULL, False);

}

void
toolbar_client_move_resize(Client *c)
{
  Wm *w = c->wm;

  int max_offset = theme_frame_defined_width_get(w->mbtheme, 
						 FRAME_UTILITY_MAX);
  int min_offset = theme_frame_defined_height_get(w->mbtheme, 
						  FRAME_UTILITY_MIN);
  int offset = toolbar_win_offset(c);
   
  base_client_move_resize(c);

  if (!(c->flags & CLIENT_IS_MINIMIZED))
    {
      if (c->flags & CLIENT_TITLE_HIDDEN_FLAG) max_offset = 0;
      
      XResizeWindow(w->dpy, c->window, c->width, c->height);
      XMoveResizeWindow(w->dpy, c->frame, c->x - max_offset,
		  c->y, c->width + max_offset, c->height );
      XMoveResizeWindow(w->dpy, c->title_frame, 0,
		  0, max_offset , c->height );
   } else {

     if (min_offset)
       {
	 XMoveResizeWindow(w->dpy, c->frame, c->x,
			   c->y, c->width + max_offset, offset );
	 XMoveResizeWindow(w->dpy, c->title_frame, 0,
			   0, c->width + max_offset , min_offset );
       }
   }      
}

void
toolbar_client_reparent(Client *c)
{
    XSetWindowAttributes attr;

    int frm_size = theme_frame_defined_width_get(c->wm->mbtheme, 
						 FRAME_UTILITY_MAX );   
    attr.override_redirect = True; 
    attr.background_pixel  = BlackPixel(c->wm->dpy, c->wm->screen);
    attr.event_mask        = ChildMask|ButtonPressMask|ExposureMask;
    
    c->frame =
       XCreateWindow(c->wm->dpy, c->wm->root, 0, c->y,
		     c->wm->dpy_width, c->height, 0,
		     CopyFromParent, CopyFromParent, CopyFromParent,
		     CWOverrideRedirect|CWEventMask|CWBackPixel,
		     &attr);
   
   attr.background_pixel = BlackPixel(c->wm->dpy, c->wm->screen);
   
   c->title_frame =
      XCreateWindow(c->wm->dpy, c->frame, 0, 0, frm_size, c->height, 0,
		    CopyFromParent, CopyFromParent, CopyFromParent,
		    CWOverrideRedirect|CWBackPixel|CWEventMask, &attr);

   XSetWindowBorderWidth(c->wm->dpy, c->window, 0);
   XAddToSaveSet(c->wm->dpy, c->window);
   XSelectInput(c->wm->dpy, c->window,
		ButtonPressMask|ColormapChangeMask|PropertyChangeMask);

   dbg("%s() reparenting at %i\n", __func__, toolbar_win_offset(c));
   
   XReparentWindow(c->wm->dpy, c->window, c->frame,
		   toolbar_win_offset(c), 0);
}


void
toolbar_client_show(Client *c)
{
  Wm   *w = c->wm;
  long win_state; 

#ifdef STANDALONE
   XFreePixmap(w->dpy, c->backing);
   c->backing = None;
#else
   if (c->backing != NULL) 
     mb_drawable_unref(c->backing);
   c->backing = NULL;
#endif

   c->mapped = True;

  if (w->main_client  		/* XXX NEEDED ANYMORE ? */
      && (w->main_client->flags & CLIENT_FULLSCREEN_FLAG))
    main_client_manage_toolbars_for_fullscreen(c, True);

   win_state = client_get_state(c);

   /* initially mapped */
   if (win_state == WithdrawnState)
     {
       client_set_state(c,NormalState);
       wm_update_layout(c->wm, c, - c->height);
     } 
   else if (win_state == IconicState) /* minimised, set maximised */
     {
       client_set_state(c,NormalState);

       /* Make sure desktop flag is unset */
       c->flags &= ~CLIENT_IS_MINIMIZED;

       wm_update_layout(c->wm, c, -(c->height - toolbar_win_offset(c)));

       c->y = c->y - ( c->height - toolbar_win_offset(c));
       if (c->flags & CLIENT_TITLE_HIDDEN_FLAG)
	 c->x = wm_get_offsets_size(c->wm, WEST,  NULL, False);
       else
	 c->x = theme_frame_defined_width_get(w->mbtheme,
					      FRAME_UTILITY_MAX )
	   + wm_get_offsets_size(c->wm, WEST,  NULL, False);
     } 
   else return; /* were already shown */ 

   /* destroy any eisting buttons */
   client_buttons_delete_all(c);   

   toolbar_client_move_resize(c);

   stack_move_above_extended(c, NULL, 
			     MBCLIENT_TYPE_APP|MBCLIENT_TYPE_DESKTOP, 
			     0);

   XMapSubwindows(w->dpy, c->frame);
   XMapWindow(w->dpy, c->frame);

   if (client_want_focus(c))
   {
      dbg("client wants focus");
      XSetInputFocus(w->dpy, c->window,
		     RevertToPointerRoot, CurrentTime);
      w->focused_client = c;
   } 

   base_client_show(c);

#if 0
   /* make sure any dialog clients are raised above */
   /* move / resize any dialogs */
   if (w->main_client &&   !(w->flags & DESKTOP_RAISED_FLAG)) 
     base_client_show(w->main_client);
#endif
}

void
toolbar_client_hide(Client *c)
{
   if (c->flags & CLIENT_IS_MINIMIZED || client_get_state(c) == IconicState) 
     return;
   
   client_set_state(c,IconicState);
   c->flags |= CLIENT_IS_MINIMIZED; 

   c->ignore_unmap++;
   XUnmapWindow(c->wm->dpy, c->window);

   client_buttons_delete_all(c);   

   c->x = wm_get_offsets_size(c->wm, WEST,  NULL, False);

   c->y = c->y + (c->height - theme_frame_defined_height_get(c->wm->mbtheme,
							     FRAME_UTILITY_MIN)
		  );
#ifdef STANDALONE
   if (c->backing) 
     XFreePixmap(c->wm->dpy, c->backing);
   c->backing = None;
#else
   if (c->backing != NULL)
     mb_drawable_unref(c->backing);
   c->backing = NULL;
#endif

   toolbar_client_move_resize(c);

   dbg("hiding toolbar y is now %i", c->y);

   wm_update_layout(c->wm, c, c->height - toolbar_win_offset(c));
}

void
toolbar_client_destroy(Client *c)
{
  Wm *w = c->wm;

  dbg("%s() called\n", __func__);
   
  c->mapped = False; /* Setting mapped to false will allow the 
                        dialog resizing/repositioning via restack
                        to ignore use  */

  if (c->x == theme_frame_defined_width_get(c->wm->mbtheme,
					    FRAME_UTILITY_MAX )
      || (c->flags & CLIENT_TITLE_HIDDEN_FLAG) )
    {
      wm_update_layout(w, c, c->height);
    } 
  else 
    {
      wm_update_layout(w, c, theme_frame_defined_height_get(c->wm->mbtheme,
							   FRAME_UTILITY_MIN));
    }
  
  base_client_destroy(c);     
}

void
toolbar_client_get_coverage(Client *c, int *x, int *y, int *w, int *h)
{
   *x = c->x; *y = c->y;
   *w = c->width + toolbar_win_offset(c);

   if (!(c->flags & CLIENT_IS_MINIMIZED))
   {
      *x = c->x - toolbar_win_offset(c);
      *h = c->height;
   } else {
     *h = toolbar_win_offset(c);
   }
}

void
toolbar_client_button_press(Client *c, XButtonEvent *e)
{
   int frame_id, cw, ch;
   int max_offset = theme_frame_defined_width_get(c->wm->mbtheme, 
						   FRAME_UTILITY_MAX);
   int min_offset = theme_frame_defined_height_get(c->wm->mbtheme, 
						    FRAME_UTILITY_MIN);

   if (!(c->flags & CLIENT_IS_MINIMIZED))
     {
       frame_id = FRAME_UTILITY_MAX;
       cw = max_offset;
       ch = c->height;
     }
   else
     {
       frame_id = FRAME_UTILITY_MIN;
       cw = c->width + max_offset; 
       ch = min_offset;
     }
   
   switch (client_button_do_ops(c, e, frame_id, cw, ch))
     {
     case BUTTON_ACTION_CLOSE:
       client_deliver_delete(c);
       break;
     case BUTTON_ACTION_MIN:
       toolbar_client_hide(c);
       break;
     case BUTTON_ACTION_MAX:
       toolbar_client_show(c);
       break;
     case -1: 		 /* Cancelled  */
       break;
     case 0:
       /* Not on button */
       break;
     }
}

int
toolbar_win_offset(Client *c)
{
   if (c->flags & CLIENT_IS_MINIMIZED)
   {
      return theme_frame_defined_height_get(c->wm->mbtheme, 
					    FRAME_UTILITY_MIN);
   } else {

     if (c->flags & CLIENT_TITLE_HIDDEN_FLAG) return 0;

     return theme_frame_defined_width_get(c->wm->mbtheme, 
					  FRAME_UTILITY_MAX);
   }
}

void
toolbar_client_redraw(Client *c, Bool use_cache)
{
   int max_offset = theme_frame_defined_width_get(c->wm->mbtheme, 
						   FRAME_UTILITY_MAX);
   int min_offset = theme_frame_defined_height_get(c->wm->mbtheme, 
						    FRAME_UTILITY_MIN);
   if (use_cache && c->backing != None) return;

   client_buttons_delete_all(c);

   if (c->flags & CLIENT_IS_MINIMIZED)
   {
     if (!min_offset) return;

     if (c->flags & CLIENT_TITLE_HIDDEN_FLAG) max_offset = 0;

      if (c->backing == None)
	 client_init_backing(c, c->width + max_offset, min_offset);

      theme_frame_paint(c->wm->mbtheme, c, FRAME_UTILITY_MIN, 
			0, 0, 
			c->width + max_offset, 
			min_offset); 

      theme_frame_button_paint(c->wm->mbtheme, c, BUTTON_ACTION_CLOSE, 
			       INACTIVE, FRAME_UTILITY_MIN, 
			       c->width + max_offset, min_offset);

      theme_frame_button_paint(c->wm->mbtheme, c, BUTTON_ACTION_MAX, 
			       INACTIVE, FRAME_UTILITY_MIN, 
			       c->width + max_offset, min_offset);

   } else {

     if (!max_offset) return;

     if (c->flags & CLIENT_TITLE_HIDDEN_FLAG) return;

     if (c->backing == None)
       client_init_backing(c, max_offset, c->height);

      theme_frame_paint( c->wm->mbtheme, c, FRAME_UTILITY_MAX,
			 0, 0, max_offset, c->height);

      theme_frame_button_paint(c->wm->mbtheme, c, BUTTON_ACTION_CLOSE, 
			       INACTIVE, FRAME_UTILITY_MAX, 
			       max_offset, c->height);

      theme_frame_button_paint(c->wm->mbtheme, c, BUTTON_ACTION_MIN, 
			       INACTIVE, FRAME_UTILITY_MAX, 
			       max_offset, c->height);
   }

#ifdef STANDALONE
   XSetWindowBackgroundPixmap(c->wm->dpy, c->title_frame, c->backing);
#else
   XSetWindowBackgroundPixmap(c->wm->dpy, c->title_frame, 
			      mb_drawable_pixmap(c->backing));
#endif
   XClearWindow(c->wm->dpy, c->title_frame);

}





