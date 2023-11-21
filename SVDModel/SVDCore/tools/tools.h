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
#ifndef TOOLS_H
#define TOOLS_H
#include <string>
#include <vector>
#include "spdlog/spdlog.h"

class Settings; // forward

class Tools
{
public:
    Tools();
    /// returns 'true' if the file exists, false otherwise
    static bool fileExists(const std::string &fileName);
    /// returns the full path to 'fileName'. If a relative path if provided,
    /// it is resolved relative to the project directory
    static std::string path(const std::string &fileName);
    /// get file size (bytes), -1 if file does not exists
    static int fileSize(const std::string &fileName);


    // maintenance
    static void setupPaths(const std::string &path, const Settings *settings);
private:
    static std::string mProjectDir;
    static std::vector< std::pair<std::string, std::string > > mPathReplace;
};

class STimer {
public:
    STimer(std::shared_ptr<spdlog::logger> logger, std::string name, bool log_on_destroy=true) { start_time = std::chrono::system_clock::now(); _logger=logger; _name=name; _log_on_destroy = log_on_destroy; }
    void reset() { start_time = std::chrono::system_clock::now(); }
    size_t elapsed() { return static_cast<size_t>( std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start_time).count() ); }
    std::string elapsedStr() {
        auto t = elapsed();
        if (t<10000.)
            return std::to_string(t) + "µs";
        else
            return (std::to_string(round(t / 1000.) / 1000.)) + "s";
    }
    void print(std::string s) { _logger->debug("[{}] Timer {}: {}: {}",
                                               std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::system_clock::now()).time_since_epoch().count(),
                                               _name, s, elapsedStr()) ; }
    void now() { _logger->debug("Timepoint: {}us", std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() ));}
    ~STimer() { if (_log_on_destroy) print("finished"); }
private:
    std::shared_ptr<spdlog::logger> _logger;
    std::string _name;
    std::chrono::system_clock::time_point start_time;
    bool _log_on_destroy { true };
};

#endif // TOOLS_H
