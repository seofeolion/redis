/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include "common.hpp"

#include <boost/asio.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <iostream>

namespace net = boost::asio;
using namespace net::experimental::awaitable_operators;
using resolver = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::resolver>;
using timer_type = net::use_awaitable_t<>::as_default_on_t<net::steady_timer>;
using boost::redis::request;
using boost::redis::operation;

namespace
{
auto redir(boost::system::error_code& ec)
   { return net::redirect_error(net::use_awaitable, ec); }
}

auto
connect(
   std::shared_ptr<connection> conn,
   std::string const& host,
   std::string const& port) -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;
   resolver resv{ex};
   timer_type timer{ex};

   boost::system::error_code ec;
   timer.expires_after(std::chrono::seconds{5});
   auto const addrs = co_await (resv.async_resolve(host, port) || timer.async_wait(redir(ec)));
   if (!ec)
      throw std::runtime_error("Resolve timeout");

   timer.expires_after(std::chrono::seconds{5});
   co_await (net::async_connect(conn->next_layer(), std::get<0>(addrs)) || timer.async_wait(redir(ec)));
   if (!ec)
      throw std::runtime_error("Connect timeout");
}

auto run(net::awaitable<void> op) -> int
{
   try {
      net::io_context ioc;
      net::co_spawn(ioc, std::move(op), [](std::exception_ptr p) {
         if (p)
            std::rethrow_exception(p);
      });
      ioc.run();

      return 0;

   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
   }

   return 1;
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
