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
    /**
	* @brief Constructor for the process class.
    * 
	* @param pid The process ID.
	* @param name The process name.
    */
    process(string pid, string name);

	/**
	* @brief Getter for the process ID.
    * 
	* @return string The process ID.
    */
    string getPid() const;

    /**
	* @brief Getter for the process name.
    * 
	* @return string The process name.
    */
    string getName();

	/**
	* @brief Setter for the process ID.
    */
    void setPid(string);

	/**
	* @brief Setter for the process name.
    */ 
    void setName(string);
};

