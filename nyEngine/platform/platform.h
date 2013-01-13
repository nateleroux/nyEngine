/*
Name: platform.h
Author: Nathan LeRoux
Purpose: Provides a platform-independant API so that this engine is easier to port
*/

#ifndef _PLATFORM_H
#define _PLATFORM_H

void platformDebugOut(const char * format);
void platformFatal(const char * error);

#endif