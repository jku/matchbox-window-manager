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
  $Id: main_client.c,v 1.12 2004/10/28 17:13:28 mallum Exp $
*/

#include "main_client.h"

#define VBW(c) 5
#define SBW(c) 5

Client*
main_client_new(Wm *w, Window win)
{
   Client *c = base_client_new(w, win); 

   if (!c) return NULL;
   
   c->type = MBCLIENT_TYPE_APP;
   c->reparent     = &main_client_reparent;
   c->redraw       = &main_client_redraw;
   c->button_press = &main_client_button_press;
   c->move_resize  = &main_client_move_resize;
   c->get_coverage = &main_client_get_coverage;
   c->hide         = &main_client_hide;
   c->show         = &main_client_show;
   c->configure    = &main_client_configure;
   c->destroy      = &main_client_destroy;
   c->iconize      = &main_client_iconize;

   main_client_check_for_state_hints(c);

   main_client_check_for_single(c);
   
   return c;
}

void
main_client_check_for_state_hints(Client *c)
{
  dbg("%s() checking for fullscreen: %s\n", __func__,
      XGetAtomName(c->wm->dpy, c->wm->atoms[WINDOW_STATE_FULLSCREEN]));

  if (ewmh_state_check(c, c->wm->atoms[WINDOW_STATE_FULLSCREEN]))
    {
      c->flags ^= CLIENT_FULLSCREEN_FLAG;
      dbg("%s() client is fullscreen\n", __func__);
    }
}

void
main_client_check_for_single(Client *c)
{
  Wm     *w = c->wm;

  if (w->flags & SINGLE_FLAG)
    {
      /* There was only main client till this came along */
      w->flags ^= SINGLE_FLAG; /* turn off single flag */
      main_client_redraw(w->stack_top_app, False); /* update menu button */
    } else if (!w->stack_top_app) /* This must be the only client*/
      c->wm->flags |= SINGLE_FLAG; /* so turn on single flag */      
}

/* Handle the case for showing input methods ( toolbars ) 
 * for fullscreen 
 */
int
main_client_manage_toolbars_for_fullscreen(Client *c, Bool main_client_showing)
{
  Wm     *w = c->wm;
  Client *p  = NULL;
  int     south_panel_size = 0, south_total_size = 0;

  if (main_client_showing 
      && (c->flags & CLIENT_TOOLBARS_MOVED_FOR_FULLSCREEN))
    return 0;

  if (!main_client_showing 
      && !(c->flags & CLIENT_TOOLBARS_MOVED_FOR_FULLSCREEN))
    return 0;

  south_panel_size = wm_get_offsets_size(w, SOUTH, NULL, False); 
  south_total_size = wm_get_offsets_size(w, SOUTH, NULL, True); 

  c->flags ^= CLIENT_TOOLBARS_MOVED_FOR_FULLSCREEN;    

  if (south_total_size > south_panel_size) /* there are toolbars */
    {
      stack_enumerate(w, p)
	{
	  /* move toolbar wins up/down over panels */
	  if (p->type == MBCLIENT_TYPE_TOOLBAR && p->mapped) 
	    {
	      if (main_client_showing)
		{
		  p->y += south_panel_size; 

		  /* cover vertical panels */
		  p->x = toolbar_win_offset(p);
		  p->width = w->dpy_width - toolbar_win_offset(p);
		}
	      else
		{
		  /* uncover any vertical panels */
		  p->x = toolbar_win_offset(p) 
		    + wm_get_offsets_size(w, WEST,  NULL, False);
		  p->width = w->dpy_width - toolbar_win_offset(c)
		    - wm_get_offsets_size(w, WEST,  NULL, False)
		    - wm_get_offsets_size(w, EAST,  NULL, False);

		  p->y -= south_panel_size; 

		}

	      p->move_resize(p);
	      XMapRaised(w->dpy, p->frame);
	    }
	  else if (p->type == MBCLIENT_TYPE_PANEL && main_client_showing)
	    {
	      XLowerWindow(w->dpy, p->frame);
	    }
	}

      return (south_total_size - south_panel_size);
    }

  return 0;
}

void
main_client_configure(Client *c)
{
  Wm *w = c->wm;
  int frm_size = main_client_title_height(c);
  int offset_south = theme_frame_defined_height_get(c->wm->mbtheme, 
						    FRAME_MAIN_SOUTH);
  int offset_east  = theme_frame_defined_width_get(c->wm->mbtheme, 
						   FRAME_MAIN_EAST );
  int offset_west  = theme_frame_defined_width_get(c->wm->mbtheme, 
						   FRAME_MAIN_WEST );

  int h = wm_get_offsets_size(w, SOUTH, NULL, True);

   if ( c->flags & CLIENT_FULLSCREEN_FLAG )
     { 
       c->y = 0;  
       c->x = 0;
       c->width  = w->dpy_width;
       c->height = w->dpy_height - main_client_manage_toolbars_for_fullscreen(c, True);
       
     }
   else
     {
       c->y = wm_get_offsets_size(c->wm, NORTH, NULL, False) + frm_size;
       c->x = wm_get_offsets_size(c->wm, WEST,  NULL, False) + offset_west;
       c->width  = c->wm->dpy_width - ( offset_east + offset_west ) 
	 - wm_get_offsets_size(c->wm, EAST,  NULL, False)
	 - wm_get_offsets_size(c->wm, WEST,  NULL, False);

       c->height = c->wm->dpy_height - c->y - h - offset_south;
       main_client_manage_toolbars_for_fullscreen(c, False);
     }

   dbg("%s() configured as %i*%i+%i+%i, frame size is %i\n", 
       __func__, c->width, c->height, c->x, c->y, frm_size);
   
   // c->wm->main_client = c;
}

int
main_client_title_height(Client *c)
{
  if (c == NULL || c->type != MBCLIENT_TYPE_APP)
    return 0;

  if ( (!c->wm->config->use_title)
       || c->flags & CLIENT_FULLSCREEN_FLAG) return 0;

  if ((c->wm->flags & TITLE_HIDDEN_FLAG) && c->type == MBCLIENT_TYPE_APP)
    return TITLE_HIDDEN_SZ;

  return theme_frame_defined_height_get(c->wm->mbtheme, FRAME_MAIN);
}

void
main_client_get_coverage(Client *c, int *x, int *y, int *w, int *h)
{
  int offset_south = theme_frame_defined_height_get(c->wm->mbtheme, 
						    FRAME_MAIN_SOUTH);
  int offset_east  = theme_frame_defined_width_get(c->wm->mbtheme, 
						   FRAME_MAIN_EAST );
  int offset_west  = theme_frame_defined_width_get(c->wm->mbtheme, 
						   FRAME_MAIN_WEST );

   *x = c->x - offset_west; 
   *y = c->y - main_client_title_height(c);
   *w = c->width + offset_east + offset_west;
   *h = c->height + main_client_title_height(c) + offset_south;

   dbg("%s() +%i+%i, %ix%i\n", __func__, *x, *y, *w, *h);
}

void
main_client_reparent(Client *c)
{
  Wm *w = c->wm;
  XSetWindowAttributes attr;

  int frm_size = main_client_title_height(c);
  int offset_south = theme_frame_defined_height_get(w->mbtheme, 
						    FRAME_MAIN_SOUTH);
  int offset_east  = theme_frame_defined_width_get(w->mbtheme, 
						   FRAME_MAIN_EAST );
  int offset_west  = theme_frame_defined_width_get(w->mbtheme, 
						     FRAME_MAIN_WEST );
  attr.override_redirect = True;
  attr.background_pixel  = w->grey_col.pixel; /* BlackPixel(w->dpy, w->screen); */
  attr.event_mask         = ChildMask|ButtonMask|ExposureMask;
  
  c->frame =
    XCreateWindow(w->dpy, w->root, 
		  c->x - offset_west, 
		  c->y - frm_size,
		  c->width + offset_east + offset_west, 
		  c->height + frm_size + offset_south, 
		  0,
		  CopyFromParent, CopyFromParent, CopyFromParent,
		  CWOverrideRedirect|CWEventMask|CWBackPixel,
		  &attr);
  
  dbg("%s frame created : %i*%i+%i+%i\n",
      __func__, c->width, c->height + frm_size, c->x, c->y);
  
  c->title_frame =
    XCreateWindow(w->dpy, c->frame, 0, 0, 
		  c->width + ( offset_east + offset_west), 
		  frm_size + c->height + offset_south, 0,
		  CopyFromParent, CopyFromParent, CopyFromParent,
		  CWBackPixel|CWEventMask, &attr);
  
  XSetWindowBorderWidth(w->dpy, c->window, 0);
  XAddToSaveSet(w->dpy, c->window);
  XSelectInput(w->dpy, c->window, ColormapChangeMask|PropertyChangeMask);
  XReparentWindow(w->dpy, c->window, c->frame, offset_west, frm_size);
}


void
main_client_move_resize(Client *c)
{
  Wm *w = c->wm;

  int offset_south = theme_frame_defined_height_get(w->mbtheme, 
						    FRAME_MAIN_SOUTH);
  int offset_east  = theme_frame_defined_width_get(w->mbtheme, 
						   FRAME_MAIN_EAST );
  int offset_west  = theme_frame_defined_width_get(w->mbtheme, 
						   FRAME_MAIN_WEST );
  base_client_move_resize(c);

  XMoveResizeWindow(w->dpy, c->window, 
		    offset_west, main_client_title_height(c), 
		    c->width, c->height);
  
  XResizeWindow(w->dpy, c->title_frame, 
		c->width + (offset_east + offset_west),
		c->height + main_client_title_height(c) + offset_south);
  
  XMoveResizeWindow(w->dpy, c->frame, 
		    c->x - offset_west,
		    c->y - main_client_title_height(c), 
		    c->width + ( offset_east + offset_west),
		    c->height + main_client_title_height(c) + offset_south);
}


void
main_client_toggle_fullscreen(Client *c)
{
  Wm *w = c->wm;

  XGrabServer(w->dpy);

  c->flags ^= CLIENT_FULLSCREEN_FLAG;

  main_client_configure(c);
  main_client_move_resize(c);

  if (!(c->flags & CLIENT_FULLSCREEN_FLAG))
    {
      /* Client was fullscreen - to be safe we redraw decoration buttons */
      client_buttons_delete_all(c);    
      c->redraw(c, False);
    }

#if 0
  if (w->have_titlebar_panel 
      && mbtheme_has_titlebar_panel(w->mbtheme))
    {
      if (c->flags & CLIENT_FULLSCREEN_FLAG)
	{
	  w->have_titlebar_panel->ignore_unmap++;
	  XUnmapWindow(w->dpy, w->have_titlebar_panel->frame);
	}
      else
	{
	  XMapRaised(w->dpy, w->have_titlebar_panel->frame);
	}

    }
#endif

  ewmh_state_set(c); /* Let win know it fullscreen state has changed, it
		        could be waiting on this to adjust ui */

  wm_activate_client(c); /* Reactivate, stacking order slightly different  */

  XUngrabServer(w->dpy);
}


/* redraws the frame */
void
main_client_redraw(Client *c, Bool use_cache)
{
  Wm  *w = c->wm;
  Bool is_shaped = False;
  int  width = 0, height = 0;
  int  offset_south, offset_east, offset_west;

  dbg("%s() called on %s\n", __func__, c->name);

   if (!w->config->use_title) return;

   if (w->flags & TITLE_HIDDEN_FLAG)
   {
      XUnmapWindow(w->dpy, c->title_frame);
      return;
   }

   if (use_cache && c->have_set_bg)  return ;

   offset_south = theme_frame_defined_height_get(w->mbtheme, 
						 FRAME_MAIN_SOUTH);
   offset_east  = theme_frame_defined_width_get(w->mbtheme, 
						FRAME_MAIN_EAST );
   offset_west  = theme_frame_defined_width_get(w->mbtheme, 
						FRAME_MAIN_WEST );
 
   width = c->width + offset_east + offset_west;
   height = theme_frame_defined_height_get(w->mbtheme, FRAME_MAIN);
   
   is_shaped = theme_frame_wants_shaped_window( w->mbtheme, FRAME_MAIN);

   dbg("%s() cache failed, actual redraw on %s\n", __func__, c->name);

   if (c->backing == None)
      client_init_backing(c, c->width + offset_east + offset_west, 
			     c->height + offset_south + height);

   if (is_shaped) 
     client_init_backing_mask(c, c->width + offset_east + offset_west, 
			      c->height, 
			      height , offset_south,
			      width - offset_east, offset_west);

   dbg("%s() calling theme_frame_paint()\n", __func__); 
   theme_frame_paint(w->mbtheme, c, FRAME_MAIN, 0, 0, width, height); 

   theme_frame_paint(w->mbtheme, c, FRAME_MAIN_WEST, 
		     0, height, offset_west, c->height); 
  
   theme_frame_paint(w->mbtheme, c, FRAME_MAIN_EAST, 
		     c->width + offset_west, height, 
		     offset_east, c->height); 

   dbg("%s() painting south %i+%i - %ix%i \n", __func__, 
       		     0, c->height + height, 
		     c->width + offset_east + offset_west, offset_south);

   theme_frame_paint(w->mbtheme, c, FRAME_MAIN_SOUTH, 
		     0, c->height + height, 
		     c->width + offset_east + offset_west, offset_south); 

   if (!(c->flags & CLIENT_IS_DESKTOP_FLAG))
     theme_frame_button_paint(w->mbtheme, c, BUTTON_ACTION_CLOSE, 
			      INACTIVE, FRAME_MAIN, width, height);

   theme_frame_button_paint(w->mbtheme, c, BUTTON_ACTION_HIDE, 
			    INACTIVE, FRAME_MAIN, width, height);

   if (!(w->flags & SINGLE_FLAG))
   {
      dbg("%s() painting next / prev buttons\n", __func__);
      theme_frame_button_paint(w->mbtheme, c, BUTTON_ACTION_MENU, 
			       INACTIVE, FRAME_MAIN, width, height);
      theme_frame_button_paint(w->mbtheme, c, BUTTON_ACTION_NEXT, 
			       INACTIVE, FRAME_MAIN, width, height);
      theme_frame_button_paint(w->mbtheme, c, BUTTON_ACTION_PREV, 
			       INACTIVE, FRAME_MAIN, width, height);
   } else {
     client_button_remove(c, BUTTON_ACTION_NEXT);
     client_button_remove(c, BUTTON_ACTION_PREV);

     if (!(w->flags & DESKTOP_DECOR_FLAG)
	   && wm_get_desktop(c->wm)) /* Paint the dropdown for the desktop */
       {
	 dbg("%s() have desktop\n", __func__);
	 theme_frame_button_paint(w->mbtheme, c, BUTTON_ACTION_MENU, 
				  INACTIVE, FRAME_MAIN, width, height);

	 theme_frame_button_paint(w->mbtheme, c, BUTTON_ACTION_DESKTOP, 
				  INACTIVE, FRAME_MAIN, width, height);
       }
     else 
       {
	 dbg("%s() removing menu button\n", __func__ );
	 client_button_remove(c, BUTTON_ACTION_MENU);
       }
   }

   if (c->flags & CLIENT_ACCEPT_BUTTON_FLAG)
      theme_frame_button_paint(w->mbtheme, c, BUTTON_ACTION_ACCEPT, 
			       INACTIVE, FRAME_MAIN, width, height);

   if (c->flags & CLIENT_HELP_BUTTON_FLAG)
     {
       dbg("%s() painting help button\n", __func__);
       theme_frame_button_paint(w->mbtheme, c, BUTTON_ACTION_HELP, 
				INACTIVE, FRAME_MAIN, width, height);
     }

   if (c->flags & CLIENT_CUSTOM_BUTTON_FLAG)
     {
       dbg("%s() painting help button\n", __func__);
       theme_frame_button_paint(w->mbtheme, c, BUTTON_ACTION_CUSTOM, 
				INACTIVE, FRAME_MAIN, width, height);
     }

  if (is_shaped)   /* XXX do we really need titleframe here ? */
    {
      XRectangle rects[1];

      rects[0].x = 0;
      rects[0].y = height;
      rects[0].width = width;
      rects[0].height = c->height;

      XShapeCombineRectangles ( w->dpy, c->title_frame, 
				ShapeBounding,
				0, 0, rects, 1, ShapeSet, 0 );

      XShapeCombineMask( w->dpy, c->title_frame, 
			 ShapeBounding, 0, 0, 
			 c->backing_masks[MSK_NORTH], ShapeUnion);

      XShapeCombineMask( w->dpy, c->title_frame, 
			 ShapeBounding, 0, c->height + height, 
			 c->backing_masks[MSK_SOUTH], ShapeUnion);

      XShapeCombineShape ( w->dpy, 
			   c->frame,
			   ShapeBounding, 0, 0, 
			   c->title_frame,
			   ShapeBounding, ShapeSet);
    }

#ifdef STANDALONE
   XSetWindowBackgroundPixmap(w->dpy, c->title_frame, c->backing);
#else
   XSetWindowBackgroundPixmap(w->dpy, c->title_frame, 
			      mb_drawable_pixmap(c->backing));
#endif

   XClearWindow(w->dpy, c->title_frame);

#ifdef STANDALONE
   XFreePixmap(w->dpy, c->backing);
   c->backing = None;
#else
   mb_drawable_unref(c->backing);
   c->backing = NULL;
#endif

   c->have_set_bg = True;
}


void main_client_button_press(Client *c, XButtonEvent *e)
{
  Wm *w = c->wm;
  int ch = 0;

   if (!w->config->use_title) return;

   if (w->flags & TITLE_HIDDEN_FLAG)
   {
      main_client_toggle_title_bar(c);
      XMapWindow(w->dpy, c->title_frame);
      XMapSubwindows(w->dpy, c->title_frame);
      return;
   }

   ch = theme_frame_defined_height_get(w->mbtheme, FRAME_MAIN);

   switch (client_button_do_ops(c, e, FRAME_MAIN, c->width, ch))
     {
      case BUTTON_ACTION_DESKTOP:
	 wm_toggle_desktop(w);
	 break;
      case BUTTON_ACTION_CLOSE:
	 client_deliver_delete(c);
	 break;
      case BUTTON_ACTION_NEXT:
	wm_activate_client(stack_cycle_backward(w, MBCLIENT_TYPE_APP));
	 break;
      case BUTTON_ACTION_PREV:
	wm_activate_client(stack_cycle_forward(w, MBCLIENT_TYPE_APP));
	 break;
      case BUTTON_ACTION_MENU:
	 select_client_new(w);
	 break;
      case BUTTON_ACTION_HIDE:
	 main_client_toggle_title_bar(c);
	 break;
      case BUTTON_ACTION_HELP:
	client_deliver_wm_protocol(c, w->atoms[_NET_WM_CONTEXT_HELP]);
	 break;
      case BUTTON_ACTION_ACCEPT:
	client_deliver_wm_protocol(c, w->atoms[_NET_WM_CONTEXT_ACCEPT]);
	 break;
      case BUTTON_ACTION_CUSTOM:
	client_deliver_wm_protocol(c, w->atoms[_NET_WM_CONTEXT_CUSTOM]);
	 break;
      case -1: 		 
	/* Cancelled  */
	 break;
      case 0:
	 /* Not on button */
	 break;
   }
}


void
main_client_toggle_title_bar(Client *c)
{
  Wm *w = c->wm;

  Client *p = NULL;
  int prev_height = main_client_title_height(c);
  int y_offset = wm_get_offsets_size(c->wm, NORTH, NULL, False);
  
  w->flags ^= TITLE_HIDDEN_FLAG;
  
  dbg("%s() called\n", __func__);
  
  XGrabServer(w->dpy);
  
  theme_img_cache_clear( w->mbtheme,  FRAME_MAIN );
  
  stack_enumerate(c->wm, p)
    if (p->type == MBCLIENT_TYPE_APP)
      {
	if (w->flags & TITLE_HIDDEN_FLAG)
	  {  /* hide */
	    p->height += (prev_height - TITLE_HIDDEN_SZ );
	    p->y = y_offset + TITLE_HIDDEN_SZ;
	    
	    if (w->have_titlebar_panel 
		&& mbtheme_has_titlebar_panel(w->mbtheme))
	      {
		w->have_titlebar_panel->ignore_unmap++;
		XUnmapWindow(w->dpy, w->have_titlebar_panel->frame);
	      }
	    
	  } else {
	    /* show */
	    p->y = main_client_title_height(p) + y_offset;
	    p->height -= ( main_client_title_height(p) - TITLE_HIDDEN_SZ );
	    XMapWindow(w->dpy, p->title_frame); /* prev will have unmapped */
	    
	    if (w->have_titlebar_panel
		&& mbtheme_has_titlebar_panel(w->mbtheme))
	      XMapRaised(w->dpy, w->have_titlebar_panel->frame);
	    
	  }
	p->move_resize(p);
	p->redraw(p, False);
      }
  
  XUngrabServer(w->dpy);
}


void
main_client_hide(Client *c)
{
  Wm *w = c->wm;

  dbg("%s() called\n", __func__);

  base_client_hide(c);
  
  if ( c->flags & CLIENT_FULLSCREEN_FLAG )
    main_client_manage_toolbars_for_fullscreen(c, False);
  
   /* lower window to bottom of stack */
   XLowerWindow(w->dpy, c->frame);
}


void
main_client_iconize(Client *c)
{
  client_set_state(c, IconicState);
  main_client_unmap(c);

#if 0
  base_client_iconize(c);
  p = client_get_prev(c, MBCLIENT_TYPE_APP);
  if (p) { p->show(p); }
#endif

}

/* 
 *  - Add new raise and lower methods. leave dummy show, but remove later  
 *  - Change hide to lower ? 
 *    - or unmap method ? 
 *
 *
 */

void
main_client_show(Client *c)
{
  Wm     *w = c->wm;
  Client *cur = NULL;

   dbg("%s() called on %s\n", __func__, c->name);
   
   if (w->flags & DESKTOP_RAISED_FLAG) 
     {
       // w->flags ^= DESKTOP_RAISED_FLAG; /* desktop not raised anymore */
       c->flags |= CLIENT_NEW_FOR_DESKTOP;
     }
   else
     {
       c->flags &= ~CLIENT_NEW_FOR_DESKTOP;
     }

   // w->main_client = c;


   /*
   if ( c->flags & CLIENT_FULLSCREEN_FLAG )
     stack_move_above_extended(c, NULL, dock, 0);
   else
     stack_move_above_extended(c, NULL, mainwin, 0);
   */

   /* Move this client and any transients to the very top of the stack.
      wm_activate_client() ( call it sync_display ? ) will then take
      care of painels etc as it can use active client as a 'watermark' 
   */
   stack_move_top(c);

   
   stack_dump(w);

   if (!c->mapped)
     {
       XMapSubwindows(w->dpy, c->frame);
       XMapWindow(w->dpy, c->frame);
     }

   c->mapped = True;

   /* check input focus */
   if (client_want_focus(c))
   {
      XSetInputFocus(w->dpy, c->window,
		     RevertToPointerRoot, CurrentTime);
      w->focused_client = c;
   }





#if 0
   /* Make sure the desktop is always at the bottom */
   if (!(w->flags & DESKTOP_DECOR_FLAG)
       && (desktop = wm_get_desktop(c->wm)) != NULL)
     XLowerWindow(w->dpy, desktop->window);


   XMapRaised(w->dpy, c->frame);
   XMapSubwindows(w->dpy, c->frame);

   if (w->have_titlebar_panel 
       && mbtheme_has_titlebar_panel(w->mbtheme)
       && !(c->flags & CLIENT_FULLSCREEN_FLAG))
     XMapRaised(w->dpy, w->have_titlebar_panel->frame);


   /* check input focus */
   if (client_want_focus(c))
   {
      XSetInputFocus(w->dpy, c->window,
		     RevertToPointerRoot, CurrentTime);
      w->focused_client = c;
   }

   if (c->flags & CLIENT_FULLSCREEN_FLAG)
     main_client_manage_toolbars_for_fullscreen(c, True);

   /* deal with transients etc */
   base_client_show(c);
#endif
}

void
main_client_unmap(Client *c)
{
   Wm     *w = c->wm;
   Client *next_client = NULL; 

   dbg("%s called for %s\n", __func__, c->name);

   if ( c->flags & CLIENT_FULLSCREEN_FLAG )
     main_client_manage_toolbars_for_fullscreen(c, False);

   /* Are we at the top of the stack ? */
   if (c == w->stack_top_app)
     {
       next_client = stack_get_below(c, MBCLIENT_TYPE_APP);
       
       dbg("%s() at stack top\n", __func__ );

       /* Is this the only main client left? */
       if(next_client == c) 
	 {
	   dbg("%s() only client left\n", __func__ );

	   if (w->flags & SINGLE_FLAG)
	     w->flags ^= SINGLE_FLAG; /* single flag off ( for menu button ) */
	   
	   w->stack_top_app = NULL; /* XXX safe ? */
	   
	   /* is there a desktop ? */
	   next_client = wm_get_desktop(w);
#if 0
	       /* Hide a dock in titlebar of exists, should call hide() ? */
	       if (w->have_titlebar_panel
		   && !(w->have_titlebar_panel->flags & CLIENT_DOCK_TITLEBAR_SHOW_ON_DESKTOP))
	   {
	     dbg("%s() unmapping panel\n", __func__);
	     XUnmapWindow(w->dpy, w->have_titlebar_panel->frame); 
	   }
	   return;
#endif
	 }
       else
	 {
	   /* There are more main clients left, but we may have been 
            * opened from the desktop and it therefor makes sense to
            * go back there. 
	    */
	   if (c->flags & CLIENT_NEW_FOR_DESKTOP)
	     next_client = wm_get_desktop(w);
	 }
	   
     }

   c->mapped = False;

   if (next_client /* only 1 main_client left ? */
       && (next_client == stack_get_below(next_client, MBCLIENT_TYPE_APP)))
     {
       dbg("%s() turning on single flag\n", __func__);
       w->flags |= SINGLE_FLAG; /* turn on single flag for menu button */
       main_client_redraw(next_client, False);
     }

   XUnmapWindow(w->dpy, c->frame); 

   if (next_client)
     wm_activate_client(next_client);   
}

void
main_client_destroy(Client *c)
{
   dbg("%s called for %s\n", __func__, c->name);
  
#if 0

   /* What main clients are left?, need to decide what nav buttons appear */

  if ( c->flags & CLIENT_FULLSCREEN_FLAG )
    main_client_manage_toolbars_for_fullscreen(c, False);

   if (c == w->main_client)
   {
      w->main_client = client_get_prev(c, mainwin);
      
      if(w->main_client == c)  /* Is this the only main client left? */
      {
	 w->main_client = NULL;
	 if (w->flags & SINGLE_FLAG)
	    w->flags ^= SINGLE_FLAG; /* turn off single flag */
	 base_client_destroy(c);

	 /* Check for a desktop win and update  */
	 if (!(w->flags & DESKTOP_RAISED_FLAG))
	   {
	     wm_toggle_desktop(w);
	   }
	 else wm_activate_client(w->head_client);

	 /* Hide a dock in titlebar of exists, should call hide() ? */
	 if (w->have_titlebar_panel
	     && !(w->have_titlebar_panel->flags & CLIENT_DOCK_TITLEBAR_SHOW_ON_DESKTOP))
	   {
	     dbg("%s() unmapping panel\n", __func__);
	     XUnmapWindow(w->dpy, w->have_titlebar_panel->frame); 
	   }
	 return;
      }
      /* If we came from the desktop, make sure we go back there */
      if (c->flags & CLIENT_NEW_FOR_DESKTOP)
	{
	  base_client_destroy(c); 
	  wm_toggle_desktop(w);
	  return;
	}
   }

#endif

   main_client_unmap(c);

   base_client_destroy(c); 

#if 0
   wm_activate_client(w->main_client);   

   if (w->main_client == client_get_prev(w->main_client, mainwin))
     {  /* Is only 1 main_client left  */
       w->flags ^= SINGLE_FLAG; /* turn on single flag */      
       main_client_redraw(w->main_client, False);
     }
#endif
}




