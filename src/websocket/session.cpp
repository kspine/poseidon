// 这个文件是 Poseidon 服务器应用程序框架的一部分。
// Copyleft 2014 - 2016, LH_Mouse. All wrongs reserved.

#include "../precompiled.hpp"
#include "session.hpp"
#include "exception.hpp"
#include "../http/low_level_session.hpp"
#include "../optional_map.hpp"
#include "../singletons/main_config.hpp"
#include "../singletons/job_dispatcher.hpp"
#include "../log.hpp"
#include "../job_base.hpp"
#include "../profiler.hpp"

namespace Poseidon {

namespace WebSocket {
	class Session::SyncJobBase : public JobBase {
	private:
		const TcpSessionBase::DelayedShutdownGuard m_guard;
		const boost::weak_ptr<Http::LowLevelSession> m_parent;
		const boost::weak_ptr<Session> m_session;

	protected:
		explicit SyncJobBase(const boost::shared_ptr<Session> &session)
			: m_guard(session->get_safe_parent()), m_parent(session->get_weak_parent()), m_session(session)
		{
		}

	private:
		boost::weak_ptr<const void> get_category() const FINAL {
			return m_parent;
		}
		void perform() FINAL {
			PROFILE_ME;

			const AUTO(session, m_session.lock());
			if(!session){
				return;
			}

			try {
				really_perform(session);
			} catch(Exception &e){
				LOG_POSEIDON(Logger::SP_MAJOR | Logger::LV_INFO,
					"WebSocket::Exception thrown: status_code = ", e.get_status_code(), ", what = ", e.what());
				session->shutdown(e.get_status_code(), StreamBuffer(e.what()));
			} catch(std::exception &e){
				LOG_POSEIDON(Logger::SP_MAJOR | Logger::LV_INFO,
					"std::exception thrown: what = ", e.what());
				session->force_shutdown();
				throw;
			} catch(...){
				LOG_POSEIDON(Logger::SP_MAJOR | Logger::LV_INFO,
					"Unknown exception thrown.");
				session->force_shutdown();
				throw;
			}
		}

	protected:
		virtual void really_perform(const boost::shared_ptr<Session> &session) = 0;
	};

	class Session::DataMessageJob : public Session::SyncJobBase {
	private:
		OpCode m_opcode;
		StreamBuffer m_payload;

	public:
		DataMessageJob(const boost::shared_ptr<Session> &session, OpCode opcode, StreamBuffer payload)
			: SyncJobBase(session)
			, m_opcode(opcode), m_payload(STD_MOVE(payload))
		{
		}

	protected:
		void really_perform(const boost::shared_ptr<Session> &session) OVERRIDE {
			PROFILE_ME;

			LOG_POSEIDON_DEBUG("Dispatching data message: opcode = ", m_opcode, ", payload_size = ", m_payload.size());
			session->on_sync_data_message(m_opcode, STD_MOVE(m_payload));

			const AUTO(keep_alive_timeout, MainConfig::get<boost::uint64_t>("websocket_keep_alive_timeout", 30000));
			session->set_timeout(keep_alive_timeout);
		}
	};

	class Session::ControlMessageJob : public Session::SyncJobBase {
	private:
		OpCode m_opcode;
		StreamBuffer m_payload;

	public:
		ControlMessageJob(const boost::shared_ptr<Session> &session, OpCode opcode, StreamBuffer payload)
			: SyncJobBase(session)
			, m_opcode(opcode), m_payload(STD_MOVE(payload))
		{
		}

	protected:
		void really_perform(const boost::shared_ptr<Session> &session) OVERRIDE {
			PROFILE_ME;

			LOG_POSEIDON_DEBUG("Dispatching control message: opcode = ", m_opcode, ", payload_size = ", m_payload.size());
			session->on_sync_control_message(m_opcode, STD_MOVE(m_payload));

			const AUTO(keep_alive_timeout, MainConfig::get<boost::uint64_t>("websocket_keep_alive_timeout", 30000));
			session->set_timeout(keep_alive_timeout);
		}
	};

	Session::Session(const boost::shared_ptr<Http::LowLevelSession> &parent, boost::uint64_t max_request_length)
		: LowLevelSession(parent)
		, m_max_request_length(max_request_length ? max_request_length
		                                          : MainConfig::get<boost::uint64_t>("websocket_max_request_length", 16384))
		, m_size_total(0), m_opcode(OP_INVALID)
	{
	}
	Session::~Session(){
	}

	void Session::on_read_avail(StreamBuffer data)
	try {
		m_size_total += data.size();
		if(m_size_total > m_max_request_length){
			DEBUG_THROW(Exception, ST_MESSAGE_TOO_LARGE, sslit("Message too large"));
		}

		LowLevelSession::on_read_avail(STD_MOVE(data));
	} catch(Exception &e){
		LOG_POSEIDON(Logger::SP_MAJOR | Logger::LV_INFO,
			"WebSocket::Exception thrown in WebSocket parser: status_code = ", e.get_status_code(), ", what = ", e.what());
		shutdown(e.get_status_code(), e.what());
	}

	void Session::on_low_level_message_header(OpCode opcode){
		PROFILE_ME;

		m_size_total = 0;
		m_opcode = opcode;
		m_payload.clear();
	}
	void Session::on_low_level_message_payload(boost::uint64_t whole_offset, StreamBuffer payload){
		PROFILE_ME;

		(void)whole_offset;

		m_payload.splice(payload);
	}
	bool Session::on_low_level_message_end(boost::uint64_t whole_size){
		PROFILE_ME;

		(void)whole_size;

		JobDispatcher::enqueue(
			boost::make_shared<DataMessageJob>(
				virtual_shared_from_this<Session>(), m_opcode, STD_MOVE(m_payload)),
			VAL_INIT);

		return true;
	}
	bool Session::on_low_level_control_message(OpCode opcode, StreamBuffer payload){
		PROFILE_ME;

		JobDispatcher::enqueue(
			boost::make_shared<ControlMessageJob>(
				virtual_shared_from_this<Session>(), opcode, STD_MOVE(payload)),
			VAL_INIT);

		return true;
	}

	void Session::on_sync_control_message(OpCode opcode, StreamBuffer payload){
		PROFILE_ME;
		LOG_POSEIDON_DEBUG("Control frame: opcode = ", opcode);

		const AUTO(parent, get_parent());
		if(!parent){
			return;
		}

		switch(opcode){
		case OP_CLOSE:
			LOG_POSEIDON_INFO("Received close frame from ", parent->get_remote_info());
			shutdown(ST_NORMAL_CLOSURE, VAL_INIT);
			break;

		case OP_PING:
			LOG_POSEIDON_INFO("Received ping frame from ", parent->get_remote_info());
			send(OP_PONG, STD_MOVE(payload));
			break;

		case OP_PONG:
			LOG_POSEIDON_INFO("Received pong frame from ", parent->get_remote_info());
			break;

		default:
			DEBUG_THROW(Exception, ST_PROTOCOL_ERROR, sslit("Invalid opcode"));
			break;
		}
	}
}

}
