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


#include "dialog_client.h"

/********************************************
 *
 * Experimental dialog dragging modifications
 */

/* different dragging restraints */
#define DIALOG_DRAG_NONE         1
#define DIALOG_DRAG_RESTRAIN     2
#define DIALOG_DRAG_FREE         3

/* Selected restraint */
#define DIALOG_DRAG_MODE         DIALOG_DRAG_FREE

/* Border size for DIALOG_DRAG_RESTRAIN */
#define DIALOG_DRAG_RESTRAIN_BDR 16

/* Hide the dialog ( just show border ) when dragging  */
#define DIALOG_WANT_HIDDEN_DRAG  1


/********************************************/


static void dialog_client_check_for_state_hints(Client *c);
static void dialog_client_drag(Client *c);
static void _get_mouse_position(Wm *w, int *x, int *y);
static void _draw_outline(Client *c, int x, int y, int w, int h);

Client*
dialog_client_new(Wm *w, Window win, Client *trans)
{

   Client *c = base_client_new(w, win); 

   if (!c) return NULL;

   c->type = dialog;
   
   c->reparent     = &dialog_client_reparent;
   c->move_resize  = &dialog_client_move_resize;
   c->hide         = &dialog_client_hide;
   
   c->configure    = &dialog_client_configure;
   c->button_press = &dialog_client_button_press;
   c->redraw       = &dialog_client_redraw;
   c->show         = &dialog_client_show;
   c->destroy      = &dialog_client_destroy;
   c->get_coverage = &dialog_client_get_coverage;

   if (trans)
     {
       /* 
        * Dont let dialogs be transient for other dialogs.
        */
       if (trans->type != mainwin)
	 c->trans = trans->trans;
       else
	 c->trans = trans;	 
     }

   dialog_client_check_for_state_hints(c);

   return c;
}


static void
dialog_client_get_offsets(Client *c, int *e, int *s, int *w)
{
  /* no decor dialogs */
  if (c->flags & CLIENT_TITLE_HIDDEN_FLAG)
    {
      *s = 0; *e = 0; *w = 0;
      return;
    }

#ifdef USE_MSG_WIN
  if (c->flags & CLIENT_IS_MESSAGE_DIALOG)
    {
      *s = theme_frame_defined_height_get(c->wm->mbtheme, 
				       FRAME_MSG_SOUTH);
      *e = theme_frame_defined_width_get(c->wm->mbtheme, 
					 FRAME_MSG_EAST );
      *w = theme_frame_defined_width_get(c->wm->mbtheme, 
					 FRAME_MSG_WEST );
      return;
    }
#endif 

   *s = theme_frame_defined_height_get(c->wm->mbtheme, 
				       FRAME_DIALOG_SOUTH);
   *e = theme_frame_defined_width_get(c->wm->mbtheme, 
				      FRAME_DIALOG_EAST );
   *w = theme_frame_defined_width_get(c->wm->mbtheme, 
				      FRAME_DIALOG_WEST );
}


static void
dialog_client_check_for_state_hints(Client *c)
{
  if (ewmh_state_check(c, c->wm->atoms[WINDOW_STATE_MODAL]))
    {
      Client *damaged;

      dbg("%s() got modal hint, setting flag\n", __func__);
      c->flags ^= CLIENT_IS_MODAL_FLAG;

      /* Call comp_engine_client_show to add damage to main window 
       * so it gets fully lowlighted ok. 
       */
      if ((damaged = wm_get_visible_main_client(c->wm)) != NULL)
	{
	  comp_engine_client_show(c->wm, damaged);
	}
    }
}


void
dialog_client_get_coverage(Client *c, int *x, int *y, int *w, int *h)
{
   int frm_size = dialog_client_title_height(c);
   int east,south,west;

   dialog_client_get_offsets(c, &east, &south, &west);

   *x = c->x - west;
   *y = c->y - frm_size;
   *w = c->width + east + west;
   *h = c->height + frm_size + south;
}


void
dialog_client_move_resize(Client *c)
{
   int frm_size     = dialog_client_title_height(c);
   int offset_south = 0, offset_west = 0, offset_east = 0;

   dialog_client_get_offsets(c, &offset_east, &offset_south, &offset_west);

   base_client_move_resize(c);

   XMoveResizeWindow(c->wm->dpy, c->window, 
		     offset_west,
		     frm_size,
		     c->width, c->height);

#ifndef USE_COMPOSITE
   if (c->wm->config->dialog_shade && (c->flags & CLIENT_IS_MODAL_FLAG))
     {
       XMoveResizeWindow(c->wm->dpy, 
			 c->title_frame, 
			 c->x - offset_west, 
			 c->y - frm_size, 
			 c->width + offset_east + offset_west,
			 c->height + frm_size + offset_south );
       XMoveWindow(c->wm->dpy, 
		   c->window, 
		   c->x, 
		   c->y);

     }
   else
#endif
     {
       XMoveResizeWindow(c->wm->dpy, 
			 c->frame, 
			 c->x - offset_west, 
			 c->y - frm_size, 
			 c->width + offset_west + offset_east,
			 c->height + frm_size + offset_south
			 );
       XResizeWindow(c->wm->dpy, 
			 c->title_frame, 
			 c->width + offset_east + offset_west,
			 c->height + frm_size + offset_south );
     }
}


void
dialog_client_hide(Client *c)
{
  Client *damaged;
  dbg("%s() called for %s\n", __func__, c->name);
  XLowerWindow(c->wm->dpy, c->frame);

  if (c->flags & CLIENT_IS_MODAL_FLAG 
      && (damaged = wm_get_visible_main_client(c->wm)) != NULL)
    {
      comp_engine_client_show(c->wm, damaged);
    }

  comp_engine_client_hide(c->wm, c);
}


int
dialog_client_title_height(Client *c)
{
   if (c->flags & CLIENT_TITLE_HIDDEN_FLAG)
      return 0;

#ifdef USE_MSG_WIN
  if (c->flags & CLIENT_IS_MESSAGE_DIALOG)
    {
      return theme_frame_defined_height_get(c->wm->mbtheme, FRAME_MSG);
    }
#endif

  if (c->flags & CLIENT_BORDERS_ONLY_FLAG
      && theme_has_frame_type_defined(c->wm->mbtheme, FRAME_DIALOG_NORTH))    
    return theme_frame_defined_height_get(c->wm->mbtheme, FRAME_DIALOG_NORTH);

   return theme_frame_defined_height_get(c->wm->mbtheme, FRAME_DIALOG);
}

void
dialog_client_show(Client *c)
{


  dbg("%s() called for %s\n", __func__, c->name);

  /* XXX paint before map so dialog gets shape 
         can probably be more optimised / experimental at the moment. 
  */
  if (client_get_state(c) != NormalState)
    dialog_client_redraw(c, False);

  client_set_state(c, NormalState); 
  XMapSubwindows(c->wm->dpy, c->frame);
  XMapRaised(c->wm->dpy, c->frame);

  if (client_want_focus(c) && (!(c->flags & CLIENT_IS_MESSAGE_DIALOG)))
    {
      XSetInputFocus(c->wm->dpy, c->window,
		     RevertToPointerRoot, CurrentTime);
      c->wm->focused_client = c;
    }

  comp_engine_client_show(c->wm, c);

#ifdef USE_MSG_WIN 	
  if (c->flags & CLIENT_IS_MESSAGE_DIALOG_LO)
    {      Client *t = NULL;

      /* Make sure any higher priority dialogs are mapped above */
      START_CLIENT_LOOP(c->wm, t)
	{
	  if (t->flags & CLIENT_IS_MESSAGE_DIALOG_HI)
	    {
	      t->show(t);
	      /* We need to call show to get the composite stacking right */
	      comp_engine_client_show(c->wm, t);
	    }
	}
      END_CLIENT_LOOP(c->wm, t);
    }

  if (c->wm->msg_win_queue_head)
    {
      Client *msg_client = NULL;
      if ((msg_client = wm_find_client(c->wm, 
				       c->wm->msg_win_queue_head->win, 
				       WINDOW)) != NULL)
	if (msg_client != c) 
	  {
	    msg_client->show(msg_client);
	    /* We need to call show to get the composite stacking right */
	    comp_engine_client_show(c->wm, msg_client);
	  }
    }
#endif

  c->mapped = True;
}


void
dialog_client_reparent(Client *c)
{
   XSetWindowAttributes attr;

   int offset_north = dialog_client_title_height(c);
   int offset_south = 0, offset_west = 0, offset_east = 0;

   dialog_client_get_offsets(c, &offset_east, &offset_south, &offset_west);

   attr.override_redirect = True; 
   attr.background_pixel  = 0; /* BlackPixel(c->wm->dpy, c->wm->screen); */
   attr.border_pixel      = 0;
   attr.event_mask        = ChildMask|ButtonPressMask|ExposureMask;

   attr.colormap          = c->cmap; 

   dbg("%s() want lowlight : wm:%i , client:%i\n", __func__,
       c->wm->config->dialog_shade, (c->flags & CLIENT_IS_MODAL_FLAG));
#ifndef USE_COMPOSITE
   if (c->wm->config->dialog_shade && (c->flags & CLIENT_IS_MODAL_FLAG))
     {
       dbg("%s() LOWLIGHTING\n", __func__);
       wm_lowlight(c->wm, c);
     }
   else
#endif
     {
       if (c->flags & CLIENT_TITLE_HIDDEN_FLAG) 
	 {
	   c->frame = c->window;
	 }
       else c->frame = XCreateWindow(c->wm->dpy, 
				     c->wm->root, 
				     0, 0,
				     c->width + offset_east + offset_west, 
				     c->height + offset_north + offset_south, 
				     0,
#ifdef USE_COMPOSITE
				     c->is_argb32 ? 32  : CopyFromParent,
				     InputOutput,  
				     c->is_argb32 ? c->visual : CopyFromParent,
#else
				     CopyFromParent,
				     CopyFromParent, 
				     CopyFromParent,

#endif
				     CWOverrideRedirect|CWEventMask
				     |CWBackPixel|CWBorderPixel|CWColormap, 
				     &attr);
     }

   if (c->flags & CLIENT_TITLE_HIDDEN_FLAG)
     {
       c->title_frame = c->window;
     }
   else
     {
       c->title_frame =
	 XCreateWindow(c->wm->dpy, 
		       c->frame, 
		       0, 0, 
		       c->width + offset_east + offset_west, 
		       c->height + offset_north + offset_south, 
		       0,
		       CopyFromParent, /* depth */
		       CopyFromParent, 
		       CopyFromParent, /* visual */
		       CWOverrideRedirect|CWBackPixel|CWEventMask|CWBorderPixel|CWColormap, 
		       &attr);

     }
   
   XSetWindowBorderWidth(c->wm->dpy, c->window, 0);

   XAddToSaveSet(c->wm->dpy, c->window);

   XSelectInput(c->wm->dpy, c->window,
		ButtonPressMask|ColormapChangeMask|PropertyChangeMask);

   if (c->frame != c->window)
     XReparentWindow(c->wm->dpy, c->window, c->frame, 
		     offset_west, offset_north);
}

/*  Padding between dialog borders and area available  
 *
 */
#define DIALOG_PADDING 4 

void
dialog_get_available_area(Client *c,
			  int    *x,
			  int    *y,
			  int    *width,
			  int    *height)
{
  Wm *w = c->wm;

#if 0
  if (c->trans)
    {
      /* Transient for root dialog,
       * 
       *
       */


    }
  else
#endif

    dbg("%s() south size is %i\n", __func__,  wm_get_offsets_size(w, SOUTH, NULL, True));

  if (c->flags & CLIENT_TITLE_HIDDEN_FLAG 
      || c->flags & CLIENT_IS_MESSAGE_DIALOG)
    {
      /* Decorationless dialogs can position themselves anywhere */
      *y = 0; *x = 0; *height = w->dpy_height; *width = w->dpy_width;
    }
  else
    {
      Client *p = NULL;
      Bool    have_toolbar = False;

      START_CLIENT_LOOP(w, p)
      {
	if (p->type == toolbar && p->mapped 
	    && !(p->flags & CLIENT_IS_MINIMIZED))
	  { have_toolbar = True; break; }
      }
      END_CLIENT_LOOP(w, p);

      *y      = wm_get_offsets_size(w, NORTH, NULL, True);

      /* if toolbar ( input window present ) dialogs can cover titlebars */
      if (!have_toolbar)
	*y  += main_client_title_height(c->trans);

      *height = w->dpy_height - *y - wm_get_offsets_size(w, SOUTH, NULL, True);
      *x      = wm_get_offsets_size(w, WEST, NULL, True);
      *width  = w->dpy_width - *x - wm_get_offsets_size(w, EAST, NULL, True);
    }
}

/* 
   dialog_check_gemoetry()

   called mainly by wm_restack to suggest better positions for dialogs
   in relation to panels and toolbar/input wins. 

   req params are reparented window geometry *without* borders

   returns True if geometry supplied fits - is good. 
   retruns False if geometry supplyied bad,  supplied geometry is updated
   to fit. 
 */
Bool
dialog_check_geometry(Client *c,
		      int    *req_x,
		      int    *req_y,
		      int    *req_width,
		      int    *req_height)
{
  Wm  *w = c->wm;
  int avail_x, avail_y, avail_width, avail_height;
  int actual_x, actual_y, actual_width, actual_height;
  int bdr_south = 0, bdr_west = 0, bdr_east = 0, bdr_north = 0;

  Bool res = True;

  if (w->config->dialog_stratergy == WM_DIALOGS_STRATERGY_FREE)
    return True;

  /* Allow decorationless dialogs to position themselves anywhere */
  if (c->flags & CLIENT_TITLE_HIDDEN_FLAG)
    return True;

  if (c->flags & CLIENT_IS_MESSAGE_DIALOG)
    return True;

  dialog_get_available_area(c,&avail_x, &avail_y, &avail_width, &avail_height);

  /* Figure out window border offsets */
  dialog_client_get_offsets(c, &bdr_east, &bdr_south, &bdr_west);
  bdr_north = dialog_client_title_height(c);

  dbg("%s() - \n\t avail_x : %d\n\tavail_y : %d\n\tavail_width : %d"
      "\n\tavail_height %d\n\tbdr_south : %d\n\tbdr_west : %d\n\tbdr_east : %d\n\tbdr_north : %d\n",
      __func__, avail_x, avail_y, avail_width, avail_height,
      bdr_south, bdr_west, bdr_east, bdr_north);

  /* Dialog geometry with decorations */
  actual_x = *req_x - bdr_east - DIALOG_PADDING;
  actual_y = *req_y - bdr_north - DIALOG_PADDING;
  actual_width = *req_width + bdr_east + bdr_west + (2*DIALOG_PADDING);
  actual_height = *req_height + bdr_north + bdr_south + (2*DIALOG_PADDING);

  if (c->init_width && c->init_height 
      && (c->init_width > *req_width || c->init_height > *req_height) )
    {
      actual_width  = c->init_width + bdr_east + bdr_west + (2*DIALOG_PADDING);
      actual_height = c->init_height + bdr_north + bdr_south + (2*DIALOG_PADDING);
    }

  if (actual_width > avail_width)  /* set width to fit  */
    {
      *req_width = avail_width - ( bdr_east + bdr_west + (2*DIALOG_PADDING));
      actual_width = avail_width;
      res = False;
    }

  if (actual_height > avail_height)  /* and height  */
    {
      *req_height = avail_height -(bdr_south + bdr_north + (2*DIALOG_PADDING));
      actual_height = avail_height;
      res = False;
    }

  if (actual_x < avail_x) 
    {
      *req_x = avail_x + bdr_west + DIALOG_PADDING; /* move dialog right */
      res = False;
    }

  if (actual_y < avail_y) 
    {
      *req_y = avail_y + bdr_north + DIALOG_PADDING; /* move dialog up */
      res = False;
    }

  if (actual_x > avail_x 
      && (actual_x + actual_width) > (avail_x + avail_width) )
    {
      *req_x = avail_x + bdr_west + DIALOG_PADDING; /* move dialog right */
      res = False;
    }

   if ( (actual_y + actual_height) > (avail_y + avail_height) )
     {
       *req_y = (avail_y + avail_height) - actual_height + bdr_north + DIALOG_PADDING;
       res = False;
     }

  return res;
}
		
void
dialog_init_geometry(Client *c)      
{
  /* 
     Called by initial configure() 
     
     - Check for 0,0 position. 
       - 0,0 ? center. 
     - Save initial position. 

  */
  Wm  *w = c->wm;
  int  avail_x, avail_y, avail_width, avail_height;
  int  bdr_south = 0, bdr_west = 0, bdr_east = 0, bdr_north = 0;

  /* Check if we actually want to perform any sizing intervention */
  if (w->config->dialog_stratergy == WM_DIALOGS_STRATERGY_FREE)
    return;

  /* Allow decorationless dialogs to position themselves anywhere */
  if (c->flags & CLIENT_TITLE_HIDDEN_FLAG)
    return;

  dialog_get_available_area(c,&avail_x, &avail_y, &avail_width, &avail_height);

  /* Figure out window border offsets */
  dialog_client_get_offsets(c, &bdr_east, &bdr_south, &bdr_west);
  bdr_north = dialog_client_title_height(c);

  dbg("%s() - \n\t avail_x : %d\n\tavail_y : %d\n\tavail_width : %d"
      "\n\tavail_height %d\n\tbdr_south : %d\n\tbdr_west : %d\n\tbdr_east : %d\n\tbdr_north : %d\n",
      __func__, avail_x, avail_y, avail_width, avail_height,
      bdr_south, bdr_west, bdr_east, bdr_north);

  /* Message Dialogs are free to postion/size where ever but can use totally  
   * offscreen request to position to window corners - see below
   */
  if (c->flags & CLIENT_IS_MESSAGE_DIALOG)
    {
      int win_width  = c->width + bdr_east;
      int win_height = c->height + bdr_south;

      if (c->x >= w->dpy_width) 
	c->x = w->dpy_width - total_win_width - (c->x - w->dpy_width );

      if (c->y >= w->dpy_height) 
	c->y = w->dpy_height - total_win_height - (c->y - w->dpy_height );

      return;
    }

  /* save values for possible resizing later if more space comes available */
  c->init_width  = c->width;
  c->init_height = c->height;

  dbg("%s() set init, %ix%i, wants x:%d y:%d\n", __func__, c->init_width, c->init_height, c->x, c->y); 

  /* Fix width/height  */
  if ((c->width + bdr_east + bdr_west) > avail_width)
    c->width = (avail_width - bdr_east - bdr_west - (2*DIALOG_PADDING));

  if ((c->height + bdr_north + bdr_south) > avail_height)
    c->height = (avail_height - bdr_north - bdr_south - (2*DIALOG_PADDING));


  /* Reposition dialog initially centered if ;
      + positioned at 0,0
      + positioned offscreen
    */
  if ( (c->x - bdr_west) <= avail_x 
       || (c->x + c->width + bdr_east + bdr_west) > (avail_x + avail_width))
    {
      dbg("%s() centering x pos\n", __func__);
      c->x = (avail_width  - (c->width + bdr_east + bdr_west))/2 
	+ bdr_west + avail_x;
    }

  if ( (c->y - bdr_north) <= avail_y
       || (c->y + c->height + bdr_south + bdr_north) > (avail_y+avail_height))
    {
      dbg("%s() centering y pos\n", __func__);
      c->y = (avail_height - (c->height + bdr_south + bdr_north))/2 + avail_y + bdr_north;
    }

}

void
dialog_client_configure(Client *c)
{
  dialog_init_geometry(c);

#if 0

#define MAX_PADDING 8 		/*Used to make sure there is always some free
				  border space around a dialog */

  int side_frame_width = 0, bottom_frame_width = 0;
  int max_w = 0, max_h = 0, toolbar_y = 0; 
  int offset_south = 0, offset_west = 0, offset_east = 0, offset_north = 0;

  /* Check if we actually want to perform any sizing intervention */
  if (c->wm->config->dialog_stratergy == WM_DIALOGS_STRATERGY_FREE)
    return;

  /* Figure out window border offsets */
  dialog_client_get_offsets(c, &offset_east, &offset_south, &offset_west);
  offset_north = dialog_client_title_height(c);

  /* Message Dialogs are free to postion/size where ever but can use totally  
   * offscreen request to position to window corners - see below
   */
  if (c->flags & CLIENT_IS_MESSAGE_DIALOG)
    {
      int total_win_width  = c->width + offset_east + offset_west;
      int total_win_height = c->height + offset_south + offset_north;

      if (c->x > c->wm->dpy_width) 
	c->x = c->wm->dpy_width - total_win_width - (c->x - c->wm->dpy_width );

      if (c->y > c->wm->dpy_height) 
	c->y = c->wm->dpy_height - total_win_height - (c->y - c->wm->dpy_height );
      return;
    }


  /* Check window has decoration */
  if (!(c->flags & CLIENT_TITLE_HIDDEN_FLAG))
    {
      side_frame_width = offset_east + offset_west;
      bottom_frame_width = offset_south;
    }

  /* Figure out where the toolbar/dock is.
     - Toolbars dont effect messages and splash windows */
  if ((c->flags & CLIENT_TITLE_HIDDEN_FLAG) 
      || (c->flags & CLIENT_IS_MESSAGE_DIALOG))
    toolbar_y = c->wm->dpy_height; 
  else
    toolbar_y = c->wm->dpy_height 
      - wm_get_offsets_size(c->wm, SOUTH, NULL,True);

  /* Calculate maximum allowed width of dialog */
  max_w = c->wm->dpy_width - side_frame_width - MAX_PADDING;

  /* Figure out max dialog size */
  max_h = toolbar_y - bottom_frame_width - offset_north - MAX_PADDING;

  dbg("dialog wants %i x %i, %i x %i\n", c->x, c->y, c->width, c->height);

  /* Resize vertical + move out of the way of any toolbars 
     ( Toolbars assumed to be input methods + we dont wanna cover dock ) 
  */
  if (c->height > max_h) 
    {
      c->height = max_h;
      c->flags |=  CLIENT_SHRUNK_FOR_TB_FLAG;
      c->y = offset_north + ( MAX_PADDING/2 );
      dbg("%s() c->height > max_h . c->y is now %i\n", __func__, c->y);
    }
  else
    {
      /* move dialog up above any toolbars  */
      if ( ( c->y + c->height + bottom_frame_width ) > toolbar_y)
	{
	  c->y -= (( c->y + c->height + bottom_frame_width ) - toolbar_y );

	  dbg("%s() overlapping toolbar . c->y is now %i\n", __func__, c->y);

	  /* check c->y is now not off screen */
	  if ((c->y - offset_north) <= 0)
	    {
	      c->height += c->y;
	      c->y = offset_north + ( MAX_PADDING/2 );
	    }
	}
    }

  /* Now horiz constraints */

  /* horiz contarined mode - force dialog to be full width*/
  if (c->wm->config->dialog_stratergy == WM_DIALOGS_STRATERGY_CONSTRAINED_HORIZ
      && !(c->flags & CLIENT_TITLE_HIDDEN_FLAG) )
    {
      c->x = 0 - offset_east;
      c->width = c->wm->dpy_width - side_frame_width;
      
      return;
    }

  /* Allow decorationless dialogs to position themselves anywhere */
  if (c->flags & CLIENT_TITLE_HIDDEN_FLAG)
    return;

  /* Clip horizonal width */
  if (c->width > max_w) c->width = max_w;
   
  /* Finally reposition dialog ( centered ) if ;
      + positioned at 0,0
      + positioned offscreen
    */
  if ( (c->x - offset_west) <= 0 
       || (c->x + c->width + side_frame_width) > c->wm->dpy_width)
    {
      dbg("%s() centering x pos\n", __func__);
      c->x = (c->wm->dpy_width  - (c->width + side_frame_width))/2 
	+ offset_west;
    }

  if ( (c->y - offset_north) <= 0 
       || (c->y + c->height + offset_south) > c->wm->dpy_height)
    {
      dbg("%s() centering y pos\n", __func__);
      c->y = (c->wm->dpy_height - (c->height + offset_south))/2;
    }

#endif
}

void
dialog_client_redraw(Client *c, Bool use_cache)
{
  Bool is_shaped = False;

  int offset_north = dialog_client_title_height(c);
  int offset_south = 0, offset_west = 0, offset_east = 0;
  int total_w = 0, total_h = 0;

  int frame_ref_top   = FRAME_DIALOG;
  int frame_ref_east  = FRAME_DIALOG_EAST;
  int frame_ref_west  = FRAME_DIALOG_WEST;
  int frame_ref_south = FRAME_DIALOG_SOUTH;

  dialog_client_get_offsets(c, &offset_east, &offset_south, &offset_west);

  total_w = offset_east  + offset_west + c->width;
  total_h = offset_north + offset_south + c->height;


  if (c->flags & CLIENT_BORDERS_ONLY_FLAG 
      && theme_has_frame_type_defined(c->wm->mbtheme, FRAME_DIALOG_NORTH))
    frame_ref_top   = FRAME_DIALOG_NORTH;

  /* 'message dialogs have there own decorations */
  if (c->flags & CLIENT_IS_MESSAGE_DIALOG)
    {
      frame_ref_top   = FRAME_MSG;
      frame_ref_east  = FRAME_MSG_EAST;
      frame_ref_west  = FRAME_MSG_WEST;
      frame_ref_south = FRAME_MSG_SOUTH;
    }

  dbg("%s() c->width : %i , offset_east : %i, offset_west : %i\n",
      __func__, c->width, offset_east, offset_west );

  if (c->flags & CLIENT_TITLE_HIDDEN_FLAG) return;

   if (use_cache && c->backing != None) return;

  is_shaped = theme_frame_wants_shaped_window( c->wm->mbtheme, frame_ref_top);
  
  if (c->backing == (Pixmap)NULL)
    client_init_backing(c, total_w, total_h);

  if (is_shaped) client_init_backing_mask(c, total_w, c->height, 
					  offset_north, offset_south,
					  offset_east, offset_west);
#ifdef STANDALONE
  /* Should prevent some flicker */
  XSetForeground(c->wm->dpy, c->wm->mbtheme->gc, 
		 BlackPixel(c->wm->dpy, c->wm->screen));
  XFillRectangle(c->wm->dpy, c->backing, c->wm->mbtheme->gc, 
		 0, 0, total_w, total_h);
#endif

  theme_frame_paint(c->wm->mbtheme, c, frame_ref_top, 
		    0, 0, total_w, offset_north); 
    
  theme_frame_paint(c->wm->mbtheme, c, frame_ref_west, 
		    0, offset_north, offset_west, c->height); 
  
  theme_frame_paint(c->wm->mbtheme, c, frame_ref_east, 
		    total_w - offset_east, offset_north, 
		    offset_east, c->height); 

  theme_frame_paint(c->wm->mbtheme, c, frame_ref_south, 
		    0, total_h - offset_south, 
		    total_w, offset_south); 

  /* We dont paint buttons for borderonly and message dialogs */
  if (!(c->flags & CLIENT_BORDERS_ONLY_FLAG
	|| c->flags & CLIENT_IS_MESSAGE_DIALOG))
    {

      theme_frame_button_paint(c->wm->mbtheme, c, 
			       BUTTON_ACTION_CLOSE, 
			       INACTIVE, FRAME_DIALOG, 
			       total_w, offset_north);

      if (c->flags & CLIENT_ACCEPT_BUTTON_FLAG)
	theme_frame_button_paint(c->wm->mbtheme, c, BUTTON_ACTION_ACCEPT, 
				 INACTIVE, FRAME_DIALOG,total_w, offset_north);

      if (c->flags & CLIENT_HELP_BUTTON_FLAG)
	theme_frame_button_paint(c->wm->mbtheme, c, BUTTON_ACTION_HELP, 
				 INACTIVE, FRAME_DIALOG,total_w, offset_north);

    }

  /* XXXX ifdef HAVE_SHAPE */
  if (is_shaped && !c->is_argb32)
    {
      XRectangle rects[1];

      rects[0].x = offset_west;  
      rects[0].y = offset_north;
      rects[0].width = total_w - offset_west - offset_east;
      rects[0].height = total_h - offset_south - offset_north;

      XShapeCombineRectangles ( c->wm->dpy, c->title_frame, 
				ShapeBounding,
				0, 0, rects, 1, ShapeSet, 0 );
      
#ifndef USE_COMPOSITE
      if (c->wm->config->dialog_shade && (c->flags & CLIENT_IS_MODAL_FLAG)) 
	{
	  XShapeCombineMask( c->wm->dpy, c->title_frame, 
			     ShapeBounding, 0, 0, 
			     c->backing_masks[MSK_NORTH], ShapeUnion);
	  XShapeCombineMask( c->wm->dpy, c->title_frame, 
			     ShapeBounding, 0, total_h - offset_south, 
			     c->backing_masks[MSK_SOUTH], ShapeUnion);

	  XShapeCombineMask( c->wm->dpy, c->title_frame, 
			     ShapeBounding, 0, offset_north, 
			     c->backing_masks[MSK_WEST], ShapeUnion);
	  XShapeCombineMask( c->wm->dpy, c->title_frame, 
			     ShapeBounding, 
			     total_w - offset_east, offset_north,
			     c->backing_masks[MSK_EAST], ShapeUnion);
	}
      else
#endif
	{
	  XShapeCombineMask( c->wm->dpy, c->title_frame, 
			     ShapeBounding, 0, 0, 
			     c->backing_masks[MSK_NORTH], ShapeUnion);

	  XShapeCombineMask( c->wm->dpy, c->title_frame, 
			     ShapeBounding, 0, total_h - offset_south, 
			     c->backing_masks[MSK_SOUTH], ShapeUnion);

	  XShapeCombineMask( c->wm->dpy, c->title_frame, 
			     ShapeBounding, 0, offset_north, 
			     c->backing_masks[MSK_WEST], ShapeUnion);
	  XShapeCombineMask( c->wm->dpy, c->title_frame, 
			     ShapeBounding, 
			     total_w - offset_east, offset_north,
			     c->backing_masks[MSK_EAST], ShapeUnion);

	  XShapeCombineShape ( c->wm->dpy, 
			       c->frame,
			       ShapeBounding, 0, 0, 
			       c->title_frame,
			       ShapeBounding, ShapeSet);
	}
    }

#ifdef STANDALONE
   XSetWindowBackgroundPixmap(c->wm->dpy, c->title_frame, c->backing);
#else
   XSetWindowBackgroundPixmap(c->wm->dpy, c->title_frame, 
			      mb_drawable_pixmap(c->backing));
#endif

   XClearWindow(c->wm->dpy, c->title_frame);

   XFlush(c->wm->dpy);
}


void
dialog_client_button_press(Client *c, XButtonEvent *e)
{
  int offset_north = dialog_client_title_height(c);
  int offset_south = 0, offset_west = 0, offset_east = 0;

  if (c->flags & CLIENT_IS_MESSAGE_DIALOG) return;

  dialog_client_get_offsets(c, &offset_east, &offset_south, &offset_west);

  dbg("%s() c->width : %i , offset_east : %i, offset_west : %i\n",
      __func__, c->width, offset_east, offset_west );

   switch (client_button_do_ops(c, e, FRAME_DIALOG, 
				c->width + offset_east + offset_west, 
				offset_north))
   {
      case BUTTON_ACTION_CLOSE:
	 client_deliver_delete(c);
	 break;
      case BUTTON_ACTION_HELP:
	client_deliver_wm_protocol(c, c->wm->atoms[_NET_WM_CONTEXT_HELP]);
	 break;
      case BUTTON_ACTION_ACCEPT:
	client_deliver_wm_protocol(c, c->wm->atoms[_NET_WM_CONTEXT_ACCEPT]);
      case -1: 		 /* Cancelled  */
	 break;
      case 0:
	 dialog_client_drag(c);     /* Not on button */
	 break;
   }
}


static void
dialog_client_drag(Client *c) /* drag box */
{
  XEvent ev;
  int x1, y1;
  int old_cx = c->x;
  int old_cy = c->y;
    
  int frm_size     = dialog_client_title_height(c);
  int offset_south = 0, offset_west = 0, offset_east = 0;

#ifdef USE_MSG_WIN
  Client *t = NULL;
#endif 

  dialog_client_get_offsets(c, &offset_east, &offset_south, &offset_west);

  if (XGrabPointer(c->wm->dpy, c->wm->root, False,
		   (ButtonPressMask|ButtonReleaseMask|PointerMotionMask),
		   GrabModeAsync,
		   GrabModeAsync, None, c->wm->curs_drag, CurrentTime)
      != GrabSuccess)
    return;
    


  /* Let the comp know theres gonna be damage in out old position.
   * XXX Must be a better way need to figure it out.    
   */

#if (DIALOG_WANT_HIDDEN_DRAG) 	/* hide the dialog on drag */

#ifdef USE_COMPOSITE

  comp_engine_client_hide(c->wm, c);
  comp_engine_render(c->wm, c->wm->all_damage);

#endif

#else

 comp_engine_client_show(c->wm, c); 

#endif

  c->flags |= CLIENT_IS_MOVING;

  _get_mouse_position(c->wm, &x1, &y1);

  _draw_outline(c, c->x - offset_west, c->y - frm_size,
		c->width + offset_west + offset_east,
		c->height + frm_size + offset_south);

#if (DIALOG_WANT_HIDDEN_DRAG) 	/* hide the dialog on drag */

#ifndef USE_COMPOSITE 		/* .. for lowlighted dialogs - ewe */
  if (c->flags & CLIENT_IS_MODAL_FLAG && c->wm->config->dialog_shade)
    {
      XUnmapWindow(c->wm->dpy, c->window);
      XUnmapWindow(c->wm->dpy, c->title_frame);
    }
  else 
#endif
    XUnmapWindow(c->wm->dpy, c->frame);

  c->ignore_unmap++;
#endif

  XSync(c->wm->dpy, False);
    
  for (;;) 
    {
      int wanted_x = 0, wanted_y = 0, have_grab = 0;
      
      XMaskEvent(c->wm->dpy, 
		 ButtonPressMask|ButtonReleaseMask|PointerMotionMask,
		 &ev);

    switch (ev.type) 
      {
      case MotionNotify:
	if (!have_grab) 
	  { XGrabServer(c->wm->dpy); have_grab = 1; }
	  
	_draw_outline(c, 
		      c->x - offset_west, 
		      c->y - frm_size,
		      c->width + offset_west + offset_east,
		      c->height + frm_size + offset_south);

#if (DIALOG_DRAG_MODE)

	wanted_x = (old_cx + (ev.xmotion.x - x1));
	wanted_y = (old_cy + (ev.xmotion.y - y1));

	switch (DIALOG_DRAG_MODE) 
	  {
	  case DIALOG_DRAG_NONE:
	    /* No modifications to postion */
	    break;
	  case DIALOG_DRAG_RESTRAIN:

	    if ( (wanted_x - offset_west) < 0 - DIALOG_DRAG_RESTRAIN_BDR)
	      c->x = 0 - DIALOG_DRAG_RESTRAIN_BDR + offset_west;
	    else if ( (wanted_x + c->width + offset_east) > c->wm->dpy_width + DIALOG_DRAG_RESTRAIN_BDR)
	      c->x = c->wm->dpy_width + DIALOG_DRAG_RESTRAIN_BDR - (c->width + offset_east);
	    else c->x = wanted_x;
	    
	    if ( (wanted_y - frm_size) < 0 - DIALOG_DRAG_RESTRAIN_BDR)
	      c->y = 0 - DIALOG_DRAG_RESTRAIN_BDR + frm_size;
	    else if ( (wanted_y + c->height + offset_south) > c->wm->dpy_height + DIALOG_DRAG_RESTRAIN_BDR)
	      c->y = c->wm->dpy_height + DIALOG_DRAG_RESTRAIN_BDR - (c->height + offset_south);
	    else c->y = wanted_y;

	    break;
          case DIALOG_DRAG_FREE:
	    c->x = wanted_x;
	    c->y = wanted_y;
	    break;
	  default:
	    break;
	  }


#else  /* Dialog drag mode disabled below */

	if (c->wm->config->dialog_stratergy != WM_DIALOGS_STRATERGY_CONSTRAINED_HORIZ)
	  c->x = old_cx + (ev.xmotion.x - x1);

	c->y = old_cy + (ev.xmotion.y - y1);
#endif

	_draw_outline(c, c->x - offset_west, c->y - frm_size,
		      c->width + offset_west + offset_east,
		      c->height + frm_size + offset_south);
	break;
	
      case ButtonRelease:
	dbg("drag, got release");
	_draw_outline(c, c->x - offset_west, c->y - frm_size,
		      c->width + offset_west + offset_east,
		      c->height + frm_size + offset_south);

#ifndef USE_COMPOSITE	
	if (c->wm->config->dialog_shade && (c->flags & CLIENT_IS_MODAL_FLAG))
	  {
	    XMoveResizeWindow(c->wm->dpy, 
			      c->title_frame, 
			      c->x - offset_west, 
			      c->y - frm_size, 
			      c->width + offset_east + offset_west,
			      c->height + frm_size + offset_south);
	    XMoveResizeWindow(c->wm->dpy, 
			      c->window, 
			      c->x, 
			      c->y, 
			      c->width,
			      c->height);
	  } 
	else
#endif 
	  {
	    XMoveWindow(c->wm->dpy, c->frame, c->x - offset_west,
			c->y - dialog_client_title_height(c));
	  }
	
	c->show(c);
	
#ifdef USE_MSG_WIN
	/* Check for message super above dialogs */
	START_CLIENT_LOOP(c->wm, t)
	  {
	    if (t->flags & CLIENT_IS_MESSAGE_DIALOG
		&& !(t->flags & CLIENT_IS_MESSAGE_DIALOG_HI)
		&& !(t->flags & CLIENT_IS_MESSAGE_DIALOG_LO))
	      {
		t->show(t);
		 comp_engine_client_show(c->wm, t); 
		break;
	      }
	  }
	END_CLIENT_LOOP(c->wm, t);
#endif 
	c->flags &= ~ CLIENT_IS_MOVING;
	
	XUngrabPointer(c->wm->dpy, CurrentTime);
	XUngrabServer(c->wm->dpy);
	return;
      }

    }
  client_deliver_config(c);
}


static void
_get_mouse_position(Wm *w, int *x, int *y)
{
    Window mouse_root, mouse_win;
    int win_x, win_y;
    unsigned int mask;

    XQueryPointer(w->dpy, w->root, &mouse_root, &mouse_win,
        x, y, &win_x, &win_y, &mask);
}


static void
_draw_outline(Client *c, int x, int y, int width, int height)
{
  XDrawRectangle(c->wm->dpy, c->wm->root, c->wm->mbtheme->band_gc, x-1, y-1, 
		 width+2, height+2);
}

 
void dialog_client_destroy(Client *c)
{
  Wm     *w = c->wm; 
  Client *d = NULL;
#ifdef USE_MSG_WIN
  int was_msg = (c->flags & CLIENT_IS_MESSAGE_DIALOG
		 && !(c->flags & CLIENT_IS_MESSAGE_DIALOG_HI)
		 && !(c->flags & CLIENT_IS_MESSAGE_DIALOG_LO));
#endif 

  base_client_destroy(c);

  /* We could be a dialog over a desktop, in which case we
     need to give it back keyboard focus - this is a bit
     ugly. XXX Improve XXX
  */

  if ((d = wm_get_visible_main_client(w)) != NULL)
    {
      if (client_want_focus(d))
	{
	  XSetInputFocus(w->dpy, d->window,
			 RevertToPointerRoot, CurrentTime);
	  w->focused_client = d;
	}

      if (w->flags & DESKTOP_RAISED_FLAG)
	comp_engine_client_show (w, d); 
    }


#ifdef USE_MSG_WIN

   if (was_msg) 
     {
       dbg("%s() was message poping queue\n", __func__);
       wm_msg_win_queue_pop(w);
     }
#endif 

   
}
