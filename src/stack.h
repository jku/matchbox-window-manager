#ifndef _HAVE_STACK_H_
#define _HAVE_STACK_H_

#include "structs.h"
#include "wm.h"
#include "config.h"

#define stack_enumerate(w,c)                               \
 if ((w)->stack_bottom)                                    \
   for ((c)=(w)->stack_bottom; (c) != NULL; (c)=(c)->next) 

#define stack_enumerate_reverse(w,c)                       \
 if ((w)->stack_top)                                       \
   for ((c)=(w)->stack_top; (c) != NULL; (c)=(c)->prev) 

#define stack_enumerate_transients(w,c,t)                  \
 if ((w)->stack_bottom)                                    \
   for ((c)=(w)->stack_bottom; (c) != NULL; (c)=(c)->next) \
     if ((c)->trans == (t))

#define stack_add_top(c) \
 stack_move_above_client((c), (c)->wm->stack_top)

#define stack_add_bottom(c) \
 stack_move_below_client((c), (c)->wm->stack_bottom)



void
stack_add_below_client(Client *client, Client *client_above);

void
stack_add_above_client(Client *client, Client *client_below);

void 
stack_append_top(Client *client);

void 
stack_prepend_bottom(Client *client);

void
stack_remove(Client *client);

void
stack_update_transients(Client *client);

void
stack_move_above_type(Client *client, int type, int flags);

void
stack_move_below_type(Client *client, int type, int flags);

void
stack_move_below_client(Client *client, Client *client_above);

void
stack_move_above_client(Client *client, Client *client_below);

Window*
stack_get_window_list(Wm *w);

void
stack_sync_to_display(Wm *w);

#endif
