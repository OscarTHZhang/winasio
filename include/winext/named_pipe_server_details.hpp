#ifndef ASIO_NAMED_PIPE_DETAILS_HPP
#define ASIO_NAMED_PIPE_DETAILS_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "boost/asio/coroutine.hpp"

#include <iostream>

namespace winext{
namespace details{

// For server pipe.
// create namedpipe and event, and make server listen on this pipe
// and return handles to caller. caller is responsible to free the handles.
inline void accept_detail2(
        HANDLE& pipe_ret,
        boost::asio::windows::overlapped_ptr &optr,
        std::string const &endpoint, 
        boost::system::error_code & ret)
{

    const int bufsize = 512;

    HANDLE hPipe = CreateNamedPipe( 
        endpoint.c_str(),        // pipe name 
        PIPE_ACCESS_DUPLEX |     // read/write access 
        FILE_FLAG_OVERLAPPED,    // overlapped mode 
        PIPE_TYPE_MESSAGE |      // message-type pipe 
        PIPE_READMODE_MESSAGE |  // message-read mode 
        PIPE_WAIT,               // blocking mode 
        PIPE_UNLIMITED_INSTANCES,               // number of instances 
        bufsize*sizeof(TCHAR),   // output buffer size 
        bufsize*sizeof(TCHAR),   // input buffer size 
        0,            // client time-out 
        NULL);                   // default security attributes 

    if (hPipe == INVALID_HANDLE_VALUE) 
    {
        // printf("CreateNamedPipe failed with %d.\n", GetLastError());
        DWORD last_error = ::GetLastError();
        boost::system::error_code pec = boost::system::error_code(last_error,
                boost::asio::error::get_system_category());
        ret = pec;
        return;
    }
    // printf("created namedpipe\n");
    
    // pipe takes ownership of handles
    //pipe->assign(hPipe);

    // wait for client to connect
    bool fConnected = false; 
    fConnected = ConnectNamedPipe(hPipe, optr.get());
    // Overlapped ConnectNamedPipe should return zero. 
    if (fConnected)
    {
        // printf("ConnectNamedPipe failed with %d.\n", GetLastError());
        DWORD last_error = ::GetLastError();
        boost::system::error_code pec = boost::system::error_code(last_error,
                boost::system::system_category());
        ret = pec;
        return;
    }

    switch (GetLastError()) 
    { 
        // The overlapped connection in progress. 
        case ERROR_IO_PENDING: 
            // OS should have triggered callback
            // std::cout<< "pipe io pending" <<std::endl;
            optr.release(); // let io_context own the callback
            break; 
        // Client is already connected, so signal an event. 
        case ERROR_PIPE_CONNECTED:
            // std::cout<< "pipe already connected" <<std::endl;
            if (SetEvent(optr.get()->hEvent)) // trigger callback here
            {
                optr.release(); // let io_context own the callback
                break; 
            }
        // If an error occurs during the connect operation... 
        default: 
        {
            // printf("ConnectNamedPipe failed with %d.\n", GetLastError());
            DWORD last_error = ::GetLastError();
            boost::system::error_code pec = boost::system::error_code(last_error,
                    boost::asio::error::get_system_category());
            optr.complete(pec, 0);
            ret = pec;
            return;
        }
    }
    pipe_ret = hPipe;
    // std::cout << "accept_detail: pipe constructed" << std::endl;
}



// handler of signature void(error_code, pipe)
template<typename Executor>
class async_move_accept_op : boost::asio::coroutine {
public:
    typedef std::string endpoint_type;

    async_move_accept_op(server_named_pipe<Executor> *pipe, endpoint_type endpoint):
        pipe_(pipe), endpoint_(endpoint){}

    template <typename Self>
    void operator()(Self& self, boost::system::error_code ec = {}) {
        // std::cout << "async_move_accept_op" << std::endl;
        if (ec) {
            // std::cout << "async_move_accept_op has error" << std::endl;
            self.complete(ec, std::move(*pipe_));
            return;
        }
        HANDLE hPipe = NULL;
       
        boost::asio::windows::overlapped_ptr optr(pipe_->get_executor(), 
            [self=std::move(self), p = pipe_](boost::system::error_code ec, std::size_t) mutable{
                std::cout << "optr handler called."<< ec.message() << std::endl;
                self.complete(ec, std::move(*p));
            });
        
        details::accept_detail2(hPipe, optr, endpoint_, ec);
        if(ec){
            std::cout << "accept_detail2 failed:"<< ec.message() << std::endl;
            self.complete(ec, std::move(*pipe_));
        }
        pipe_->assign(hPipe);
    }

private:
    // pipe for movable case is the pipe holder in acceptor, which needs to moved to handler function,
    // so that to free acceptor pipe holder to handle the next connection.
    server_named_pipe<Executor> *pipe_;
    endpoint_type const endpoint_;
};

// handler of signature void(error_code)
template<typename Executor>
class async_accept_op : boost::asio::coroutine {
public:
    typedef std::string endpoint_type;

    async_accept_op(server_named_pipe<Executor> *pipe, endpoint_type endpoint):
        pipe_(pipe), endpoint_(endpoint){}

    template <typename Self>
    void operator()(Self& self, boost::system::error_code ec = {}) {
        // std::cout << "async_move_accept_op" << std::endl;
        if (ec) {
            // std::cout << "async_move_accept_op has error" << std::endl;
            self.complete(ec);
            return;
        }
        HANDLE hPipe = NULL;
       
        boost::asio::windows::overlapped_ptr optr(pipe_->get_executor(), 
            [self=std::move(self)](boost::system::error_code ec, std::size_t) mutable{
                std::cout << "optr handler called."<< ec.message() << std::endl;
                self.complete(ec);
            });
        
        details::accept_detail2(hPipe, optr, endpoint_, ec);
        if(ec){
            std::cout << "accept_detail2 failed:"<< ec.message() << std::endl;
            self.complete(ec);
        }
        pipe_->assign(hPipe);
    }

private:
    // pipe for movable case is the pipe holder in acceptor, which needs to moved to handler function,
    // so that to free acceptor pipe holder to handle the next connection.
    server_named_pipe<Executor> *pipe_;
    endpoint_type const endpoint_;
};


} // namespace details
} // namespace asio

#endif //ASIO_NAMED_PIPE_DETAILS_HPP