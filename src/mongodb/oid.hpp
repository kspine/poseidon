// 这个文件是 Poseidon 服务器应用程序框架的一部分。
// Copyleft 2014 - 2016, LH_Mouse. All wrongs reserved.

#ifndef POSEIDON_MONGODB_OID_HPP_
#define POSEIDON_MONGODB_OID_HPP_

#include "../cxx_ver.hpp"
#include <cstring>
#include <boost/array.hpp>
#include <boost/cstdint.hpp>

namespace Poseidon {

namespace MongoDb {
	class Oid {
	public:
		static Oid random();

	private:
		union {
			unsigned char bytes[12];
			struct {
				boost::uint8_t uts[4];
				boost::uint8_t mid[3];
				boost::uint8_t pid[2];
				boost::uint8_t inc[3];
			};
		} m_storage;

	public:
		CONSTEXPR Oid() NOEXCEPT
			: m_storage()
		{
		}
		explicit Oid(const unsigned char (&bytes)[12]){
			std::memcpy(m_storage.bytes, bytes, 12);
		}
		explicit Oid(const boost::array<unsigned char, 12> &bytes){
			std::memcpy(m_storage.bytes, bytes.data(), 12);
		}

	public:
		CONSTEXPR const unsigned char *begin() const {
			return m_storage.bytes;
		}
		unsigned char *begin(){
			return m_storage.bytes;
		}
		CONSTEXPR const unsigned char *end() const {
			return m_storage.bytes + 12;
		}
		unsigned char *end(){
			return m_storage.bytes + 12;
		}
		CONSTEXPR const unsigned char *data() const {
			return m_storage.bytes;
		}
		unsigned char *data(){
			return m_storage.bytes;
		}
		CONSTEXPR std::size_t size() const {
			return 12;
		}

	public:
		const unsigned char &operator[](unsigned index) const {
			return m_storage.bytes[index];
		}
		unsigned char &operator[](unsigned index){
			return m_storage.bytes[index];
		}
	};

	inline bool operator==(const Oid &lhs, const Oid &rhs){
		return std::memcmp(lhs.begin(), rhs.begin(), 12) == 0;
	}
	inline bool operator!=(const Oid &lhs, const Oid &rhs){
		return std::memcmp(lhs.begin(), rhs.begin(), 12) != 0;
	}
	inline bool operator<(const Oid &lhs, const Oid &rhs){
		return std::memcmp(lhs.begin(), rhs.begin(), 12) < 0;
	}
	inline bool operator>(const Oid &lhs, const Oid &rhs){
		return std::memcmp(lhs.begin(), rhs.begin(), 12) > 0;
	}
	inline bool operator<=(const Oid &lhs, const Oid &rhs){
		return std::memcmp(lhs.begin(), rhs.begin(), 12) <= 0;
	}
	inline bool operator>=(const Oid &lhs, const Oid &rhs){
		return std::memcmp(lhs.begin(), rhs.begin(), 12) >= 0;
	}
}

}

#endif
