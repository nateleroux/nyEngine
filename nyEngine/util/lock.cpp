/*
Name: lock.cpp
Author: Nathan LeRoux
Purpose: Thread synchronization
*/

#include "..\include.h"

#ifdef _WIN32
#include <Windows.h>

lock::lock()
{
	data = (PCRITICAL_SECTION)malloc(sizeof(CRITICAL_SECTION));
	InitializeCriticalSection((PCRITICAL_SECTION)data);
}

lock::~lock()
{
	DeleteCriticalSection((PCRITICAL_SECTION)data);
	free(data);
}

void lock::enter()
{
	EnterCriticalSection((PCRITICAL_SECTION)data);
}

bool lock::tryenter(int spinCount)
{
	while(spinCount > 0)
	{
		if(TryEnterCriticalSection((PCRITICAL_SECTION)data))
			return true;
		else
			spinCount--;
	}

	return false;
}

void lock::leave()
{
	LeaveCriticalSection((PCRITICAL_SECTION)data);
}
#endif