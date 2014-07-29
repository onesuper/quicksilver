#pragma once


struct Entry {  
  Entry *prev;
  Entry *next;

  virtual void print() {};
};

