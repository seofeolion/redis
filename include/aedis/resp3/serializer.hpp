/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <queue>
#include <vector>
#include <string>
#include <string_view>
#include <utility>
#include <ostream>
#include <iterator>

#include <aedis/resp3/detail/composer.hpp>
#include <aedis/command.hpp>

namespace aedis {
namespace resp3 {

/** @brief Creates a RESP3 request from user data.
 *  \ingroup classes
 *  
 *  A request is composed of one or more redis commands and is
 *  referred to in the redis documentation as a pipeline, see
 *  https://redis.io/topics/pipelining, for example
 *
 *  @code
 *  serializer<command> sr;
 *  sr.push(command::hello, 3);
 *  sr.push(command::flushall);
 *  sr.push(command::ping);
 *  sr.push(command::incr, "key");
 *  sr.push(command::quit);
 *  co_await async_write(socket, buffer(sr.request()));
 *  @endcode
 *
 *  This class also maintains an internal queue of already added
 *  commands to assist users processing the response to each
 *  individual command contained in the request see response_queue.cpp
 *  for simple usage and echo_server.cpp for adavanced usage.
 */
template <class ResponseId>
class serializer {
private:
   std::string request_;

public:
   /// The commands that have been queued in this request.
   std::queue<ResponseId> commands;

public:
   /** \brief Clears the serializer.
    *  
    *  \remark Already acquired memory won't be released. The is useful
    *  to reusing memory insteam of allocating again each time.
    */
   void clear()
   {
      request_.clear();
      commands = {};
   }

   /** \brief Returns the request in RESP3 format.
    */
   auto const& request() const noexcept {return request_;}

   /** @brief Appends a new command to the end of the request.
    *
    *  Non-string types will be converted to string by using
    *  \c to_string, which must be made available by the user by ADL.
    */
   template <class... Ts>
   void push(ResponseId qelem, Ts const&... args)
   {
      // TODO: Should we detect any std::pair in the type in the pack
      // to calculate the header size correctly?

      auto constexpr pack_size = sizeof...(Ts);
      detail::add_header(request_, 1 + pack_size);

      auto const cmd = detail::request_get_command<ResponseId>::apply(qelem);
      detail::add_bulk(request_, to_string(cmd));
      (detail::add_bulk(request_, args), ...);

      if (!detail::has_push_response(cmd))
         commands.emplace(qelem);
   }

   /** @brief Appends a new command to the end of the request.
       
       This overload is useful for commands that have a key. For example
     
       \code{.cpp}
       std::map<std::string, std::string> map
	  { {"key1", "value1"}
	  , {"key2", "value2"}
	  , {"key3", "value3"}
	  };

       request req;
       req.push_range(command::hset, "key", std::cbegin(map), std::cend(map));
       \endcode
    */
   template <class Key, class ForwardIterator>
   void push_range(ResponseId qelem, Key const& key, ForwardIterator begin, ForwardIterator end)
   {
      // Note: For some commands like hset it would helpful to users
      // to assert the value type is a pair.

      using value_type = typename std::iterator_traits<ForwardIterator>::value_type;

      auto constexpr size = detail::value_type_size<value_type>::size;
      auto const distance = std::distance(begin, end);
      detail::add_header(request_, 2 + size * distance);
      auto const cmd = detail::request_get_command<ResponseId>::apply(qelem);
      detail::add_bulk(request_, to_string(cmd));
      detail::add_bulk(request_, key);

      for (; begin != end; ++begin)
	 detail::add_bulk(request_, *begin);

      if (!detail::has_push_response(cmd))
         commands.emplace(qelem);
   }

   /** @brief Appends a new command to the end of the request.
     
       This overload is useful for commands that don't have a key. For
       example
     
       \code{.cpp}
       std::set<std::string> channels
	  { "channel1" , "channel2" , "channel3" }

       request req;
       req.push(command::subscribe, std::cbegin(channels), std::cedn(channels));
       \endcode
    */
   template <class ForwardIterator>
   void push_range(ResponseId qelem, ForwardIterator begin, ForwardIterator end)
   {
      // Note: For some commands like hset it would be a good idea to assert
      // the value type is a pair.

      using value_type = typename std::iterator_traits<ForwardIterator>::value_type;

      auto constexpr size = detail::value_type_size<value_type>::size;
      auto const distance = std::distance(begin, end);
      detail::add_header(request_, 1 + size * distance);
      auto const cmd = detail::request_get_command<ResponseId>::apply(qelem);
      detail::add_bulk(request_, to_string(cmd));

      for (; begin != end; ++begin)
	 detail::add_bulk(request_, *begin);

      if (!detail::has_push_response(cmd))
         commands.emplace(qelem);
   }
};

} // resp3
} // aedis