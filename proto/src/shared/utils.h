/* Utilities & standard top-level types
Copyright (C) 2005-2008, Jonathan Bachrach, Jacob Beal, and contributors 
listed in the AUTHORS file in the MIT Proto distribution's top directory.

This file is part of MIT Proto, and is distributed under the terms of
the GNU General Public License, with a linking exception, as described
in the file LICENSE in the MIT Proto distribution's top directory. */

#ifndef __UTILS__
#define __UTILS__

#include <stdlib.h> // also provides NULL
#include <string.h>
#include <queue>
#include <unistd.h>

/*****************************************************************************
 *  NUMBERS AND DIMENSIONS                                                   *
 *****************************************************************************/
// Numbers
#include <math.h>  // will makes the values INFINITY and M_PI available

#ifndef MIN
#define MIN(x,y) (((x)<(y))?(x):(y))
#define MAX(x,y) (((x)>(y))?(x):(y))
#define ABS(x)  ((x)<0?-(x):(x))
#endif
#define BOUND(x,y,z) MIN(MAX(x,y),z)

typedef float flo; // short name that jrb likes

// time and space measurement
typedef double SECONDS;
typedef flo METERS;

struct Rect { 
  METERS l,r,b,t; // right>left, top>bottom
  Rect(METERS left, METERS right, METERS bottom, METERS top) {
    l=left; r=right; b=bottom; t=top; 
  }
  virtual Rect* clone() { new Rect(l,r,b,t); }
  virtual int dimensions() { return 2; }
};
struct Rect3 : public Rect { 
  METERS f,c;   // ceiling>floor
  Rect3(METERS left, METERS right, METERS bottom, METERS top, METERS floor, 
	METERS ceiling) : Rect(left,right,bottom,top) {
    f=floor; c=ceiling;
  }
  virtual Rect* clone() { new Rect3(l,r,b,t,f,c); }
  virtual int dimensions() { return 3; }
};

// uniform random numbers
flo urnd(flo min, flo max);

// booleans

typedef int BOOL;
#ifndef TRUE 
#define TRUE (1) 
#define FALSE (0)
#endif

/*****************************************************************************
 *  SMALL MISC EXTENSIONS                                                    *
 *****************************************************************************/
// memory management, since we want to roll our own on some platforms
#ifndef MALLOC // avoid conflicts with MALLOC.H
extern void  *MALLOC(size_t size);
#endif
extern void  FREE(void *ptr);

// LISP-like null
//#define NULL ((void*)0)

// string buffers
#define MAX_STRBUF 1024
struct Strbuf {
  int idx; // pointer into the buffer; should be started at zero
  char data[MAX_STRBUF];
  Strbuf() { idx=0; }
};

BOOL str_is_number(char* str);
// these next three functions do not promise their values will remain
// past the next call to any of the three: they're for string construction
const char* bool2str(BOOL b);
const char* flo2str(float num, int precision=2);
const char* int2str(int num);

/*****************************************************************************
 *  NOTIFICATION FUNCTIONS                                                   *
 *****************************************************************************/
// (since we may want to change the "printf" behavior on some platforms)
extern "C" void uerror (char* message, ...);
extern "C" void debug(char* dstring, ...);
extern "C" void post(char* pstring, ...);
extern "C" void post_into(Strbuf *buf, char* pstring, ...);

/*****************************************************************************
 *  COMMAND LINE ARGUMENT SUPPORT                                            *
 *****************************************************************************/
// Makes it easy to have multiple routines that pull out switches during boot
class Args {
  int argp;  // Pointer to the current argument; 0 is the command, starts at 1
  char *last_switch; // last successfully found switch
 public:
  int argc;  // number of arguments
  char **argv; // pointers to argument strings

 public:
  Args(int argc, char** argv) { 
    argp=1; this->argc=argc; 
    this->argv=argv; 
    last_switch="(no switch yet)";
  }
  BOOL find_switch(char *sw); // tests if sw is in the list, leaves ptr there
  BOOL extract_switch(char *sw); // like find_switch, but deletes it if it is
  BOOL extract_switch(char *sw, BOOL warn); // allows safety to be disabled
  char* pop_next(); // removes the argument at the pointer and returns it
  char* peek_next(); // returns the argument at the pointer w/o removing
  double pop_number(); // like pop_next, but converts to number
  void goto_first(); // returns the pointer to the start of the arguments
  void remove(int i); // shrinks the list, deleting the ith argument
  void undefault(BOOL *value,char* pos,char* neg); // modify a default switch
};

/*****************************************************************************
 *  POPULATION                                                               *
 *****************************************************************************/
// A Population is a cross between an array and a list
// It is designed w. fast random access, compact storage, and automatic resize
class Population {
  int pop_size; // number of slots that are full
  int top; // next never-used slot
  int capacity;
  void** store;
  std::queue<int> removed;
 public:
  Population() { init_pop(10); }
  Population(int capacity) { init_pop(capacity); }
  ~Population() { free(store); }
  int add(void* item); // adds an item, returns where it went
  void* remove(int i); // removes the item at location i and returns i
  void destroy(int i); // idempotent freeing of item at location i
  void* get(int i);    // return the item at location i (NULL if empty)
  void clear();        // remove every item in the population
  int size() { return pop_size; }
  int max_id() { return top; }
 private:
  void init_pop(int cap); // 
  void resize_pop(int newcap); // resize the population
};

/*****************************************************************************
 *  GLUT-BASED EVENT MODEL                                                   *
 *****************************************************************************/
// Simple event model derived from GLUT
// The current mouse location in window coordinates
// [(0,0) in top-left corner, X increases rightward, Y increases downward]

//Older versions of GLUT do not support wheels
#ifndef GLUT_WHEEL_UP
# define GLUT_WHEEL_UP 3
#endif
#ifndef GLUT_WHEEL_DOWN
# define GLUT_WHEEL_DOWN 4
#endif

struct MouseEvent {
  int x;
  int y;
  int state; // 0=click, 1=drag start, 2=drag, 3=end drag, -1=drag failed
  BOOL shift; // only shift modifier, since CTRL and ALT select button on a Mac
  int button; // left, right, middle, or none (-1)?
  MouseEvent() { x=0; y=0; state=0; shift=FALSE; button=-1; }
};

// Warning: GLUT generates *different* numbers for control keys than normal
// keys.  Alphabetic keys give A=1, B=2, ... Z=26; Space is zero, others are
// not consistent across platforms.  Also, there is no case distinction.
// I do not know why this is, only that GLUT does this.
struct KeyEvent {
  BOOL normal; // is this a normal key or a GLUT special key?
  BOOL ctrl; // only CTRL: shift gives case, Macs mix ALT w. META and APPLE
  union {
    unsigned char key; // ordinary characters
    int special; // GLUT directionals and F-keys
  };
};

class EventConsumer {
public:
  virtual BOOL handle_key(KeyEvent* key) {return FALSE;} // return if consumed
  virtual BOOL handle_mouse(MouseEvent* mouse) {return FALSE;} // same return
  virtual void visualize() {} // draw, assuming a prepared OpenGL context
  // evolve moves state forward in time to 'limit' (an absolute)
  virtual BOOL evolve(SECONDS limit) {} // return whether state changed
};

#endif // __UTILS__