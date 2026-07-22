/********************************************************************************************
**    SVD - the scalable vegetation dynamics model
**    https://github.com/SVDmodel/SVD
**    Copyright (C) 2018-  Werner Rammer, Rupert Seidl
**
**    This program is free software: you can redistribute it and/or modify
**    it under the terms of the GNU General Public License as published by
**    the Free Software Foundation, either version 3 of the License, or
**    (at your option) any later version.
**
**    This program is distributed in the hope that it will be useful,
**    but WITHOUT ANY WARRANTY; without even the implied warranty of
**    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**    GNU General Public License for more details.
**
**    You should have received a copy of the GNU General Public License
**    along with this program.  If not, see <http://www.gnu.org/licenses/>.
********************************************************************************************/

#include "filereader.h"
#include "strtools.h"

#include <string>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <algorithm>

// helper: extract tokens from string 'str' and fill a vector with them.
static void tokenize(const std::string& str,
                     std::vector<std::string>& tokens,
                     const char delimiters = ' ')
{
    std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
    std::string::size_type pos     = str.find_first_of(delimiters, lastPos);

    while (std::string::npos != pos || std::string::npos != lastPos)
    {
        tokens.push_back(str.substr(lastPos, pos - lastPos));
        lastPos = str.find_first_not_of(delimiters, pos);
        pos     = str.find_first_of(delimiters, lastPos);
    }
}

FileReader::~FileReader()
{
   clear();
}

void FileReader::clear()
{
    mFields.clear();
    mValues.clear();
    if (mInStream.is_open())
         mInStream.close();
    mInStream.clear();
    mColCount = 0;
}

void FileReader::loadFile(const std::string &fileName)
{
    clear();
    mFileName = fileName;
    mInStream.open(fileName.c_str(), std::ifstream::in);
    if (!mInStream.is_open())
        throw std::logic_error("FileReader:: cannot open file: " + fileName);

    bool isFirst = true;
    while (!mInStream.eof()) {
       mInStream.getline(mBuffer, FRBUFSIZE);
       if (isFirst) {
           isFirst = false;
           if (strncmp(mBuffer, "\xEF\xBB\xBF", 3) == 0) {
               memmove(mBuffer, mBuffer + 3, strlen(mBuffer + 3) + 1);
           }
       }
       if (mBuffer[0] != '#' && *mBuffer != '\0') // skip comments and empty lines
           break;
    }
    if (mInStream.eof())
        throw std::logic_error("FileReader:: file contains no data: " + fileName);

    readHeader();
    mDataStart = mInStream.tellg();
}

void FileReader::readHeader()
{
    size_t ctab = count_occ(mBuffer, '\t');
    size_t csemi = count_occ(mBuffer, ';');
    size_t ccol = count_occ(mBuffer, ',');
    size_t cspc = count_occ(mBuffer, ' ');
    size_t maxc = std::max(std::max(ctab, csemi), std::max(ccol, cspc));
    if (maxc == 0)
        throw std::logic_error("FileReader:: cannot determine delimiter in " + mFileName);
    if (ctab == maxc) mDelimiter = '\t';
    if (csemi == maxc) mDelimiter = ';';
    if (ccol == maxc) mDelimiter = ',';
    if (cspc == maxc) mDelimiter = ' ';

    mFields.clear();
    tokenize(std::string(mBuffer), mFields, mDelimiter);

    mColCount = mFields.size();
    mValues.assign(mColCount, 0.0);

    for (size_t i = 0; i < mFields.size(); ++i) {
        mFields[i] = unquote(mFields[i]);
    }
}

size_t FileReader::count_occ(const char* s, char c)
{
  size_t n = 0;
  while (*s) {
    if (*s++ == c)
        n++;
  }
  return n;
}

size_t FileReader::indexOf(const std::string &columnName)
{
    auto it = std::find(mFields.begin(), mFields.end(), columnName);
    if (it == mFields.end())
        throw std::logic_error("FileReader:: invalid column: " + columnName + "\nin: " + mFileName);
    return (it - mFields.begin());
}

bool FileReader::requiredColumns(const std::vector<std::string> &cols)
{
    std::string msg;
    for (const auto &s : cols)
        if (!contains(mFields, s))
            msg += s + ", ";
    if (msg.empty())
        return true;
    throw std::logic_error("Required column(s) not in File '" + mFileName + "': " + msg + " (required are: " + join(cols) + ")");
}

size_t FileReader::columnIndex(const char *columnName)
{
    auto it = std::find(mFields.begin(), mFields.end(), std::string(columnName));
    if (it == mFields.end())
        return std::string::npos;
    return (it - mFields.begin());
}

void FileReader::reset()
{
    if (mInStream.is_open()) {
        mInStream.clear();
        mInStream.seekg(mDataStart);
    }
}

bool FileReader::next()
{
    if (eof())
        return false;

    size_t line_len = 0;
    while (!eof()) {
        mInStream.getline(mBuffer, FRBUFSIZE);
        if ((line_len = strlen(mBuffer)) > 0)
            break;
    }
    if (line_len == 0)
       return false;

    char dsp[2] = "\0";
    dsp[0] = mDelimiter;

    char *p = mBuffer;
    while (*p && (*p == mDelimiter || *p == ' ')) p++;

    for (size_t i = 0; i < mColCount; ++i) {
        if (!*p || *p == mDelimiter) {
            mValues[i] = 0.0;
        } else {
            char *p_start = p;
            while (*p && !strchr(dsp, *p)) p++;
            std::string tok(p_start, p - p_start);
            tok = unquote(tok);

            if (tok.empty()) {
                mValues[i] = 0.0;
            } else {
                mValues[i] = atof(tok.c_str());
            }
        }
        if (*p == mDelimiter) p++;
        while (*p && *p == ' ') p++;
    }
    return true;
}

std::string FileReader::valueString(const size_t columnIndex)
{
    int dfound = 0;
    char *p = mBuffer;
    char *ps = nullptr;
    while (*p) {
        if (*p == mDelimiter) {
            dfound++;
        }
        if (dfound == columnIndex) {
            if (*p == mDelimiter)
                ++p;
            ps = p;

            while (*p && *p != mDelimiter) p++;
            if (!*p) {
                if (columnIndex < mColCount - 1)
                    throw std::logic_error("FileReader::valueString not enough values.\nError at line:" + std::string(mBuffer) + "\nin:" + mFileName);
                std::string s(ps);
                return unquote(s);
            }
            std::string s(ps, p - ps);
            return unquote(s);
        }
        ++p;
    }
    throw std::logic_error("FileReader::valueString not enough values.\nError at line:" + std::string(mBuffer) + "\nin:" + mFileName);
}
