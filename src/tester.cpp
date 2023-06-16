#include "tester.h"

namespace Tester
{

void compileFiles()
{
	SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,true};
	HANDLE file=CreateFile("compile.log",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,nullptr);

	std::mutex locker;
	auto compileOne=[&](const std::string &name)
	{
		HANDLE hread,hwrite;
		CreatePipe(&hread,&hwrite,&sa,0);

		STARTUPINFO si;
		PROCESS_INFORMATION pi;
		initmem(si),initmem(pi);
		si.dwFlags|=STARTF_USESTDHANDLES;
		si.hStdError=hwrite;
		std::string cmd="g++ "+name+".cpp -o "+name+" "+opt.compile_opt;
		if(!CreateProcess(nullptr,const_cast<char*>(cmd.data()),nullptr,nullptr,true,0,nullptr,nullptr,&si,&pi))
		{
			locker.lock();
			quitError("Failed to create compiling process for %s.",name.c_str());
			locker.unlock();
		}
		DWORD ret;
		WaitForSingleObject(pi.hProcess,INFINITE);
		GetExitCodeProcess(pi.hProcess,&ret);
		CloseHandle(hwrite);

		locker.lock();
		std::string buf;
		DWORD len;
		if(ret==0)
		{
			buf=name+".cpp is successfully compiled.\n";
			WriteFile(file,buf.c_str(),buf.size(),&len,nullptr);
			printMessage("%s.cpp has been compiled.",name.c_str());
		}
		else
		{
			buf="Error message of "+name+":\n";
			WriteFile(file,buf.c_str(),buf.size(),&len,nullptr);
			buf=readFile(hread);
			WriteFile(file,buf.c_str(),buf.size(),&len,nullptr);
			quitFailed("Compilation error on %s.cpp. See compile.log for details.",name.c_str());
		}
		locker.unlock();

		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		CloseHandle(hread);
	};

	std::vector<std::thread> vec;
	vec.emplace_back(compileOne,opt.gen_name);
	vec.emplace_back(compileOne,opt.pro_name);
	vec.emplace_back(compileOne,opt.std_name);
	for(auto &i: vec) i.join();
}

int checkResult(Runner *run,bool ignore_re=false)
{
	auto &res=run->getLastResult();
	auto name=run->getName().c_str();
	int ret=0;
	switch(res.type)
	{
	 case RunnerResult::TLE:
		printColor(COLOR_YELLOW,"%s Time Limit Exceeded\n",name),ret=1;
		break;
	 case RunnerResult::MLE:
	 	printColor(COLOR_YELLOW,"%s Memory Limit Exceeded\n",name),ret=2;
		break;
	 case RunnerResult::RE:
		if(!ignore_re) printColor(COLOR_RED,"%s Runtime Error\n",name),ret=3;
		break;
	 case RunnerResult::KILLED:
		printColor(COLOR_PURPLE,"Terminated\n");
	 	ret=-1;
	 	break;
	 case RunnerResult::OK:
		break;
	}
	return ret;
}

void main()
{
	compileFiles();
	std::fputc('\n',stderr);

	Runner *gen_run=new Runner("Generator",opt.gen_name,opt.tl_gen,opt.ml_gen),
		   *chk_run=new Runner("Checker",opt.chk_name,opt.tl_chk,opt.ml_chk),
		   *pro_run=new Runner("Testee",opt.pro_name,opt.tl,opt.ml),
		   *std_run=new Runner("Standard",opt.std_name,opt.tl,opt.ml);
	gen_run->setOutputFile(opt.file+".in");
	pro_run->setInputFile(opt.file+".in");
	std_run->setInputFile(opt.file+".in");
	pro_run->setOutputFile(opt.file+".out");
	std_run->setOutputFile(opt.file+".ans");
	chk_run->setErrorFile(opt.file+".log");

	volatile bool force_quit=false;
	std::mutex mut;
	std::unique_lock<std::mutex> lk(mut);
	std::condition_variable cv;

	static std::function<void(int)> tryQuit;
	static auto p_tryQuit=[](int signum){ tryQuit(signum); };
	tryQuit=[&](int signum)
	{
		force_quit=true;
		pro_run->terminate();
		std_run->terminate();
		gen_run->terminate();
		chk_run->terminate();
		cv.wait(lk);
		std::string s;
		std::fprintf(stderr,"Quit testing? (y/n): ");
		std::getline(std::cin,s);
		force_quit=(s=="y" || s=="Y");
		cv.notify_all();
		std::signal(SIGINT,p_tryQuit);
	};
	std::signal(SIGINT,p_tryQuit);

	std::string chk_argu(opt.file+".in "+opt.file+".out "+opt.file+".ans");
	for(SIZE_T id=0;id<opt.test_cnt;id++)
	{
		std::fprintf(stderr,"Testcase #%lu: ",id);
		int lsterr=0;

		gen_run->start();
		gen_run->wait();
		if(checkResult(gen_run)) goto bad;
		pro_run->start();
		std_run->start();
		pro_run->wait();
		if(checkResult(pro_run)) goto bad;
		std_run->wait();
		if(checkResult(std_run)) goto bad;
		chk_run->start(chk_argu);
		chk_run->wait();
		if(checkResult(chk_run,true)) goto bad;

		if(chk_run->getLastResult().type==RunnerResult::RE)
		{
			std::string msg=readFile(openFile(opt.file+".log",'r'),256);
			if(msg.size()>=256) msg+="...";
			msg="Failed on testcase #"+std::to_string(id)+" (256 bytes only):\n"+msg+'\n';
			msg+="See "+opt.file+".log for detail.";
			printColor(COLOR_RED|FOREGROUND_INTENSITY,"Wrong Answer\n");
			MessageBox(nullptr,msg.c_str(),"Oops",MB_ICONERROR);
			break;
		}
		printColor(COLOR_GREEN|FOREGROUND_INTENSITY,"Accepted\n");
		if(!force_quit) continue;

		bad: if(force_quit)
		{
			cv.notify_all();
			cv.wait(lk);
			if(force_quit) break;
		}
		else break;
	}
	delete gen_run;
	delete pro_run;
	delete std_run;
	delete chk_run;
}

} // namespace Tester