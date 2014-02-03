#include "smtpsend.hpp"

#include <fstream>
#include <vector>
#include <numeric>
#include <algorithm>
#include <algorithm>
#include <cctype>
#include <functional>
#include <string> 
//#include <boost/algorithm/string/replace.hpp>
//#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string.hpp>

#include <ace/SOCK_Connector.h>

#include <bk/debug.hpp>

namespace transport
{

boost::uint32_t SmtpTransport::connect_count = 0;

int SmtpTransport::get_connection_count()
{
    return connect_count;
}

SmtpTransport::~SmtpTransport()
{
    if(connected_)
    {
        stream_.close();
        boost::interprocess::ipcdetail::atomic_dec32(&connect_count);
    }
}

SmtpTransport::SmtpTransport(int timeout) : timeout_(timeout) 
{
    connected_ = false;
}

std::string SmtpTransport::normalize(const std::string& email)
{
    if(email[0] == '<' && email[email.length() -1] == '>')
    {
        return email;
    }
    else
    {
        return std::string("<") + email + std::string(">");
    }
}

void SmtpTransport::connect(const std::string& host, int port, std::string helo)
{
    ACE_SOCK_Connector connector;
    ACE_INET_Addr addr;

    ACE_Time_Value tm(timeout_);

    if(addr.set(port, host.c_str()))
        throw resolve_error(host, port);
    else if(connector.connect(stream_, addr, &tm) )
        throw connect_exception(host, port);

    host_ = host;
    port_ = port;

    boost::interprocess::ipcdetail::atomic_inc32(&connect_count);
    connected_ = true;
    
    std::vector<std::string> lines;
    unsigned int retcode = get(lines, 1024);
    if(retcode >= 400 && retcode < 500)
    {
        throw transient_error(std::accumulate(lines.begin(), lines.end(), std::string("")) );
    }
    else if(retcode > 500)
    {
        throw permanent_error(std::accumulate(lines.begin(), lines.end(), std::string("")) );
    }
   
    if(helo.empty())
    {
        helo = "me";
    }

    try
    {
        say("EHLO", helo);
    }
    catch(const permanent_error& e)
    {
        say("HELO", helo);
    }
}

void SmtpTransport::send_message(std::istream& is, const std::string& from, const RecipientList& recipient_list)
{
    if(connected_ == false)
    {
        throw connect_exception();
    }
    
    //std::cout << "MAIL FROM: "<< from << "\n";

    say("MAIL FROM:", normalize(from));
    
    RecipientList::const_iterator pos;
    for(pos = recipient_list.begin(); pos != recipient_list.end(); ++pos)
    {
        //std::cout << "RCPT TO: "<< (*pos) << "\n";
        say("RCPT TO:", normalize(*pos));
    }

    //std::cout << "DATA\n";
    say("DATA");
    
    //std::cout << "MESSAGE\n";
    std::istreambuf_iterator<char> eos;
    std::string buf(std::istreambuf_iterator<char>(is), eos);
    say_data(buf);

    //std::cout << "QUIT\n";
    say("QUIT");
}

unsigned int SmtpTransport::say(const std::string& cmd, const std::string& arg, unsigned int expected_retcode)
{
    iovec iov[4];
    iov[0].iov_base = const_cast<char*> (cmd.c_str());
    iov[0].iov_len = cmd.length();
    iov[1].iov_base = const_cast<char*> (" ");
    iov[1].iov_len = 1;
    iov[2].iov_base = const_cast<char*> (arg.c_str());
    iov[2].iov_len = arg.length();
    iov[3].iov_base = const_cast<char*> ("\r\n");
    iov[3].iov_len = 2;

    unsigned int buff_len = cmd.length() + 1 + arg.length() + 2;

    unsigned int res = stream_.sendv_n(iov, 4, &timeout_);
    if (res != buff_len)
    {
        throw transport_error("Can't send data", host_, port_);
    }

    std::vector<std::string> lines;
    unsigned int retcode = get(lines, 1024);
    if(retcode != expected_retcode && expected_retcode != 0)
    {
        throw command_unexpected(cmd, arg, std::accumulate(lines.begin(), lines.end(), std::string("")), expected_retcode);
    }
    
    if(retcode >= 400 && retcode < 500)
    {
        throw transient_error(std::accumulate(lines.begin(), lines.end(), std::string("")) );
    }
    else if(retcode > 500)
    {
        throw permanent_error(std::accumulate(lines.begin(), lines.end(), std::string("")) );
    }

    return retcode;
}

unsigned int SmtpTransport::say(const std::string& cmd, unsigned int expected_retcode)
{
    iovec iov[2];
    iov[0].iov_base = const_cast<char*> (cmd.c_str());
    iov[0].iov_len = cmd.length();
    iov[1].iov_base = const_cast<char*> ("\r\n");
    iov[1].iov_len = 2;

    unsigned int buff_len = cmd.length() + 2;

    unsigned int res = stream_.sendv_n(iov, 2, &timeout_);
    if (res != buff_len)
    {
        throw transport_error("Can't send data", host_, port_);
    }

    std::vector<std::string> lines;
    unsigned int retcode = get(lines, 1024);
    if(retcode != expected_retcode && expected_retcode != 0)
    {
        throw command_unexpected(cmd, std::accumulate(lines.begin(), lines.end(), std::string("")), expected_retcode);
    }

    if(retcode >= 400 && retcode < 500)
    {
        throw transient_error(std::accumulate(lines.begin(), lines.end(), std::string("")) );
    }
    else if(retcode > 500)
    {
        throw permanent_error(std::accumulate(lines.begin(), lines.end(), std::string("")) );
    }

    return retcode;
}

unsigned int SmtpTransport::say_data(std::string& buf, unsigned int expected_retcode)
{
    boost::algorithm::replace_all(buf, "\r\n.\r\n", "\r\n..\r\n");

    buf.append("\r\n.\r\n");

    unsigned int res = stream_.send_n(&buf[0], buf.size(), &timeout_);
    if (res != buf.size())
    {
        throw transport_error("Can't send data", host_, port_);
    }
    
    std::vector<std::string> lines;
    unsigned int retcode = get(lines, 1024);
    if(retcode != expected_retcode && expected_retcode != 0)
    {
        throw command_unexpected(std::accumulate(lines.begin(), lines.end(), std::string("")), expected_retcode);
    }
    
    if(retcode >= 400 && retcode < 500)
    {
        throw transient_error(std::accumulate(lines.begin(), lines.end(), std::string("")) );
    }
    else if(retcode > 500)
    {
        throw permanent_error(std::accumulate(lines.begin(), lines.end(), std::string("")) );
    }

    return retcode;
}

unsigned int SmtpTransport::get(std::vector<std::string>& response, unsigned int max_len)
{
    std::vector<char> buff(max_len);

    unsigned int read_bytes = 0;
    unsigned int match_from = 0;

    while( read_bytes < max_len)
    {
        int received = stream_.recv(&buff[read_bytes], max_len - read_bytes, &timeout_);
        if(received == -1)
        {
            throw transport_error("Can't receive data", host_, port_);
        }
        else if(received == 0)
        {
            throw transport_error("Can't receive data", host_, port_);
        }

        unsigned int line_start = read_bytes;
        for(unsigned int i = read_bytes; i < read_bytes + received; ++i)
        {
            if(match_str(&buff[i], &buff[received], "\r\n", 2, &match_from ))
            {
                response.push_back( std::string(&buff[line_start], i - line_start) );

                if((i - line_start) > 3)
                {
                    if(buff[line_start + 3] == ' ')
                    {
                        std::string retcode_str = std::string(&buff[line_start],3);
                        unsigned int retcode;
                        try
                        {
                            retcode = boost::lexical_cast<unsigned int>(retcode_str); 
                        }
                        catch(boost::bad_lexical_cast& e)
                        {
                            throw protocol_error("Broken reply code", retcode_str);
                        }

                        if(retcode < 200 || retcode > 599)
                        {
                            throw protocol_error("Unknown reply code", retcode_str);
                        }

                        return retcode;
                    }
                }
                else
                {
                    throw protocol_error("Broken reply", std::string(&buff[0], read_bytes + received));
                }

                line_start = i + 2;

                match_from = 0;
            }
        }

        read_bytes += received;
    }
    
    throw protocol_error("Broken reply");
}

}

