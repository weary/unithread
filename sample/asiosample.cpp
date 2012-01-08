#define BOOST_ASIO_DISABLE_THREADS

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include "unithread.h"

using boost::asio::ip::tcp;

template<typename T>
std::string to_str(const T&t) { return boost::lexical_cast<std::string>(t); }


class session
{
public:
	session(boost::asio::io_service& io_service, unithread::launcher_t *launcher) :
		socket_(io_service), d_launcher(launcher)
	{ }

	tcp::socket& socket() { return socket_; }

	void start();

	void readline(std::string &s)
	{
		unithread::condition_t cond(d_launcher);
		d_threads.push_back(value_t(&cond, s));
		d_launcher->active_thread()->yield(cond);
		// by the time we return our string will be set
	}

	void write(const std::string &data)
	{
		d_towrite += data;
		if (d_nowwriting.empty())
			schedule_write();
	}

private:
	void handle_read(const boost::system::error_code& error, size_t bytes_transferred)
	{
		if (error)
		{
			printf("connection died in read: %s\n", to_str(error).c_str());
			delete this;
			return;
		}
		d_read.append(data_, bytes_transferred);
		size_t i = d_read.find('\n');
		if (i>=0)
		{
			std::string line = d_read.substr(0,i-1);
			d_read = d_read.substr(i+1);

			// find someone who needs a line
			if (d_threads.empty())
			{
				printf("no-one wants a line\n");
				return;
			}
			d_threads.front().second = line;
			d_threads.front().first->set();
			d_threads.pop_front();
		}
		schedule_read();
	}

	void handle_write(const boost::system::error_code& error)
	{
		if (error)
		{
			printf("connection died in write: %s\n", to_str(error).c_str());
			delete this;
			return;
		}
		d_nowwriting.clear();
		if (!d_towrite.empty())
			schedule_write();
	}

	void schedule_write()
	{
		d_nowwriting = d_towrite;
		d_towrite.clear();
		boost::asio::async_write(socket_,
				boost::asio::buffer(d_nowwriting.data(), d_nowwriting.size()),
				boost::bind(&session::handle_write, this, boost::asio::placeholders::error));
	}

	void schedule_read()
	{
		socket_.async_read_some(boost::asio::buffer(data_, max_length),
				boost::bind(&session::handle_read, this,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred));
	}

	tcp::socket socket_;
	enum { max_length = 1024 };
	char data_[max_length];

	std::string d_read;
	std::string d_nowwriting;
	std::string d_towrite;

	unithread::launcher_t *d_launcher;
	typedef std::pair<unithread::condition_t *, std::string &> value_t;
	std::list<value_t> d_threads;
};

class server
{
public:
	server(boost::asio::io_service& io_service, short port, unithread::launcher_t *launcher) :
		io_service_(io_service),
		acceptor_(io_service, tcp::endpoint(tcp::v4(), port)),
		d_launcher(launcher)
	{
		start_accept();
	}

private:
	void start_accept()
	{
		session* new_session = new session(io_service_, d_launcher);
		acceptor_.async_accept(new_session->socket(),
				boost::bind(&server::handle_accept, this, new_session,
					boost::asio::placeholders::error));
	}

	void handle_accept(session* new_session, const boost::system::error_code& error)
	{
		if (!error)
			new_session->start();
		else
			delete new_session;

		start_accept();
	}

	boost::asio::io_service& io_service_;
	tcp::acceptor acceptor_;
	unithread::launcher_t *d_launcher;
};

struct some_thread_t : public unithread::thread_t
{
	some_thread_t(unithread::launcher_t *launcher, int tid, session *ses) :
		unithread::thread_t(launcher, true, 8192),
		d_threadid(tid),
		d_session(ses)
	{}

	void run()
	{
		std::string line;
		d_session->readline(line);
		printf("some thread got line '%s'\n", line.c_str());
		d_session->write("echo from thread " + to_str(d_threadid) + ": '" + line + "'\n");
	}

	void died()
	{
		printf("some thread died\n");
		delete this;
	}

	int d_threadid;
	session *d_session;
};

struct main_thread_t : public unithread::thread_t
{
	main_thread_t(unithread::launcher_t *launcher) :
		unithread::thread_t(launcher, true, 8192)
	{}

	void run()
	{
		boost::asio::io_service io_service;
		server s(io_service, 2300, launcher());
		printf("going to listen on port 2300\n");

		while (1)
		{
			if (launcher()->have_inactive_threads())
				io_service.poll();
			else
				io_service.run_one();
			yield();
		}
	}

	void died()
	{
		printf("main thread died\n");
	}
};

void session::start()
{
	write("welcome!\n"); schedule_read();
	write("we launch 10 unithreads that will read one line, echo it and die\n");
	for (int n=0; n<10; ++n)
	{
		new some_thread_t(d_launcher, n, this);
	}
}

int main(int argc, char* argv[])
	try
{
	unithread::launcher_t d_launcher;
	main_thread_t thread(&d_launcher);
	d_launcher.start();


	return 0;
}
catch (std::exception& e)
{
	printf("Exception: %s\n", e.what());
}
