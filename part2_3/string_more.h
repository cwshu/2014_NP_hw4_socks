#ifndef __STRING_MORE_H__
#define __STRING_MORE_H__

#include <string>

std::string fetch_word(std::string& parsed_str, const char* split_chars);

const char WHITESPACE[] = " \t\r\n\v\f";
std::string lstrip(std::string str); /* strip left whitespaces */
std::string rstrip(std::string str); /* strip right whitespaces */
std::string strip(std::string str); /* strip left and right whitespaces */ 
#endif
