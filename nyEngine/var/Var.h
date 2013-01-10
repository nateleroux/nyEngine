#ifndef _VAR_H
#define _VAR_H

#define VAR_MAX_COUNT 1024 // The maximum amount of vars that can exist at any given time

#define VAR_READONLY	0x0001 // The value of this var can not be changed
#define VAR_RANGE		0x0002 // This var has a range of values
#define VAR_SCRIPT		0x0004 // This var was created by a script and will be destroyed upon map exit
#define VAR_MODIFIED	0x0008 // This var has been modified and will be flushed to disk in the save operation
#define VAR_NOSYNC		0x0010 // This var can not be synced over network
#define VAR_NOLOAD		0x0020 // This var can not be loaded from a savegame

class CVar
{
public:
	// Create a var
	static CVar * Create(const char * name, const char * value, uint flags = 0);
	static CVar * Create(const char * name, double value, uint flags = 0, double min = 0, double max = 0);
	static CVar * Create(const char * name, int value, uint flags = 0, int min = 0, int max = 0);
	static CVar * Create(const char * name, bool value, uint flags = 0);

	// Find a var, to be used by script operations
	static CVar * Find(const char * name);

	// Purge all script vars from memory
	static void Purge(bool all = false);
	// Load vars from disk
	static void LoadAllVars(file& f);
	// Save vars to disk
	static void SaveAllVars(file& f);

	// Dump the var list to the debug output
	static void DumpToDebug(bool sorted);

	// Set the value of this var
	void Set(const char * value);
	void Set(double value);
	void Set(int value);

	// Get the value of this var
	const char * GetString();
	double GetDouble();
	int GetInt();
	int GetBool(); // Same as GetInt, named GetBool so it looks nicer

	// Reset the value of this var to default (useful after changing maps, etc)
	void Reset();

	// Get the flags of this var
	uint GetFlags();
	// Set the flags of this var
	void SetFlags(uint flags);

	// Get the name of this var
	const string& Name() const;
	// Get the value of this var
	const string& Value() const;
	
private:
	CVar() { doModify = false; }
	~CVar() { }

	void UpdateValues();
	void SetRange(double value);

	string name;
	string value;
	string defaultValue;
	double valueDouble;
	uint flags;

	double min, max;
	bool doModify;
};

// Var naming convention
/*
	g_XXXX : Pertains to the game as a whole, examples would be the framerate, platform, is-online, etc
	sv_XXXX : Pertains to the server portion of the game, examples would be if cheats are enabled, the max player count, etc
	cv_XXXX : Pertains to the client portion of the game, examples would be the field of view, texture quality, etc
	scr_XXXX : Pertains to the scripts of the game, usually created by a script
*/

// Var saving
/*
	ushort total var count
	vars : total var count
	{
		string var name
		string var value
	}

*/

#endif