#ifndef _DBG_H
#define _DBG_H

void dbgInit();
const char * dbgVersionNumber();
void dbgError(const char * format, ...);
void dbgOut(const char * format, ...);

#endif