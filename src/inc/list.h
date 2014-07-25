#pragma once

#include <stdio.h>
#include "entry.h"


/*
 * A general double circular linked list used to main the global order
 */
class List {

private:
  Entry *_head;

public:
  List():
    _head(NULL)
    {}

  void insertTail(Entry *entry) {
    // The only entry in the list, elect him to be the head
    if (_head == NULL) {
      _head = entry;
      entry->prev = entry;
      entry->next = entry;
    } else {
      Entry *tail = _head->prev;

      // insert the entry into the chain
      tail->next = entry;
      _head->prev = entry;

      // connect the entry's pointer
      entry->prev = tail;
      entry->next = _head;
    }
    return;
  }


  void remove(Entry *entry) {

    // Never remove a emty list
    assert(_head != NULL);

    Entry *prev = entry->prev;
    Entry *next = entry->next;

    // The only entry in the list, removing it is remove all
    if (prev == entry && next == entry) {
      _head = NULL;
    } else {
      prev->next = next;
      next->prev = prev;
      // If we are removing the head entry, choose the next to be the new head
      if (_head == entry) {
        _head = next;      
      }
    }
  }

  void print(void) {

    Entry *entry = _head;
    
    do {
      entry->print();
      entry = entry->next;
    } while (entry != _head); // Travel around

    return;
  }
  
};
