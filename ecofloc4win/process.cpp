#include "process.h"

process::process(string pid, string name)
{
	this->setPid(pid);
	this->setName(name);
}

string process::getPid()
{
	return this->m_pid;
}

string process::getName()
{
	return this->m_name;
}

void process::setPid(string pid)
{
	this->m_pid = pid;
}

void process::setName(string name)
{
	this->m_name = name;
}

/*void process::addProcPid()
{
	if (!getState)
	{
		setState(true);
		cout << "Addition of the process via the pid: " << getPid() << endl;
	}
	else
	{
		cout << "The process with the pid: " << getPid() << "is already added" << endl;
	}
}

void process::addProcName()
{
	if (!getState)
	{
		setState(true);
		cout << "Addition of the process via the name: " << getName() << endl;
	}
	else
	{
		cout << "The process with the name: " << getName() << "is already added" << endl;
	}
}

void process::removeProcPid()
{
	if (getState)
	{
		setState(false);
		cout << "The process with the pid " << getPid() << " has been removed" << endl;
	}
	else
	{
		cout << "The process with the pid: " << getPid() << "was not added" << endl;
	}
}

void process::removeProcName()
{
	if (getState)
	{
		setState(false);
		cout << "The process with the name " << getName() << " has been removed" << endl;
	}
	else
	{
		cout << "The process with the name " << getName() << "was not added" << endl;
	}
}*/