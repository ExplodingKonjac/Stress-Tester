#pragma once

#include <bits/stdc++.h>
#include <boost/process.hpp>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

namespace bp=boost::process;

struct RunnerResult
{
	enum Types{ OK=1,TLE,MLE,RE,KILLED };
	Types type;
	unsigned exit_code;
	std::size_t time_used,memory_used;
	RunnerResult();
	RunnerResult(Types _tp,unsigned _e,std::size_t _t,std::size_t _m);
};

class Runner
{
 private:
	std::string name,app;
	std::size_t tl,ml;
	std::string fin,fout,ferr;
	boost::process::child proc;
	std::thread watcher;
	RunnerResult res;

	void watching();

 public:
	Runner(const std::string &_name,const std::string &_app,std::size_t _tl,std::size_t _ml);
	~Runner();
	void setInputFile(const std::string &file);
	void setOutputFile(const std::string &file);
	void setErrorFile(const std::string &file);
	const std::string &getName();
	const RunnerResult &getLastResult();
	bool running();
	void start(const std::string &args="");
	void terminate();
	const RunnerResult &wait();
};