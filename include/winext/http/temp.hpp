#ifndef UNICODE
#define UNICODE
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <http.h>

#include "winext/http/http.hpp"

#include <stdio.h>
#include <string>
#include <vector>
#include <iostream>
#include <functional>

#pragma comment(lib, "httpapi.lib")

namespace net = boost::asio;

template <typename Executor = net::any_io_executor>
class http_connection : public std::enable_shared_from_this<http_connection<Executor>>
{
public:
    typedef Executor executor_type;

    http_connection(winext::http::basic_http_handle<executor_type> & queue_handle)
        :   http_connection(queue_handle, [](const winext::http::simple_request<std::vector<CHAR>>& request, 
                winext::http::simple_response<std::string>& response){
                // default handler
                response.set_status_code(503);
                response.set_reason("Not Implemented");
            })
    { }

    http_connection(winext::http::basic_http_handle<executor_type> & queue_handle,
        std::function<void(const winext::http::simple_request<std::vector<CHAR>>&, 
        winext::http::simple_response<std::string>&)> handler)
        :   queue_handle_(queue_handle),
            request_(),
            response_(),
            handler_(handler)
    { }

    void
    start()
    {
        receive_request();
        // check_deadline();
    }

    // void set_handler(std::function<void(const winext::http::simple_request<std::vector<CHAR>>&, 
    //     winext::http::simple_response<std::string>&)> handler){
    //         this->handler_ = handler;
    // }

private:
    // request block
    winext::http::simple_request<std::vector<CHAR>> request_;
    // response block
    winext::http::simple_response<std::string> response_;

    winext::http::basic_http_handle<executor_type> & queue_handle_;

    std::function<void(const winext::http::simple_request<std::vector<CHAR>>&, winext::http::simple_response<std::string>&)> handler_;

    void
    receive_request()
    {
        std::cout << "connection receive_request" <<std::endl;
        auto self = this->shared_from_this();
        queue_handle_.async_recieve_request(net::buffer(request_.get_request_buffer()),
        [self](boost::system::error_code ec, std::size_t){
            if(ec){
                std::cout << "async_recieve_request failed: " << ec.message() <<std::endl;
            }else{
                self->on_receive_request();
                // start another connection
                std::make_shared<http_connection>(self->queue_handle_, self->handler_)->start();
            }
        });
    }

    void on_receive_request(){
        auto self = this->shared_from_this();
        queue_handle_.async_recieve_body(
            net::buffer(request_.get_body_buffer()),
            request_.get_request_id(),
            0, // flag
            [self](boost::system::error_code ec, std::size_t len){
                if(ec){
                    std::cout << "async_recieve_body failed: " << ec.message() <<std::endl;
                }else{
                    self->on_recieve_body(len);
                }
            });
    }

    void on_recieve_body(std::size_t len){
        request_.set_body_len(len);
        // all request data is ready, invoke use handler
        this->handler_(this->request_, this->response_);

        auto self = this->shared_from_this();
        queue_handle_.async_send_response(
            response_.get_response(),
            request_.get_request_id(),
            HTTP_SEND_RESPONSE_FLAG_DISCONNECT, // single resp flag
            [self](boost::system::error_code ec, std::size_t){
                std::cout << "async_send_response handler: " << ec.message() << std::endl; 
            });
    }
};