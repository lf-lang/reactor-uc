#ifndef REACTOR_UC_REACTOR_ELEMENT_H
#define REACTOR_UC_REACTOR_ELEMENT_H

typedef enum { ACTION, TIMER, INPUT, OUTPUT, CONN_DELAYED, CONN_PHYSICAL, CONN_LOGICAL } ElementType;

typedef struct {
  ElementType type;
} ReactorElement;

#endif