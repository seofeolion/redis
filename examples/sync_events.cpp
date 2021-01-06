/* Copyright (c) 2019 - 2020 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/aedis.hpp>

namespace net = aedis::net;

using namespace net;
using namespace aedis;

enum class myevents
{ ignore
, interesting1
, interesting2
};

int main()
{
   try {
      resp::request<myevents> req;
      req.hello();
      req.rpush("list", {1, 2, 3});
      req.lrange("list", 0, -1, myevents::interesting1);
      req.sadd("set", std::set<int>{3, 4, 5});
      req.smembers("set", myevents::interesting2);
      req.quit();

      io_context ioc {1};
      tcp::resolver resv(ioc);
      tcp::socket socket {ioc};
      net::connect(socket, resv.resolve("127.0.0.1", "6379"));
      resp::write(socket, req);

      std::string buffer;
      for (;;) {
	 switch (req.events.front().second) {
	    case myevents::interesting1:
	    {
	       resp::response_list<int> res;
	       resp::read(socket, buffer, res);
	       print(res.result);
	    } break;
	    case myevents::interesting2:
	    {
	       resp::response_set<int> res;
	       resp::read(socket, buffer, res);
	       print(res.result);
	    } break;
	    default:
	    {
	       resp::response_ignore res;
	       resp::read(socket, buffer, res);
	    }
	 }
	 req.events.pop();
      }
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
