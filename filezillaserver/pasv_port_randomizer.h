#ifndef FZS_PASV_PORT_RANDOMIZER_HEADER
#define FZS_PASV_PORT_RANDOMIZER_HEADER

#include <atomic>

/*
FTP suffers from connection stealing attacks. The only actual solution
to this problem that prevents all attacks is TLS session resumption.

To mitigate (but not eliminate) this problem when using plaintext sessions,
the passive mode port should at least be randomized.

The randomizer picks a random free port from the assigned passive mode range.

If there is no free port, it picks a used port with a different peer, provided
said port is not in the listening stage.

As last resort, it reuses a busy port from the same peer.

*/
class PasvPortManager;
class PortLease final
{
public:
	PortLease(){};

	PortLease(PortLease && lease);
	PortLease& operator=(PortLease && lease);

	~PortLease();

	PortLease(PortLease const&) = delete;
	PortLease& operator=(PortLease const&) = delete;

	unsigned int GetPort() const { return port_; }

	void SetConnected();
private:
	friend class PasvPortRandomizer;
	friend class PasvPortManager;

	PortLease(unsigned int p, std::wstring const& peer, PasvPortManager & manager);

	unsigned int port_{};
	std::wstring peerIP_;
	PasvPortManager * portManager_{};
	bool connected_{};
};

class COptions;

class PasvPortRandomizer final
{
public:
	explicit PasvPortRandomizer(PasvPortManager & manager, std::wstring const& peerIP, COptions& options);

	PasvPortRandomizer(PasvPortRandomizer const&) = delete;
	PasvPortRandomizer& operator=(PasvPortRandomizer const&) = delete;

	PortLease GetPort();

private:
	unsigned int DoGetPort();

	unsigned int min_{};
	unsigned int max_{};

	unsigned int first_port_{};
	unsigned int prev_port_{};

	bool allow_reuse_other_{};
	bool allow_reuse_same_{};

	std::wstring const peerIP_;

	PasvPortManager& manager_;
};


class COptions;
class PasvPortManager final
{
public:
	PasvPortManager(){};
	PasvPortManager(PasvPortManager const&) = delete;
	PasvPortManager& operator=(PasvPortManager const&) = delete;

private:
	friend class PortLease;
	friend class PasvPortRandomizer;

	void Release(unsigned int p, std::wstring const& peer, bool connected);
	void SetConnected(unsigned int p, std::wstring const& peer);

	void Prune(unsigned int port, uint64_t now);

	struct entry
	{
		std::wstring peer_;
		unsigned int leases_{};
		uint64_t expiry_{};
	};
	std::mutex mutex_[65536];
	std::vector<entry> entries_[65536];
	std::atomic_char connecting_[65536];
};

#endif