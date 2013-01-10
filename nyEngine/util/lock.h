#ifndef _LOCK_H
#define _LOCK_H

class lock
{
public:
	lock();
	~lock();

	void enter();
	bool tryenter(int spinCount = 1);
	void leave();

private:
	void * data; // platform specific lock data
};

#endif