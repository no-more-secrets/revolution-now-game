/****************************************************************
**thing.hpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2021-06-09.
*
* Description: C++ containers for Lua values/objects.
*
*****************************************************************/
#pragma once

#include "core-config.hpp"

// luapp
#include "types.hpp"

// base
#include "base/error.hpp"
#include "base/fmt.hpp"
#include "base/variant.hpp"

// C++ standard library
#include <string>

namespace luapp {

struct reference {
  reference() = delete;
  reference( lua_State* st, int ref );
  ~reference() noexcept;

  void release() noexcept;

  reference( reference&& rhs ) noexcept;
  reference& operator=( reference&& rhs ) noexcept;

  reference( reference const& ) = delete;
  reference& operator=( reference const& ) = delete;

  operator bool() const noexcept;

  // Pushes nil if there is no reference.
  void push() const noexcept;

  static int noref() noexcept;

protected:
  lua_State* L = nullptr; // not owned.
private:
  int ref_ = noref();
};

struct table : public reference {
  using Base = reference;

  using Base::Base;
  using Base::operator bool;
};

struct lstring : public reference {
  using Base = reference;

  using Base::Base;
  using Base::operator bool;
};

struct lfunction : public reference {
  using Base = reference;

  using Base::Base;
  using Base::operator bool;
};

struct userdata : public reference {
  using Base = reference;

  using Base::Base;
  using Base::operator bool;
};

struct lthread : public reference {
  using Base = reference;

  using Base::Base;
  using Base::operator bool;
};

// This is just a value type.
struct lightuserdata : public base::safe::void_p {
  using Base = base::safe::void_p;

  using Base::Base;
  // using Base::operator==;
  // using Base::operator void*;
  using Base::get;
};

// nil_t should be first so that it is selected as the default.
using thing_base = base::variant<nil_t,         //
                                 boolean,       //
                                 lightuserdata, //
                                 integer,       //
                                 floating,      //
                                 lstring,       //
                                 table,         //
                                 lfunction,     //
                                 userdata,      //
                                 lthread        //
                                 >;

struct thing : public thing_base {
  using Base = thing_base;

  // If you are getting an error because of this, that means that
  // you must explicitely cast the pointer to `void*` before com-
  // parison so that there is no ambiguity about what it repre-
  // sents (pointers represent light userdata, not e.g. string
  // literals).
  template<typename T>
  // clang-format off
  requires( !std::is_same_v<T*, void*> )
  bool operator==( T* p ) const noexcept = delete;
  // clang-format on

  template<typename T>
  // clang-format off
  requires( !std::is_same_v<thing, std::remove_cvref_t<T>> )
  bool operator==( T const& rhs ) const noexcept {
    // clang-format on
    return this->visit( [&]<typename U>( U&& o ) {
      using elem_t      = decltype( std::forward<U>( o ) );
      using elem_base_t = std::remove_cvref_t<elem_t>;
      if constexpr( std::is_same_v<T, std::remove_cvref_t<U>> ) {
        static_assert(
            std::equality_comparable_with<elem_t, T const&> );
        return ( o == rhs );
      } else if constexpr( std::is_convertible_v<T const&,
                                                 elem_t> ) {
        return ( o == static_cast<elem_t>( rhs ) );
      } else if constexpr( std::is_convertible_v<elem_t,
                                                 T const&> ) {
        return ( static_cast<T const&>( o ) == rhs );
      } else {
        return false;
      }
    } );
  }

  bool operator==( thing const& rhs ) const noexcept;

  // Follows Lua's rules, where every value is true except for
  // boolean:false and nil.
  operator bool() const noexcept;

  template<typename T>
  bool operator!=( T const& rhs ) const noexcept {
    return !( *this == rhs );
  }

  e_lua_type type() const noexcept;

  /**************************************************************
  ** Take things that we're not going to call explicitly.
  ***************************************************************/
  using Base::Base;
  using Base::operator=;
};

/****************************************************************
** to_str
*****************************************************************/
void to_str( table const& o, std::string& out );
void to_str( lstring const& o, std::string& out );
void to_str( lfunction const& o, std::string& out );
void to_str( userdata const& o, std::string& out );
void to_str( lthread const& o, std::string& out );
void to_str( lightuserdata const& o, std::string& out );

} // namespace luapp

/****************************************************************
** fmt
*****************************************************************/
TOSTR_TO_FMT( luapp::table );
TOSTR_TO_FMT( luapp::lstring );
TOSTR_TO_FMT( luapp::lfunction );
TOSTR_TO_FMT( luapp::userdata );
TOSTR_TO_FMT( luapp::lthread );
TOSTR_TO_FMT( luapp::lightuserdata );

DEFINE_FORMAT( luapp::thing, "{}", o.as_std() );
