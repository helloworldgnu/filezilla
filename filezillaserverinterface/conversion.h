#ifndef __CONVERSION_H__
#define __CONVERSION_H__

#include <string>

CStdStringW ConvFromNetwork(const char* buffer);
std::string ConvToNetwork(const CStdStringW& str);

CStdStringA ConvToLocal(const CStdStringW& str);
CStdStringW ConvFromLocal(const CStdStringA& str);

#endif //__CONVERSION_H__