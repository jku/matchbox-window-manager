#include "list.h"

struct list_item*
list_new(int id, char *name, void *data)
{
  struct list_item* list;
  list = malloc(sizeof(struct list_item));
  memset(list, 0, sizeof(struct list_item));

  if (name) list->name = strdup(name);
  if (id)   list->id   = id;
  list->data = data;
  list->next = NULL;

  return list;
}

void*
list_add(struct list_item** head, char *name, int id, void *data)
{
  struct list_item* tmp = *head;

  if (tmp == NULL) 
    {
      *head = list_new(id, name, data);
      // printf("added new list\n");
      return NULL;
    }
  
  while (tmp->next != NULL) tmp = tmp->next;
  tmp->next = list_new(id, name, data);

  return tmp->next->data;
}


void*
list_find_by_id(struct list_item* head, int needed_id)
{
  struct list_item* tmp = head;
  
  //dbg("button finding id %i\n", needed_id);

  while (tmp != NULL) 
    {
      //dbg("%s button checking %i vs %i\n", __func__, tmp->id, needed_id);
      if (tmp->id && tmp->id == needed_id) return tmp->data;
      tmp = tmp->next;
    }

  // printf("failed to id %i\n", needed_id);

  return NULL;
}

void*
list_find_by_name(struct list_item* head, char *name)
{
  struct list_item* tmp = head;
  
  // printf("finding %s\n", name);

  while (tmp != NULL) 
    {
      if (tmp->name && name && !strcmp(tmp->name, name)) return tmp->data;
      tmp = tmp->next;
    }
  // printf("failed to find %s\n", name);

  return NULL;
}


void
list_destroy(struct list_item** head)
{
  struct list_item* next = NULL, *cur = *head;
  while (cur != NULL)
    {
      next = cur->next;
      if (cur->name) free (cur->name);
      free(cur);
      cur = next;
    }
  *head = NULL;
}
  
