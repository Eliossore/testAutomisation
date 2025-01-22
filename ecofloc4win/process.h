#pragma once
#include <iostream>
using namespace std;
#include <vector>
#include <string>
#include <sstream>
#include <list>

class process
{
private:
    string m_pid;
    string m_name;

public:
    process(string, string);
    string getPid();
    string getName();
    void setPid(string);
    void setName(string);
    /*void addProcPid();
    void addProcName();
    void removeProcPid();
    void removeProcName();*/
};

