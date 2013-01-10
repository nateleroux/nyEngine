#include "..\include.h"
#include "..\nyEngine.rc"

// The name of the log file to write to
CVar * g_logfile;

void dbgInit()
{
	file f;

	// Setup the version number
	// strlen("PRODUCTVERSION ") = 15
	memmove(versionNumber, versionNumber + 15, strlen(versionNumber) - 14);
	versionNumber[strlen(versionNumber) - 1] = 0;

	g_logfile = CVar::Create("g_logfile", "nyEngine.log", VAR_NOSYNC);

	// Use a file object to clear the log file
	if(f.openWrite(g_logfile->GetString()))
		f.close();
}

const char * dbgVersionNumber()
{
	return versionNumber;
}

void dbgLog(const char * str)
{
	const char * filename;
	file f;
	
	// Get the filename to log to
	filename = g_logfile->GetString();

	// Do nothing on a blank filename
	if(filename[0] == 0)
		return;

	// Because we use a var, a modified save file could be used to write the log anywhere
	// VAR_NOLOAD takes care of that
	// VAR_NOSYNC also takes care of servers attempting to push a log file location
	if(f.openWrite(filename, false, true))
	{
		// log to the file
		f.write(str, strlen(str));
		f.close();
	}
}

void dbgError(const char * format, ...)
{
	va_list va;
	char buf[0x200];
	int i;

	strcpy(buf, "ERROR: ");
	i = strlen(buf);

	_crt_va_start(va, format);
	vsnprintf(buf + i, 0x200 - i - 2, format, va);
	_crt_va_end(va);

	i = strlen(buf);
	buf[i] = '\n';
	buf[i + 1] = 0;

	dbgLog(buf);
	platformDebugOut(buf);
	platformFatal(buf);
}

void dbgOut(const char * format, ...)
{
	va_list va;
	char buf[0x200];
	int i;

	strcpy(buf, "MESSAGE: ");
	i = strlen(buf);

	_crt_va_start(va, format);
	vsnprintf(buf + i, 0x200 - i - 2, format, va);
	_crt_va_end(va);

	i = strlen(buf);
	buf[i] = '\n';
	buf[i + 1] = 0;

	dbgLog(buf);
	platformDebugOut(buf);
}