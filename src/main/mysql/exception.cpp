// 这个文件是 Poseidon 服务器应用程序框架的一部分。
// Copyleft 2014, LH_Mouse. All wrongs reserved.

#include "../precompiled.hpp"
#include "exception.hpp"
using namespace Poseidon;

MySqlException::MySqlException(const char *file, std::size_t line,
	unsigned code, const char *message)
	: ProtocolException(file, line, SharedNtmbs(message, true), code)
{
}
MySqlException::~MySqlException() NOEXCEPT {
}