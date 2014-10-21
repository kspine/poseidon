#ifndef POSEIDON_PLAYER_STATUS_HPP_
#define POSEIDON_PLAYER_STATUS_HPP_

namespace Poseidon {

enum PlayerStatus {
	PLAYER_OK					= 0,
	PLAYER_INTERNAL_ERROR		= 1,
	PLAYER_END_OF_STREAM		= 2,
	PLAYER_NOT_FOUND			= 3,
	PLAYER_REQUEST_TOO_LARGE	= 4,
};

}

#endif