/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <tuple>
#include <string>
#include <iostream>

#include <boost/asio.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/ssl.hpp>

#include <boost/redis.hpp>
#include <boost/redis/ssl/connection.hpp>

namespace net = boost::asio;
namespace redis = boost::redis;
using namespace net::experimental::awaitable_operators;
using resolver = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::resolver>;
using connection = net::use_awaitable_t<>::as_default_on_t<redis::ssl::connection>;
using boost::redis::request;
using boost::redis::response;
using boost::redis::ignore_t;

auto verify_certificate(bool, net::ssl::verify_context&) -> bool
{
   std::cout << "set_verify_callback" << std::endl;
   return true;
}

net::awaitable<void> co_main(std::string, std::string)
{
   request req;
   req.push("HELLO", 3, "AUTH", "aedis", "aedis");
   req.push("PING");
   req.push("QUIT");

   response<ignore_t, std::string, ignore_t> resp;

   // Resolve
   auto ex = co_await net::this_coro::executor;
   resolver resv{ex};
   auto const endpoints = co_await resv.async_resolve("db.occase.de", "6380");

   net::ssl::context ctx{net::ssl::context::sslv23};
   connection conn{ex, ctx};
   conn.next_layer().set_verify_mode(net::ssl::verify_peer);
   conn.next_layer().set_verify_callback(verify_certificate);

   co_await net::async_connect(conn.lowest_layer(), endpoints);
   co_await conn.next_layer().async_handshake(net::ssl::stream_base::client);
   co_await (conn.async_run() || conn.async_exec(req, resp));

   std::cout << "Response: " << std::get<1>(resp).value() << std::endl;
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
