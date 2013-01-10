#include "..\include.h"

CVar * varList[VAR_MAX_COUNT] = {0};
CVar * sortedVarList[VAR_MAX_COUNT] = {0};
int varListCount = 0;
lock varLock;

void varAddList(CVar * var)
{
	int i, j;
	int cmp;

	varLock.enter();
	for(i = 0;i < VAR_MAX_COUNT;i++)
	{
		if(varList[i])
		{
			if(i == VAR_MAX_COUNT - 1)
				dbgError("too many vars (max '%i')", VAR_MAX_COUNT);
			continue;
		}

		varList[i] = var;
		varListCount++;
		break;
	}

	for(i = 0;i < VAR_MAX_COUNT;i++)
	{
		if(sortedVarList[i])
		{
			cmp = var->Name().compare(sortedVarList[i]->Name());
			if(cmp == 0)
				dbgError("duplicate var '%s'", var->Name());

			if(cmp > 0)
				continue;
		}

		// Push the other vars back
		for(j = VAR_MAX_COUNT - 1;j > i;j--)
			sortedVarList[j] = sortedVarList[j - 1];

		// Put ourselves in the correct spot
		sortedVarList[i] = var;

		break;
	}

	varLock.leave();
}

CVar * CVar::Create(const char * name, const char * value, uint flags)
{
	CVar * var = new CVar();

	var->name = name;
	var->flags = flags;

	var->Set(value);

	varAddList(var);
	
	return var;
}

CVar * CVar::Create(const char * name, double value, uint flags, double min, double max)
{
	CVar * var = new CVar();

	var->name = name;
	var->min = min;
	var->max = max;
	if(min != max)
		var->flags = flags | VAR_RANGE;
	else
		var->flags = flags;

	var->Set(value);

	varAddList(var);
	
	return var;
}

CVar * CVar::Create(const char * name, int value, uint flags, int min, int max)
{
	return Create(name, (double)value, flags, (double)min, (double)max);
}

CVar * CVar::Create(const char * name, bool value, uint flags)
{
	CVar * var = new CVar();

	var->name = name;
	var->flags = flags;

	var->Set(value ? "true" : "false");

	varAddList(var);
	
	return var;
}

CVar * CVar::Find(const char * name)
{
	int i, cmp;
	CVar * var = NULL;

	for(i = 0;i < VAR_MAX_COUNT;i++)
	{
		if(!sortedVarList[i])
			break;

		cmp = sortedVarList[i]->Name().compare(name);
		if(cmp > 0)
			break;

		if(cmp < 0)
			continue;

		var = sortedVarList[i];
		break;
	}

	if(var == NULL)
	{
		// Unable to find the named var, so create it with default values
		// As any vars that would be referenced in the source would be referenced directly,
		//   this must be a script created var
		// All script created vars must be flushed to disk as well
		var = Create(name, "", VAR_SCRIPT | VAR_MODIFIED);
	}

	return var;
}

void CVar::Purge(bool all)
{
	int i, j;

	// First we clean up the standard list
	for(i = 0;i < VAR_MAX_COUNT;i++)
	{
		if(varList[i] && (all || (varList[i]->GetFlags() & VAR_SCRIPT)))
		{
			varList[i] = NULL;
			varListCount--;
		}
	}

	i = 0;

	// >Mission: Kill all script vars
	while(i < VAR_MAX_COUNT)
	{
		if(!sortedVarList[i])
			break;

		if(all || (sortedVarList[i]->GetFlags() & VAR_SCRIPT))
		{
			delete sortedVarList[i];
			sortedVarList[i] = NULL;

			for(j = i;j < VAR_MAX_COUNT - 1;j++)
				sortedVarList[j] = sortedVarList[j + 1];

			sortedVarList[VAR_MAX_COUNT] = NULL;
		}
		else
			i++;
	}
}

void CVar::LoadAllVars(file& f)
{
	ushort s;
	CVar * var;
	string name, value;

	// Read the length
	s = f.readuint16();

	// Read in the new vars
	while(s)
	{
		name.load(f);
		value.load(f);

		var = Find(name.c_str());
		if(var->flags & VAR_NOLOAD)
			dbgError("var '%s' can not be loaded", var->name);
		var->Set(value.c_str());

		s--;
	}
}

void CVar::SaveAllVars(file& f)
{
	int i, len;
	CVar * var;

	// Write the length
	len = 0;
	for(i = 0;i < VAR_MAX_COUNT;i++)
	{
		if(sortedVarList[i] && (sortedVarList[i]->GetFlags() & VAR_MODIFIED))
			len++;
	}
	f.write((ushort)len);

	// Write each modified var
	for(i = 0;i < VAR_MAX_COUNT;i++)
	{
		if(!sortedVarList[i])
			break;

		var = sortedVarList[i];

		if(!(var->GetFlags() & VAR_MODIFIED))
			continue;

		var->Name().save(f);
		var->Value().save(f);
	}
}

void CVar::DumpToDebug(bool sorted)
{
	int i;
	CVar ** list;

	if(sorted)
		list = sortedVarList;
	else
		list = varList;

	dbgOut("Begin%s var dump", sorted ? " sorted" : "");
	dbgOut("--------------------");

	for(i = 0;i < VAR_MAX_COUNT;i++)
	{
		if(!list[i])
			continue;

		dbgOut("'%s':'%s'", list[i]->Name().c_str(), list[i]->Value().c_str());
	}

	dbgOut("--------------------");
	dbgOut("End%s var dump", sorted ? " sorted" : "");
}

void CVar::Set(const char * value)
{
	if(flags & VAR_RANGE)
		SetRange(atoi(value));
	else
		this->value = value;
	
	UpdateValues();

	if(!doModify)
	{
		defaultValue = this->value;
		doModify = true;
	}
	else
		flags |= VAR_MODIFIED;
}

void CVar::Set(double value)
{
	if(flags & VAR_RANGE)
		SetRange(value);
	else
		this->value = value;

	UpdateValues();

	if(!doModify)
	{
		defaultValue = this->value;
		doModify = true;
	}
	else
		flags |= VAR_MODIFIED;
}

void CVar::Set(int value)
{
	Set((double)value);
}

const char * CVar::GetString()
{
	return value.c_str();
}

double CVar::GetDouble()
{
	return valueDouble;
}

int CVar::GetInt()
{
	return (int)valueDouble;
}

int CVar::GetBool()
{
	return (int)valueDouble;
}

void CVar::Reset()
{
	value = defaultValue;
	UpdateValues();

	// As we are resetting the values to default, the modified flag no longer applies if we are not a script var
	if(!(flags & VAR_SCRIPT))
		flags &= ~VAR_MODIFIED;
}

uint CVar::GetFlags()
{
	return flags;
}

void CVar::SetFlags(uint flags)
{
	this->flags = flags;
}

const string& CVar::Name() const
{
	return name;
}

const string& CVar::Value() const
{
	return value;
}

void CVar::UpdateValues()
{
	this->valueDouble = this->value.asDouble();
}

void CVar::SetRange(double value)
{
	if(value < min)
		this->value = min;
	else if(value > max)
		this->value = max;
	else
		this->value = value;
}
