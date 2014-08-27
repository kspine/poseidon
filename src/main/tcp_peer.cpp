#include "../precompiled.hpp"
#include "tcp_peer.hpp"
#include "exception.hpp"
#include "singletons/epoll_daemon.hpp"
#include "log.hpp"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
using namespace Poseidon;

TcpPeer::TcpPeer(ScopedFile &socket)
	: m_shutdown(false)
{
	m_socket.swap(socket);

	union {
		::sockaddr sa;
		::sockaddr_in sin;
		::sockaddr_in6 sin6;
	} u;
	::socklen_t salen = sizeof(u);

	if(::getpeername(m_socket.get(), &u.sa, &salen) != 0){
		const int code = errno;
		LOG_ERROR("Could not get peer address.");
		DEBUG_THROW(SystemError, code);
	}
	m_remoteIp.resize(63);
	const char *text;
	if(u.sa.sa_family == AF_INET){
		text = ::inet_ntop(AF_INET, &u.sin.sin_addr, &m_remoteIp[0], m_remoteIp.size());
	} else if(u.sa.sa_family == AF_INET6){
		text = ::inet_ntop(AF_INET6, &u.sin6.sin6_addr, &m_remoteIp[0], m_remoteIp.size());
	} else {
		DEBUG_THROW(Exception, "Unknown IP protocol " + boost::lexical_cast<std::string>(u.sa.sa_family));
	}
	if(!text){
		DEBUG_THROW(SystemError, errno);
	}
	m_remoteIp.resize(std::strlen(text));

	LOG_INFO("Created tcp peer, remote ip = ", m_remoteIp);
}
TcpPeer::~TcpPeer(){
	LOG_INFO("Destroyed tcp peer, remote ip = ", m_remoteIp);
}

std::size_t TcpPeer::peekWriteAvail(boost::mutex::scoped_lock &lock, void *data, std::size_t size) const {
	boost::mutex::scoped_lock(m_queueMutex).swap(lock);
	if(size == 0){
		return m_sendBuffer.size();
	} else {
		return m_sendBuffer.peek(data, size);
	}
}
void TcpPeer::notifyWritten(std::size_t size){
	const boost::mutex::scoped_lock lock(m_queueMutex);
	m_sendBuffer.discard(size);
}

void TcpPeer::send(const void *data, std::size_t size){
	if(atomicLoad(m_shutdown) != false){
		DEBUG_THROW(Exception, "Trying to send on a socket that has been shut down.");
	}
	{
		const boost::mutex::scoped_lock lock(m_queueMutex);
		m_sendBuffer.put(data, size);
	}
	EpollDaemon::notifyWriteable(virtualSharedFromThis<TcpPeer>());
}
void TcpPeer::shutdown(){
	atomicStore(m_shutdown, true);
	{
		const boost::mutex::scoped_lock lock(m_queueMutex);
		if(m_sendBuffer.empty()){
			::shutdown(getFd(), SHUT_RDWR);
		} else {
			EpollDaemon::notifyWriteable(virtualSharedFromThis<TcpPeer>());
		}
	}
}
void TcpPeer::forceShutdown(){
	atomicStore(m_shutdown, true);
	::shutdown(getFd(), SHUT_RDWR);
}