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

#ifndef FileReaderH
#define FileReaderH

#include <string>
#include <vector>
#include <fstream>
#include <cassert>

#define FRBUFSIZE 10000

class FileReader
{
public:
   FileReader() {}
   ~FileReader();

   /// create and open file
   FileReader(const std::string &fileName) { clear(); loadFile(fileName); }

   /// open file and auto-parse
   void loadFile(const std::string &fileName);

   // access
   /// true, if end of file reached
   bool eof() { return mInStream.eof(); }
   bool next(); ///< go to next line, return "false" if end of file reached
   void reset(); ///< reset to first line

   /// number of columns
   size_t columnCount() const { return mColCount; }
   const std::string &columnName(const size_t columnIndex) { assert(columnIndex < mColCount); return mFields[columnIndex]; }

   /// retrieve the index of a given column or std::string::npos if the column is not found.
   size_t columnIndex(const char *columnName);
   const char *currentLine() const { return mBuffer; }

   double value(const size_t columnIndex) { assert(columnIndex < mColCount); return mValues[columnIndex]; }
   double value(const std::string &columnName) { return value(indexOf(columnName)); }

   std::string valueString(const size_t columnIndex);
   std::string valueString(const std::string &columnName) { return valueString(indexOf(columnName)); }

   /// retrieve the index of a column name. Throws logic_error if column not present.
   size_t indexOf(const std::string &columnName);

   /// check if *all* columns provided in 'cols' are in the file.
   bool requiredColumns(const std::vector<std::string> &cols);

private:
   void readHeader();
   void clear();
   size_t count_occ(const char* s, char c);

   char mBuffer[FRBUFSIZE];
   std::string mFileName;
   std::streampos mDataStart;
   std::vector<std::string> mFields;
   std::vector<double> mValues;
   std::ifstream mInStream;
   char mDelimiter;
   size_t mColCount{0};
};

#endif
