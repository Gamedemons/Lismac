#pragma once

namespace lance 
{
	// Application Utilities
	void writeFile(
		std::ofstream& outputFile, 
		char inputLocation[], 
		std::string seperator,
		bool enable_markdown,
		bool recursive_search,
		bool enable_fullpaths,
		int& tab_stack
	);

	std::vector<std::string> getFileNames(char path[]);

	long long getFileSize(std::string path);

	long getCurrentTime(char type);

	std::string getOutputFileName(char path[], char name[], bool enable_markdown);

	std::string extractName(std::string str, bool pathVisible);

	std::string doRegex(std::string str, std::string reg1, std::string reg2);

	std::string getTabs(int stack);

	// Type Conversions

	short int toShint(short int x);
	short int toShint(double x);
	float toFloat(double x);
	std::string handleUnicodeStrings(const std::wstring& wide_string);

	void increaseStack(int& stack);
	void decreaseStack(int& stack);


	void ltrim(std::string& s);
	void rtrim(std::string& s);
	void trim(std::string& s);
	bool isNumber(const std::string s);
}