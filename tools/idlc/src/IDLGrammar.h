#ifndef IDLGRAMMER_H
#define IDLGRAMMER_H

#include "InterfaceDescriptionBuilder.h"

#include <iostream>
#include <memory>
#include <tao/pegtl.hpp>

/**
 * Define the IDL grammar.
 */
namespace idl {
using namespace tao::pegtl;

// define whitespace and a template `padr` that will accept a token padded by zero or more of it
struct ws : one<' ', '\t', '\n', '\r'> {};
template<typename R, typename P = ws>
struct padr : seq<R, star<P>> {};
template<typename R, typename P = ws>
struct padl : seq<star<P>, R> {};
template<typename R, typename P = ws>
struct pad : seq<star<P>, R, star<P>> {};

// single line comments are any lines that start with the "//" tokens
struct single_comment_cont : until<eolf> {};
struct single_comment : seq<star<ws>, two<'/'>, single_comment_cont> {};
// multiline comments are any string enclosed by "/*" and "*/"
struct multi_comment_cont : until<TAO_PEGTL_STRING("*/")> {};
struct multi_comment : seq<star<ws>, TAO_PEGTL_STRING("/*"), multi_comment_cont> {};
// either type of comment is acceptable anywhere
struct comment : sor<single_comment, multi_comment> {};

// include statement
struct include_open: one<'<'> {};
struct include_path: star<sor<ascii::identifier_other, one<'.'>, one<'/'>>> {};
struct include_close: one<'>'> {};
struct include_cont : until<eolf> {};
struct include : seq<TAO_PEGTL_STRING("#include"), star<ascii::blank>,
    include_open, include_path, include_close, include_cont> {};

// define basic structure of the interface
struct begin_interface : padr<one<'{'>> {};
struct end_interface : one<'}'> {};

// empty lines (only whitespace)
struct empty_line : seq<star<ascii::blank>, eol> {};

// decorators (arbitrary key/value tags to "glue" to a subsequent method)
struct decorator_ws : star<ascii::blank> {};

struct decorator_open : one<'['> {};
struct decorator_key : identifier {};
struct decorator_sep : pad<one<'='>> {};
struct decorator_value : star<sor<ranges< 'a', 'z', 'A', 'Z', '0', '9', '_' >>> {};
struct decorator_close : one<']'> {};

struct decorator_content: seq<decorator_key, decorator_sep, decorator_value> {};
struct method_decorator_group: seq<decorator_open, decorator_content, decorator_close, decorator_ws> {};

// define an interface method
struct method_arg_name : identifier {};
struct method_arg_sep : pad<one<':'>> {};
struct method_arg_type: seq<identifier_first, star<sor<identifier_other, two<':'>>>> {};
struct method_arg_end : success{};
struct method_arg : seq<method_arg_name, method_arg_sep, method_arg_type, method_arg_end> {};

struct method_next_arg_sep : padr<one<','>> {};
struct method_next_arg : seq<method_arg> {};

struct method_args_open : one<'('> {};
struct method_args : opt<method_arg, star<method_next_arg_sep, method_next_arg>> {};
struct method_args_close : one<')'> {};

// a group of name:type args, optionally comma separated, in a () container
struct method_args_group: sor<
                          must<method_args_open, method_args, method_args_close>
                        > {};

// whitespace allowed in a method body
struct method_return_open : one<'('> {};
struct method_return_close : one<')'> {};

struct method_ws : star<ascii::blank> {};
struct method_name: identifier {};
struct method_return : sor<must<method_return_open, method_args, method_return_close>> {};

struct method_async_return_marker : TAO_PEGTL_STRING("=|") {};
struct method_sync_return_marker : TAO_PEGTL_STRING("=>") {};

struct method_sync_return : must<method_sync_return_marker, method_ws, method_return> {};

struct method_end : success{};
struct method_return_type : sor<method_async_return_marker, method_sync_return> {};

struct method: seq<
    method_name, method_ws,
    opt<star<method_decorator_group>>,
    method_ws, method_args_group, 
    method_ws, method_return_type,
    method_ws, method_end> {};

// define overall structure of the interface construct
struct interface_name : identifier {};
struct interface_start : seq<padl<TAO_PEGTL_KEYWORD("interface")>, plus<ws>, interface_name, star<ws>,
    one<'{'>> {};
struct interface_member : sor<empty_line, comment, padl<method>> {};
struct interface_next_member : seq<interface_member> {};
struct interface_content : opt<interface_member, star<interface_next_member>> {};
struct interface_end : pad<one<'}'>> {};

struct interface : seq<interface_start, interface_content, interface_end> {};


// Define the grammar as an arbitrary sequence of comments and interfaces, followed by end-of-file.
struct grammar : must<star<sor<empty_line, include, comment, interface>>, eof> {};
}

#endif
