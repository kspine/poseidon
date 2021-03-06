#include "../precompiled.hpp"
#include "client.hpp"
#include "exception.hpp"
#include "status_codes.hpp"
#include "../singletons/job_dispatcher.hpp"
#include "../log.hpp"
#include "../job_base.hpp"
#include "../profiler.hpp"

namespace Poseidon {

namespace Http {
	class Client::SyncJobBase : public JobBase {
	private:
		const TcpSessionBase::DelayedShutdownGuard m_guard;
		const boost::weak_ptr<Client> m_client;

	protected:
		explicit SyncJobBase(const boost::shared_ptr<Client> &client)
			: m_guard(client), m_client(client)
		{
		}

	private:
		boost::weak_ptr<const void> get_category() const FINAL {
			return m_client;
		}
		void perform() FINAL {
			PROFILE_ME;

			const AUTO(client, m_client.lock());
			if(!client){
				return;
			}

			try {
				really_perform(client);
			} catch(std::exception &e){
				LOG_POSEIDON(Logger::SP_MAJOR | Logger::LV_INFO, "std::exception thrown: what = ", e.what());
				client->force_shutdown();
				throw;
			} catch(...){
				LOG_POSEIDON(Logger::SP_MAJOR | Logger::LV_INFO, "Unknown exception thrown.");
				client->force_shutdown();
				throw;
			}
		}

	protected:
		virtual void really_perform(const boost::shared_ptr<Client> &client) = 0;
	};

	class Client::ConnectJob : public Client::SyncJobBase {
	public:
		explicit ConnectJob(const boost::shared_ptr<Client> &client)
			: SyncJobBase(client)
		{
		}

	protected:
		void really_perform(const boost::shared_ptr<Client> &client) OVERRIDE {
			PROFILE_ME;

			client->on_sync_connect();
		}
	};

	class Client::ResponseJob : public Client::SyncJobBase {
	private:
		ResponseHeaders m_response_headers;
		std::string m_transfer_encoding;
		StreamBuffer m_entity;

	public:
		ResponseJob(const boost::shared_ptr<Client> &client,
			ResponseHeaders response_headers, std::string transfer_encoding, StreamBuffer entity)
			: SyncJobBase(client)
			, m_response_headers(STD_MOVE(response_headers)), m_transfer_encoding(STD_MOVE(transfer_encoding)), m_entity(STD_MOVE(entity))
		{
		}

	protected:
		void really_perform(const boost::shared_ptr<Client> &client) OVERRIDE {
			PROFILE_ME;

			client->on_sync_response(STD_MOVE(m_response_headers), STD_MOVE(m_transfer_encoding), STD_MOVE(m_entity));
		}
	};

	Client::Client(const SockAddr &addr, bool use_ssl)
		: LowLevelClient(addr, use_ssl)
	{
	}
	Client::Client(const IpPort &addr, bool use_ssl)
		: LowLevelClient(addr, use_ssl)
	{
	}
	Client::~Client(){
	}

	void Client::on_connect(){
		PROFILE_ME;

		JobDispatcher::enqueue(
			boost::make_shared<ConnectJob>(
				virtual_shared_from_this<Client>()),
			VAL_INIT);

		LowLevelClient::on_connect();
	}

	void Client::on_low_level_response_headers(
		ResponseHeaders response_headers, std::string transfer_encoding, boost::uint64_t content_length)
	{
		PROFILE_ME;

		(void)content_length;

		m_response_headers = STD_MOVE(response_headers);
		m_transfer_encoding = STD_MOVE(transfer_encoding);
		m_entity.clear();
	}
	void Client::on_low_level_response_entity(boost::uint64_t entity_offset, bool is_chunked, StreamBuffer entity){
		PROFILE_ME;

		(void)entity_offset;
		(void)is_chunked;

		m_entity.splice(entity);
	}
	bool Client::on_low_level_response_end(boost::uint64_t content_length, bool is_chunked, OptionalMap headers){
		PROFILE_ME;

		(void)content_length;
		(void)is_chunked;

		for(AUTO(it, headers.begin()); it != headers.end(); ++it){
			m_response_headers.headers.append(it->first, STD_MOVE(it->second));
		}

		JobDispatcher::enqueue(
			boost::make_shared<ResponseJob>(
				virtual_shared_from_this<Client>(), STD_MOVE(m_response_headers), STD_MOVE(m_transfer_encoding), STD_MOVE(m_entity)),
			VAL_INIT);

		return true;
	}

	void Client::on_sync_connect(){
		PROFILE_ME;
		LOG_POSEIDON_INFO("CBPP client connected: remote = ", get_remote_info());
	}
}

}
