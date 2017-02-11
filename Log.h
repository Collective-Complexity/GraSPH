/*
 * mpUtils
 * mpLog.h
 *
 * @author: Hendrik Schwanekamp
 * @mail:   hendrik.schwanekamp@gmx.net
 *
 * Implements the Log class, which provides logging to text files, syslog, or custom streams
 *
 * Copyright 2016 Hendrik Schwanekamp
 *
 */

#ifndef MPUTILS_MPLOG_H
#define MPUTILS_MPLOG_H

// includes
//--------------------
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <memory>
#include <experimental/filesystem>
#include "mpUtils.h"

#ifdef __linux__
#include "SyslogStreambuf.h"
#endif
//--------------------

// defines
//--------------------

// macro to print file position
#define _mpu_mystr(x) _mpu_mystr2(x) //convert to string
#define _mpu_mystr2(x) #x
#define MPU_FILEPOS  __FILE__ " in line: "  _mpu_mystr(__LINE__)  " in function " + std::string(__PRETTY_FUNCTION__) // output file, line and function

// macros for simplified global logging

#define logFATAL_ERROR(MODULE) if(mpu::Log::noGlobal() || mpu::Log::getGlobal().getLogLevel() < mpu::LogLvl::FATAL_ERROR) ; \
                    else mpu::Log::getGlobal()(mpu::LogLvl::FATAL_ERROR, MPU_FILEPOS, MODULE)
#define logERROR(MODULE) if(mpu::Log::noGlobal() || mpu::Log::getGlobal().getLogLevel() < mpu::LogLvl::ERROR) ; \
                    else mpu::Log::getGlobal()(mpu::LogLvl::ERROR, MPU_FILEPOS, MODULE)
#define logWARNING(MODULE) if(mpu::Log::noGlobal() || mpu::Log::getGlobal().getLogLevel() < mpu::LogLvl::WARNING) ; \
                    else mpu::Log::getGlobal()(mpu::LogLvl::WARNING, MPU_FILEPOS, MODULE)
#define logINFO(MODULE) if(mpu::Log::noGlobal() || mpu::Log::getGlobal().getLogLevel() < mpu::LogLvl::INFO) ; \
                    else mpu::Log::getGlobal()(mpu::LogLvl::INFO, MPU_FILEPOS, MODULE)

// debug is disabled on release build
#ifdef NDEBUG
    #define logDEBUG(MODULE) if(false) mpu::Log::getGlobal()(mpu::LogLvl::DEBUG, MPU_FILEPOS, MODULE)
    #define logDEBUG2(MODULE) if(false) mpu::Log::getGlobal()(mpu::LogLvl::DEBUG2, MPU_FILEPOS, MODULE)
#else

    #define logDEBUG(MODULE) if(mpu::Log::noGlobal() || mpu::Log::getGlobal().getLogLevel() < mpu::LogLvl::DEBUG) ; \
                        else mpu::Log::getGlobal()(mpu::LogLvl::DEBUG, MPU_FILEPOS, MODULE)
    #define logDEBUG2(MODULE) if(mpu::Log::noGlobal() || mpu::Log::getGlobal().getLogLevel() < mpu::LogLvl::DEBUG2) ; \
                        else mpu::Log::getGlobal()(mpu::LogLvl::DEBUG2, MPU_FILEPOS, MODULE)
#endif
//--------------------

// namespace
//--------------------
namespace mpu {
//--------------------

// forward declarations
//--------------------
class LogStream;
//--------------------

//-------------------------------------------------------------------
/**
 * enum LogLvl
 * enum to specify the log level1
 */
enum LogLvl // enum to specify log level
{
    INVALID = 9999, // invalid is very high, so invalid messages are never logged
    ALL = 7,
    DEBUG2 = 6,
    DEBUG = 5,
    INFO = 4,
    WARNING = 3,
    ERROR = 2,
    FATAL_ERROR = 1,
    NOLOG = 0
};
extern const std::string LogLvlToString[]; // lookup to transform Loglvl to string
extern const std::string LogLvlStringInvalid; // lookup to transform Loglvl to string

//-------------------------------------------------------------------
/**
 * enum LogPolicy
 * enum to specify the log policy
 */
enum LogPolicy
{
    NONE = 0,
    CONSOLE = 1, //log to std out and err
    FILE = 2, // log to a text file
    SYSLOG = 3, // log to the linux syslog (only on linux)
    CUSTOM = 4 // specify your own stream to log to
};
extern const std::string LogPolicyToString[]; // loockup to transform LogPolicy to string

//-------------------------------------------------------------------
/**
 * @class Log
 * provides flexible formatted logging to stdout, files, the syslog, or custom streams.
 *
 * @usage:
 * To use the log create an object using one of the initialising constructors. you can also call the
 * empty constructor and initialise the log using one of the open(...) functions.
 * Choose a log policy from the enum above and provide either a file name, a custom stream, or a
 * Identity string and a facility if you want to use syslog on linux. (When using LogPolicy::CONSOLE
 * you do not have to provide anything else). When using a custom stream you are responsible for managing
 * the streams lifetime and ensure that it outlifes the log.
 * When using a file that already exists the log output is appended to the file. To prevent files growing into infinity
 * see the section on logrotation.
 *
 * You can set the global Log level with setLogLevel(). Only messages wih equal or higher priority will
 * be logged. To log a message you can use the "( ... )" function call operator and provide additional
 * parameters like the LogLevel of the mesage and then input text to the message using the "<<" operator.
 * The message is formatted and written to the Log automatically in a different thread.
 *
 * You can also use the logMessage(...) function to write unformatted strings to the log.
 * The logLevel Parameter n this case is not printed  to the log, but used to check if the Message
 * should be printed at all.
 *
 * additional options:
 * You can modify the timestamp format written to the file via setTimeFormat(string) where string is
 * the std library configuration string used for strftime.
 * You can give the "( .. )" - operator a second parameter which will be displayed at the end of a
 * message seperated by "@" e.g. You can use the MPU_FILEPOS macro defined above to get the current
 * file, line and function name.
 * You can also give a Module name as a third argument which is included in the message, this way
 * you can later grep for [MODULE_NAME] to filter the log by module.
 *
 * logrotation:
 * You can call setupLogrotate(size,numOfFiles) to tell the logger to rotate the log when the file is
 * getting larger then size bytes. numOfFiles old logfiles are beeing kept. When the log needs to be rotated,
 * the oldes file is deleted and all files are renamed file.4 -> file.5 file.3->file.4 and so on.
 * If numOfFiles is zero, the old log will simply be overwritten.
 *
 * the Global log:
 * There is one additional feature called the global log. you can make a log global using makeGlobal().
 * Note that there can only be one global log at any time. You can use the macro definitions above
 * to write message to the global log like logERROR << "An Error!" << endl
 * The first log created is always made global automatically.
 * All other classes of the mpu library will use the global log to output error messages.
 *
 * Thread safety:
 * The class was developed with the goal to allow logging from all threads at the same time, as a
 * result the class is totally thread save. Messages from different threads are printed line after line.
 * Also all parameters can safely be changed from different threads.
 *
 * exceptions:
 * If initialisation fails, the constructor or the open(...) function will throw an exception.
 * The default constructor will never throw.
 * No other exceptions are thrown.
 *
 */
class Log
{
public:
    // constructors
    Log(LogPolicy policy, LogLvl lvl);
    Log(LogPolicy policy = LogPolicy::NONE, const std::string &sFile = "", LogLvl lvl = LogLvl::INFO);
    Log(LogPolicy policy, std::ostream *out, LogLvl lvl = LogLvl::INFO);

#ifdef __linux__
    Log(LogPolicy policy, const std::string &sIdent, int iFacility, LogLvl lvl = LogLvl::INFO);
#endif

    ~Log(); // destructor

    // open function to open/reopen the log
    void open(LogPolicy policy, const std::string &sFile = "");
    void open(LogPolicy policy, std::ostream *out);

#ifdef __linux__
    void open(LogPolicy policy, const std::string &sIdent, int iFacility);
#endif

    void close(); // close the internal streams, is called automatically before open and on destruction

    void logMessage(const std::string &sMessage, LogLvl lvl); // logs a string as one message

    void setupLogrotate(std:: size_t maxFileSize, int numLogsToKeep = 1); // filesize in bytes

    // getter and setter
    void setLogLevel(LogLvl lvl) {logLvl = lvl;} // set the current log level
    LogLvl getLogLevel() const {return logLvl;} // get the current log level
    void setTimeFormat(const std::string sFormat) {std::lock_guard<std::mutex> lck(timeFormatMtx); sTimeFormat = sFormat;} // set the timestamp format (like strftime)
    std::string getTimeFormat() {std::lock_guard<std::mutex> lck(timeFormatMtx); return sTimeFormat;} // get the timestamp format
    LogPolicy getCurrentPolicy() const {return logPolicy;} // get the log policy
    void makeGlobal() {globalLog = this;}   // makes the current log global
    static Log &getGlobal() {return *globalLog;} // gets the global log
    static bool noGlobal() {return (globalLog == nullptr);} // checks if there is no global log set
    LogLvl getLastLvl() {return lastLvl;} // returns the log level of the message that is currently written

    // operators
    LogStream operator()(LogLvl lvl, std::string sFilepos ="", std::string sModule="");

    // make noncopyable and nonmoveable
    Log(const Log& that) = delete;
    Log& operator=(const Log& that) = delete;
    Log(Log &&that) = delete;
    Log& operator=(const Log&& that) = delete;

private:
    std::atomic<LogLvl> logLvl; // the log level
    std::atomic<LogLvl> lastLvl; // the log level of the message that is currently written
    std::string sTimeFormat; // format of the timestamp

    std::atomic<LogPolicy> logPolicy; // the log policy
    std::ostream *outStream; // ponits to the stream we print our log on
    std::unique_ptr<std::ostream> ownedStream; // here we put a stream if we own it
    std::unique_ptr<std::streambuf> ownedStreambuff; // here we put a custom streambuffer

    // for log rotation with files
    std::atomic<std::size_t> maxFileSize; // max file size before log is rotated
    std::atomic<int> iNumLogsToKeep; // number of old logs to keep
    std::string sLogfileName; // save the filename to manage old logfiles

    static Log* globalLog; // point this to the global log

    std::queue< std::pair<std::string,LogLvl>> messageQueue; // queue to collect messages from all threads

    // thread management
    std::mutex timeFormatMtx; // mutex to protect the time format
    std::mutex queueMtx;  // mutex to protect the queue
    std::mutex loggerMtx; // protect the logging operation
    std::condition_variable loggerCv; // cv to notify the logger when new messages arrive
    bool bShouldLoggerRun; // controle if the logger thread is running

    std::thread loggerMainThread; // the logger main thread
    void loggerMainfunc(); // the mainfunc of the second thread
};

// global functions
//--------------------
// toString overloads
inline const std::string &toString(LogLvl lvl)
{
    return (lvl < 0 || lvl > LogLvl::ALL)
           ? LogLvlStringInvalid : LogLvlToString[lvl];
}

inline const std::string &toString(LogPolicy policy)
{
    return LogPolicyToString[policy];
}
//--------------------

}

// include forward declared classes
//--------------------
#include "LogStream.h"
//--------------------

#endif //MPUTILS_MPLOG_H
