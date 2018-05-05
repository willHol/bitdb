#pragma once
#include <stdbool.h>
#include <sys/types.h>

typedef struct dl_node
{
  void* elem;
  struct dl_node* prev;
  struct dl_node* next;
} dl_node;

typedef struct dl_list
{
  bool do_alloc;
  size_t num_elems;
  size_t elem_size;
  struct dl_node* dummy;
  struct dl_node* head;
  struct dl_node* tail;
} dl_list;

int
dl_list_init(dl_list* list, size_t elem_size, bool do_alloc);

int
dl_list_get(dl_list* list, size_t i, void** out);

int
dl_list_set(dl_list* list, size_t i, void* x);

int
dl_list_add(dl_list* list, size_t i, void* x);

int
dl_list_remove(dl_list* list, size_t i, void** x);

int
dl_list_push(dl_list* list, void* x);

int
dl_list_pop(dl_list* list, void** x);

int
dl_list_stack_peek(dl_list* list, void** x);

int
dl_list_enqueue(dl_list* list, void* x);

int
dl_list_dequeue(dl_list* list, void** x);

int
dl_list_queue_peek(dl_list* list, void** x);

int
dl_list_destroy(dl_list* list);
