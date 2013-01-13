/*
Name: include.h
Author: Nathan LeRoux
Purpose: Define types, include files
*/

#ifndef _INCLUDE_H
#define _INCLUDE_H

typedef unsigned int uint32, uint;
typedef unsigned short uint16, ushort;
typedef unsigned char uint8, byte;
typedef signed int int32;
typedef signed short int16;
typedef signed char int8;

#ifndef SUPPRESS_DEBUG_NEW
#ifdef _DEBUG
#define new new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define _CRTDBG_MAP_ALLOC
#endif
#endif

// How many physics/script ticks are in a second
#define TICKS_PER_SECOND 60

#include "warn.h"

#include <iostream>
#include "util\file.h"
#include "util\string.h"
#include "util\lock.h"
#include "platform\platform.h"
#include "dbg\dbg.h"
#include "var\Var.h"
#include "map\map.h"

#endif