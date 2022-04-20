/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres.gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>

#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/co_spawn.hpp>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

namespace net = boost::asio;
namespace resp3 = aedis::resp3;

using aedis::resp3::node;
using aedis::redis::command;
using aedis::generic::make_serializer;
using net::ip::tcp;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;

net::awaitable<void> example()
{
   auto ex = co_await net::this_coro::executor;

   tcp::resolver resv{ex};
   auto const res = resv.resolve("127.0.0.1", "6379");
   tcp_socket socket{ex};
   co_await socket.async_connect(*std::begin(res));

   std::string request, buffer;

   auto sr = make_serializer(request);
   sr.push(command::hello, 3);
   sr.push(command::ping, "Some message.");
   sr.push(command::quit);
   co_await net::async_write(socket, net::buffer(request));

   auto adapter = [](node<boost::string_view> const& nd, boost::system::error_code&)
   {
      std::cout << nd << std::endl;
   };

   auto dbuffer = net::dynamic_buffer(buffer);
   co_await resp3::async_read(socket, dbuffer); // hello
   co_await resp3::async_read(socket, dbuffer, adapter);
   co_await resp3::async_read(socket, dbuffer); // quit
}

int main()
{
   try {
      net::io_context ioc;
      net::co_spawn(ioc, example(), net::detached);
      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
