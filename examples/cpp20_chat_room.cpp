/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/asio.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#include <iostream>
namespace net = boost::asio;
#if defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/redis.hpp>
#include <boost/redis/experimental/run.hpp>
#include <unistd.h>

#include "common/common.hpp"

using namespace net::experimental::awaitable_operators;
using stream_descriptor = net::use_awaitable_t<>::as_default_on_t<net::posix::stream_descriptor>;
using signal_set = net::use_awaitable_t<>::as_default_on_t<net::signal_set>;
using boost::redis::request;
using boost::redis::generic_response;
using boost::redis::experimental::async_check_health;

// Chat over Redis pubsub. To test, run this program from multiple
// terminals and type messages to stdin.

// Receives Redis pushes.
auto receiver(std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   for (generic_response resp;;) {
      co_await conn->async_receive(resp);
      std::cout << resp.value().at(1).value << " " << resp.value().at(2).value << " " << resp.value().at(3).value << std::endl;
      resp.value().clear();
   }
}

// Publishes stdin messages to a Redis channel.
auto publisher(std::shared_ptr<stream_descriptor> in, std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   for (std::string msg;;) {
      auto n = co_await net::async_read_until(*in, net::dynamic_buffer(msg, 1024), "\n");
      request req;
      req.push("PUBLISH", "chat-channel", msg);
      co_await conn->async_exec(req);
      msg.erase(0, n);
   }
}

// Called from the main function (see main.cpp)
auto co_main(std::string host, std::string port) -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;
   auto conn = std::make_shared<connection>(ex);
   auto stream = std::make_shared<stream_descriptor>(ex, ::dup(STDIN_FILENO));
   signal_set sig{ex, SIGINT, SIGTERM};

   request req;
   req.push("HELLO", 3);
   req.push("SUBSCRIBE", "chat-channel");

   co_await connect(conn, host, port);
   co_await ((conn->async_run() || publisher(stream, conn) || receiver(conn) ||
         async_check_health(*conn) || sig.async_wait()) &&
         conn->async_exec(req));
}

#else // defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
auto co_main(std::string host, std::string port) -> net::awaitable<void>
{
   std::cout << "Requires support for posix streams." << std::endl;
   co_return;
}
#endif // defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
