#include "stack.h"

void
stack_add_below_client(Client *client, Client *client_above)
{
  Wm *w = client->wm;

  if (client_above == NULL)
    {
      /* nothing above raising to top */
      if (w->stack_top)
	{
	  client->below = w->stack_top;
	  w->stack_top->below = client;
	}

      w->stack_top = client;
    }
  else
    {
      client->above = client_above;
      client->below = client_above->below;

      if (client_above->below) client_above->below->above = client;
      client_above->below = client;
    }

  if (client_above == w->stack_bottom)
    w->stack_bottom = client;

  w->stack_n_items++;
}


void
stack_add_above_client(Client *client, Client *client_below)
{
  Wm *w = client->wm;

  if (client_below == NULL)
    {
      /* NULL so nothing below add at bottom */
      if (w->stack_bottom) 
	{
	  client->above = w->stack_bottom;
	  w->stack_bottom->below = client;
	}

      w->stack_bottom = client;
    }
  else
    {
      client->below = client_below;
      client->above = client_below->above;
      if (client->below) client->below->above = client;
      if (client->above) client->above->below = client;


    }

  if (client_below == w->stack_top)
    w->stack_top = client;

  w->stack_n_items++;
}


void   /* can this call above client ? */
stack_append_top(Client *client)
{
  Wm *w = client->wm;

  stack_add_above_client(client, w->stack_top);
}

void   /* can this call above client ? */
stack_prepend_bottom(Client *client)
{
  Wm *w = client->wm;

  stack_add_below_client(client, w->stack_bottom);
}



void
stack_remove(Client *client)
{
  Wm *w = client->wm;

  if (w->stack_top == w->stack_bottom)
    {
      w->stack_top = w->stack_bottom = NULL;
    }
  else
    {
      if (client == w->stack_top)
	w->stack_top = client->below;

      if (client == w->stack_bottom)
	w->stack_bottom = client->above;

      if (client->below != NULL) client->below->above = client->above;
      if (client->above != NULL) client->above->below = client->below;
    }

  client->above = client->below = NULL;

  w->stack_n_items--;
}

void
stack_preppend(Client *client)
{

}

void
stack_update_transients(Client *client, Client *client_below)
{
  Wm     *w = client->wm;
  Client *client_cur;

  stack_enumerate_transients(w,client_cur,client)
    {
      stack_move_above_client(client_cur, client);
    }

  stack_enumerate_transients(w,client_cur,client_below)
    {
      stack_move_below_client(client_cur, client);
    }


}

void
stack_move_above_extended(Client *client,       
			  Client *client_below,
			  int     type, 
			  int     flags)
{
  Wm     *w = client->wm;
  Client *client_found_below = NULL, *client_cur;

  if (client_below == NULL) 
    client_below = w->stack_bottom;
  else
    client_below = client_below->above;

  for (client_cur = client_below; 
       client_cur; 
       client_cur = client_cur->above)   
    {
      if (client_cur->type & type)
	{
	  if (flags)
	    {
	      if (client_cur->flags & flags)
		client_found_below = client_cur;
	    }
	  else client_found_below = client_cur;
	}
    }


  if (client_found_below)  /* TODO: what if this is NULL ?? */
    {
      dbg("%s() moving %s just above %s\n", 
	  __func__, client->name, client_found_below->name); 
      stack_move_above_client(client, client_found_below);
    }
}

void 				/* XXX needed ? should sync with above */
stack_move_below_type(Client *client, int type, int flags)
{
  Wm     *w = client->wm;
  Client *client_above = NULL, *client_cur;

  stack_enumerate_reverse(w, client_cur)
    {
      if (client_cur->type == type)
	{
	  if (flags)
	    {
	      if (client_cur->flags & flags)
		client_above = client_cur;
	    }
	  else client_above = client_cur;
	}
    }

  if (client_above)  /* TODO: what if this is NULL ?? */
    stack_move_below_client(client, client_above);


  /* must move any transients too */
}

void
stack_move_below_client(Client *client, Client *client_above)
{
  if (client == client_above) return;

  stack_remove(client);
  stack_add_below_client(client, client_above);

  /* must move any transients too */
}

void
stack_move_above_client(Client *client, Client *client_below)
{
  if (client == client_below) return;

  stack_remove(client);
  stack_add_above_client(client, client_below);

  /* must move any transients too */
  // stack_update_transients(client, client_below);

  /* XXX Is this a good idea. I think it makes more sense to just
   *     provide mechanism here and add the policy higher up.
   *
   */

}

void
stack_move_type_above_client(MBClientTypeEnum wanted_type, Client *client)
{
  Wm     *w = client->wm;
  Client *c = NULL;

  stack_enumerate(w,c)
    if ((c->type & wanted_type) && c->mapped)
      stack_move_above_client(c, client);

}

void
stack_move_type_below_client(MBClientTypeEnum wanted_type, Client *client)
{
  Wm     *w = client->wm;
  Client *c = NULL;

  stack_enumerate_reverse(w,c)
    if ((c->type & wanted_type) && c->mapped)
      stack_move_below_client(c, client);
}

void
stack_move_transients_to_top(Wm *w, Client *client_trans_for, int flags)
{
  /* This function shifts a clients transients to the top
   * of the stack keeping there respective order.   
   *
   *
   */

  MBList *transient_list = NULL, *list_item = NULL;

  client_get_transient_list(w, &transient_list, client_trans_for);
      
  list_enumerate(transient_list, list_item)
    {
      Client *cur = (Client *)list_item->data;
      
      if (flags && !(cur->flags & flags))
	continue;

      dbg("%s() moving %s to top (trans for %s)\n", 
	  __func__, cur->name, 
	  client_trans_for ? client_trans_for->name : "NULL" );

      stack_move_top(cur);
    }

  list_destroy(&transient_list);


#if 0
  Client *cur = NULL, *client_tmp = NULL, *fake_stack_top = NULL;

  cur = w->stack_bottom;

  if (!cur) return;

  do
    {
      /* cur->above will get trashed by stack_move_top() */
      client_tmp = cur->above; 
      if (cur->type == MBCLIENT_TYPE_DIALOG && cur->trans == client_trans_for)
	{
	  if (flags && !(cur->flags & flags))
	    continue;

	  stack_move_top(cur);
	  // cur->show(cur);
	  /* we never want to check above our initial raise */
	  if (!fake_stack_top) fake_stack_top = cur;
	}
      cur = client_tmp;
    }
  while (cur != fake_stack_top);
#endif

}

/* returns top -> bottom */
Window*
stack_get_window_list(Wm *w)
{
  Window *win_list;
  Client *c;
  int     i = 0;

  if (!w->stack_n_items) return NULL;

  win_list = malloc(sizeof(Window)*w->stack_n_items);

  dbg("%s() called, list is ", __func__);

  stack_enumerate_reverse(w, c)
  {
    dbg("%i:%li, ", i, c->frame);
    win_list[i++] = c->frame;
  }

  dbg("\n");

  return win_list;
}

/* returns the new highest wanted type */
Client*
stack_cycle_forward(Wm *w, MBClientTypeEnum type_to_cycle)
{
  Client *c = NULL;

  c = stack_get_highest(w, type_to_cycle);

  if (c) stack_move_below_type(c, type_to_cycle, 0);

  /* now return the new highest */
  return stack_get_highest(w, type_to_cycle);
}

/* returns the new highest wanted type */
Client*
stack_cycle_backward(Wm *w, MBClientTypeEnum type_to_cycle)
{
  Client *c = NULL;

  c = stack_get_lowest(w, type_to_cycle);

  if (c) stack_move_above_extended(c, NULL, type_to_cycle, 0);

  return c;
}

Client*
stack_get_highest(Wm *w, MBClientTypeEnum wanted_type)
{
  Client *c = NULL;

  stack_enumerate_reverse(w,c)
    if (c->type == wanted_type && c->mapped)
      return c;

  return NULL;
}



Client*
stack_get_lowest(Wm *w, MBClientTypeEnum wanted_type)
{
  Client *c = NULL;

  stack_enumerate(w,c)
    if (c->type == wanted_type && c->mapped)
      return c;

  return NULL;
}

Client*
stack_get_above(Client* client_below, MBClientTypeEnum wanted_type)
{
  Wm     *w = client_below->wm;
  Client *c = client_below->above;

  if (wanted_type == MBCLIENT_TYPE_ANY)
    return (client_below->above) ? client_below->above : w->stack_bottom; 

  while ( c != client_below )
    {
      if (c == NULL)
	c = w->stack_bottom;

      if (c->type == wanted_type && c->mapped)
	return c;

      c = c->above;
    }

  return client_below;
}

Client*
stack_get_below(Client*          client_above, 
		MBClientTypeEnum wanted_type)
{
  Wm     *w = client_above->wm;
  Client *c = client_above->below;

  /*
  if (wanted_type == MBCLIENT_TYPE_ANY)
    return (client_above->below) ? client_above->below : w->stack_top; 
  */

  while ( c != client_above )
    {
      if (c == NULL)
	c = w->stack_top;

      if (c->type == wanted_type && c->mapped)
	return c;

      c = c->below;
    }

  return client_above;
}




void
stack_dump(Wm *w)
{
  Client *c = NULL;

  printf("\n---------------------------------------------------------\n");

  printf("w->stack_top : %s, next %p   w->stack_bottom : %s, prev %p\n\n", 
	 w->stack_top->name, w->stack_top->above, 
	 w->stack_bottom->name, w->stack_bottom->below);

  stack_enumerate(w,c)
    {
      printf("%s\n", c->name);
    }


  printf("\n---------------------------------------------------------\n\n");
}

void
stack_sync_to_display(Wm *w)
{
  /*

    int XRestackWindows(Display *display, Window windows[], int nwindows);

    The XRestackWindows function restacks the windows in the order speci-
    fied, from top to bottom.  The stacking order of the first window in
    the windows array is unaffected, but the other windows in the array are
    stacked underneath the first window, in the order of the array.  The
    stacking order of the other windows is not affected.  For each window
    in the window array that is not a child of the specified window, a Bad-
    Match error results.


  */

  Window *win_list = stack_get_window_list(w);
  
  if (win_list)
    {
      XRestackWindows(w->dpy, win_list, w->stack_n_items);
      free(win_list);
    }

}

#if 0

/* Test bits for stack  */

int
main(int argc, char **argv)
{
  Wm *w;
  int i = 0; 
  Client *c, *midc, *midc2; 


  w = malloc(sizeof(Wm));
  memset(w, 0, sizeof(Wm));

  for (i=0; i<10; i++)
    {
      char    buf[64];

      sprintf(buf, "Client-%i", i);
      
      c = client_new(w, buf);

      stack_append_top(c);

      if (i == 5) midc = c;
      if (i == 6) midc2 = c;
    }

  printf("w->stack_top : %s, next %p   w->stack_bottom : %s, prev %p\n", 
	 w->stack_top->name, w->stack_top->above, 
	 w->stack_bottom->name, w->stack_bottom->below);


  stack_move_below_client(w->stack_top, w->stack_bottom);
  stack_move_above_client(w->stack_bottom, w->stack_top);

  printf("w->stack_top : %s, next %p   w->stack_bottom : %s, prev %p\n", 
	 w->stack_top->name, w->stack_top->above, 
	 w->stack_bottom->name, w->stack_bottom->below);


  stack_enumerate(w,c)
    {
      printf("%s\n", c->name);
    }

  printf("\n");


  printf("\n");



  stack_remove(w->stack_top);

  stack_remove(w->stack_bottom);



  stack_enumerate(w,c)
    {
      printf("%s\n", c->name);
    }

  stack_move_below_client(w->stack_top, w->stack_bottom);
  stack_move_above_client(midc, midc2);

  printf("\n");


  stack_enumerate(w,c)
    {
      printf("%s\n", c->name);
    }

}

#endif
