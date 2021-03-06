﻿/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef UTIL_LOGGER_H_
#define UTIL_LOGGER_H_

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <map>
#include <deque>
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <memory>
#include <mutex>
#include "Util/util.h"
#include "Thread/semaphore.h"

using namespace std;

namespace toolkit {

typedef enum { LTrace = 0, LDebug, LInfo, LWarn, LError} LogLevel;

class LogContext;
class LogChannel;
class LogWriter;
typedef std::shared_ptr<LogContext> LogContextPtr;

/**
 * 日志类
 */
class Logger : public std::enable_shared_from_this<Logger> , public noncopyable {
public:
    friend class AsyncLogWriter;
    friend class LogContextCapturer;
    typedef std::shared_ptr<Logger> Ptr;

    /**
     * 获取日志单例
     * @return
     */
    static Logger &Instance();

    /**
     * 废弃的接口，无实际操作
     * @deprecated
     */
    static void Destory(){};

    ~Logger();

    /**
     * 添加日志通道，非线程安全的
     * @param channel log通道
     */
    void add(const std::shared_ptr<LogChannel> &channel);

	/**
     * 删除日志通道，非线程安全的
     * @param name log通道名
     */
    void del(const string &name);

    /**
     * 获取日志通道，非线程安全的
     * @param name log通道名
     * @return 线程通道
     */
    std::shared_ptr<LogChannel> get(const string &name);

    /**
     * 设置写log器，非线程安全的
     * @param writer 写log器
     */
    void setWriter(const std::shared_ptr<LogWriter> &writer);

    /**
     * 设置所有日志通道的log等级
     * @param level log等级
     */
    void setLevel(LogLevel level);
private:
    Logger();
    void writeChannels(const LogContextPtr &stream);
    void write(const LogContextPtr &stream);
private:
    map<string, std::shared_ptr<LogChannel> > _channels;
    std::shared_ptr<LogWriter> _writer;
};

///////////////////LogContext///////////////////
/**
 * 日志上下文
 */
class LogContext : public ostringstream{
public:
    friend class LogContextCapturer;

    /**
     * 打印日志至输出流
     * @param ost 输出流
     * @param enableColor 是否请用颜色
     * @param enableDetail 是否打印细节(函数名、源码文件名、源码行)
     */
    void format(ostream &ost,bool enableColor = true, bool enableDetail = true) ;
    static std::string printTime(const timeval &tv);
public:
    LogLevel _level;
    int _line;
    string _file;
    string _function;
    timeval _tv;
private:
    LogContext(LogLevel level,const char *file,const char *function,int line);
};

/**
 * 日志上下文捕获器
 */
class LogContextCapturer {
public:
	typedef std::shared_ptr<LogContextCapturer> Ptr;
    LogContextCapturer(Logger &logger,LogLevel level, const char *file, const char *function, int line);
    LogContextCapturer(const LogContextCapturer &that);

    ~LogContextCapturer();

    /**
     * 输入std::endl(回车符)立即输出日志
     * @param f std::endl(回车符)
     * @return 自身引用
     */
	LogContextCapturer &operator << (ostream &(*f)(ostream &));

    template<typename T>
    LogContextCapturer &operator<<(T &&data) {
        if (!_logContext) {
            return *this;
        }
		(*_logContext) << std::forward<T>(data);
        return *this;
    }

    void clear();
private:
    LogContextPtr _logContext;
	Logger &_logger;
};


///////////////////LogWriter///////////////////
/**
 * 写日志器
 */
class LogWriter : public noncopyable {
public:
	LogWriter() {}
	virtual ~LogWriter() {}
	virtual void write(const LogContextPtr &stream) = 0;
};

class AsyncLogWriter : public LogWriter {
public:
    AsyncLogWriter(Logger &logger = Logger::Instance());
    ~AsyncLogWriter();
private:
    void run();
    void flushAll();
	void write(const LogContextPtr &stream) override ;
private:
    bool _exit_flag;
    std::shared_ptr<thread> _thread;
    deque<LogContextPtr> _pending;
    semaphore _sem;
    mutex _mutex;
    Logger &_logger;
};

///////////////////LogChannel///////////////////
/**
 * 日志通道
 */
class LogChannel : public noncopyable{
public:
	LogChannel(const string& name, LogLevel level = LDebug);
	virtual ~LogChannel();
	virtual void write(const LogContextPtr & stream) = 0;

	const string &name() const ;
	void setLevel(LogLevel level);
protected:
	string _name;
	LogLevel _level;
};

/**
 * 输出日志至终端，支持输出日志至android logcat
 */
class ConsoleChannel : public LogChannel {
public:
    ConsoleChannel(const string &name = "ConsoleChannel" , LogLevel level = LDebug) ;
    ~ConsoleChannel();
    void write(const LogContextPtr &logContext) override;
};

/**
 * 输出日志至文件
 */
class FileChannel : public LogChannel {
public:
    FileChannel(const string &name = "FileChannel",const string &path = exePath() + ".log", LogLevel level = LDebug);
    ~FileChannel();

    void write(const std::shared_ptr<LogContext> &stream) override;
    void setPath(const string &path);
    const string &path() const;
protected:
    virtual void open();
    virtual void close();
protected:
    ofstream _fstream;
    string _path;
};

#define TraceL LogContextCapturer(Logger::Instance(), LTrace, __FILE__,__FUNCTION__, __LINE__)
#define DebugL LogContextCapturer(Logger::Instance(),LDebug, __FILE__,__FUNCTION__, __LINE__)
#define InfoL LogContextCapturer(Logger::Instance(),LInfo, __FILE__,__FUNCTION__, __LINE__)
#define WarnL LogContextCapturer(Logger::Instance(),LWarn,__FILE__, __FUNCTION__, __LINE__)
#define ErrorL LogContextCapturer(Logger::Instance(),LError,__FILE__, __FUNCTION__, __LINE__)
#define WriteL(level) LogContextCapturer(Logger::Instance(),level,__FILE__, __FUNCTION__, __LINE__)


} /* namespace toolkit */

#endif /* UTIL_LOGGER_H_ */
