// =====================================================================================
// 
//       Filename:  smtpsend.hpp
//        Version:  1.0
//        Created:  05.12.2012 11:15:58
//         Author:  selivanov
// =====================================================================================
// 
//
// Добавить авторизацию
// Добавить резольвилку MX (отправка без указания сервера)
#pragma once

#include <string>
#include <vector>
#include <stdexcept>

#include <boost/lexical_cast.hpp>
#include <boost/interprocess/detail/atomic.hpp>
#include <ace/SOCK_Stream.h>

namespace transport
{

class temporary_error : public std::runtime_error
{
public:
    explicit temporary_error(const std::string& msg)
        : std::runtime_error("Temporary error [" + msg + "]")
    {
    }
};

class permanent_error : public std::runtime_error
{
public:
    explicit permanent_error(const std::string& msg)
        : std::runtime_error("Permanent error [" + msg + "]")
    {
    }

    explicit permanent_error(const std::string& cmd, const std::string& retmsg)
        : std::runtime_error("Permanent error [" + cmd + "] [" + retmsg + "]")
    {}
    explicit permanent_error(const std::string& cmd, const std::string& arg, const std::string& retmsg)
        : std::runtime_error("Permanent error [" + cmd + " " + arg + "] [" + retmsg + "]")
    {}
};

class resolve_error : public permanent_error
{
public:
    explicit resolve_error(const std::string& msg)
        : permanent_error("Resolve error [" + msg + "]")
    {
    }
    explicit resolve_error(const std::string& host, int port)
        : permanent_error("Can't resolve " + host + ":"+ boost::lexical_cast<std::string>(port))
    {}
};

class connect_exception : public temporary_error
{
    public:
        explicit connect_exception() 
            : temporary_error("Connection not established")
        {}
        explicit connect_exception(const std::string& host, int port) 
            : temporary_error("Can't connect to " + host + ":"+ boost::lexical_cast<std::string>(port))
        {}
};

class transport_error : public temporary_error
{
    public:
        explicit transport_error(const std::string& msg, const std::string& host, int port) 
            : temporary_error("Transport error: " + msg + " " + host + ":"+ boost::lexical_cast<std::string>(port))
        {}
};

class command_unexpected : public temporary_error
{
    public:
        explicit command_unexpected(const std::string& retmsg, int expected_retcode) 
            : temporary_error("Unexpected command reply [" + retmsg + "] expected:" + boost::lexical_cast<std::string>(expected_retcode))
        {}
        
        explicit command_unexpected(const std::string& cmd, const std::string& retmsg, int expected_retcode) 
            : temporary_error("Unexpected command reply [" + cmd + "] [" + retmsg + "] expected:" + boost::lexical_cast<std::string>(expected_retcode))
        {}
        
        explicit command_unexpected(const std::string& cmd, const std::string& arg, const std::string& retmsg, int expected_retcode) 
            : temporary_error("Unexpected command reply [" + cmd + " " + arg + "] [" + retmsg + "] expected:" + boost::lexical_cast<std::string>(expected_retcode))
        {}
};

class transient_error : public temporary_error
{
    public:
        explicit transient_error(const std::string& retmsg) 
            : temporary_error("Transient error [" + retmsg + "]")
        {}
        explicit transient_error(const std::string& cmd, const std::string& retmsg) 
            : temporary_error("Transient error [" + cmd + "] [" + retmsg + "]")
        {}
        explicit transient_error(const std::string& cmd, const std::string& arg, const std::string& retmsg) 
            : temporary_error("Transient error [" + cmd + " " + arg + "] [" + retmsg + "]")
        {}
};

class protocol_error : public temporary_error
{
    public:
        explicit protocol_error(const std::string& retmsg) 
            : temporary_error("Protocol error [" + retmsg + "]")
        {}
        explicit protocol_error(const std::string& cmd, const std::string& retmsg) 
            : temporary_error("Protocol error [" + cmd + "] [" + retmsg + "]")
        {}
        explicit protocol_error(const std::string& cmd, const std::string& arg, const std::string& retmsg) 
            : temporary_error("Protocol error [" + cmd + " " + arg + "] [" + retmsg + "]")
        {}
};

typedef std::vector<std::string> RecipientList;

// Пример использования
//
// transport::SmtpTransport t;
// t.connect(mx_host);
// std::stringstream ss;
// ss<<"From: <test@km.ru>\r\n\r\nMessage body";
// transport::RecipientList rl;
// rl.push_back("support@km.ru");
// t.send_message(ss, "test@km.ru", rl);
//
class SmtpTransport
{
	public:
		~SmtpTransport();

		SmtpTransport(int timeout = 4);	

        void connect(const std::string& host, int port = 25, std::string helo = "");

        void send_message(std::istream& ifs, const std::string& from, const RecipientList& recipient_list);
        
        std::string host() const { return host_; }
        
        int port() const { return port_; }

    private:

        std::string normalize(const std::string& email);

        unsigned int say(const std::string& cmd, unsigned int expected_retcode = 0);

        unsigned int say(const std::string& cmd, const std::string& arg, unsigned int expected_retcode = 0);

        unsigned int say_data(std::string& buf, unsigned int expected_retcode = 0);

        unsigned int get(std::vector<std::string>& response, unsigned int max_len = 4096);


	private:
        std::string host_;
        int port_;
        bool connected_;
        ACE_SOCK_Stream stream_;
        ACE_Time_Value timeout_;

        static boost::uint32_t connect_count;
public:
        static int get_connection_count();
};

inline bool match_str(const char* b, const char* e, const char* term_str, unsigned int term_len, unsigned int* match_from)
{
    unsigned int n = e - b;
    if( n >= (term_len - *match_from))
    {
        n = (term_len - *match_from);
    }

    unsigned int i;
    for(i = 0; i < n; ++i)
    {
        if( b[i] != term_str[*match_from+i])
        {
            return false;
        }
    }

    if(*match_from + i == term_len)
    {
        return true;
    }
    else
    {
        *match_from += (i);
        return false;
    }
}

}

