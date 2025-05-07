#include "Windows.h"
#define DLL extern "C" __declspec(dllexport)

#include <sstream>
#include <vector>

#include "callback.hpp"
#include "core/field.hpp"
#include "finder/thread_pool.hpp"
#include "finder/concurrent_perfect_clear.hpp"

static const unsigned char BitsSetTable256[256] =
{
#   define B2(n) n,     n+1,     n+1,     n+2
#   define B4(n) B2(n), B2(n+1), B2(n+1), B2(n+2)
#   define B6(n) B4(n), B4(n+1), B4(n+1), B4(n+2)
	B6(0), B6(1), B6(1), B6(2)
};

enum Game {
	None = 0,
	PPT = 1,
	TETRIO = 2
};

using PPTFinder = finder::ConcurrentPerfectClearFinder<false, true>;
using TETRIOFinder = finder::ConcurrentPerfectClearFinder<true, false>;

auto srs = core::Factory::create();
auto srsPlus = core::Factory::createForSRSPlus();

auto threadPool = finder::ThreadPool(1);

std::optional<PPTFinder> pptfinder;
std::optional<TETRIOFinder> tetriofinder;
Game game = Game::None;

DLL void set_abort(Callback handler) {
	Abort = handler;
}

// returns whether PC finder is inited or not
DLL bool init_finder(Game init) {
	if (game > Game::None) return true;

	if (init == Game::PPT) {
		pptfinder.emplace(srs, threadPool);
	} else if (init == Game::TETRIO) {
		tetriofinder.emplace(srsPlus, threadPool);
	} else {
		return false;
	}
	
	game = init;
	return true;
}

DLL void set_threads(unsigned int threads) {
	threadPool.changeThreadCount(threads);
}

core::PieceType charToPiece(char x) {
	switch (x) {
		case 'S':
			return core::PieceType::S;

		case 'Z':
			return core::PieceType::Z;

		case 'J':
			return core::PieceType::J;

		case 'L':
			return core::PieceType::L;

		case 'T':
			return core::PieceType::T;

		case 'O':
			return core::PieceType::O;

		case 'I':
			return core::PieceType::I;

		default:
			assert(true);
	}
}

DLL void action(
	const char* _field, const char* _queue, const char* _hold, int height,
	int max_height, bool swap, int searchtype, int combo, bool b2b, bool twoLine,
	char* _str, int _len
) {
	bool solved = false;
	std::stringstream out;

	if (game > Game::None) {
		auto field = core::createField(_field);

		int minos_placed = 0;

		for (core::Bitboard v : field.boards)
			minos_placed += BitsSetTable256[v & 0xff] +
			BitsSetTable256[(v >> 8) & 0xff] +
			BitsSetTable256[(v >> 16) & 0xff] +
			BitsSetTable256[(v >> 24) & 0xff] +
			BitsSetTable256[(v >> 32) & 0xff] +
			BitsSetTable256[(v >> 40) & 0xff] +
			BitsSetTable256[(v >> 48) & 0xff] +
			BitsSetTable256[v >> 56];

		if (minos_placed % 2 == 0) {
			if (max_height < 0) max_height = 0;
			if (max_height > 20) max_height = 20;

			auto pieces = std::vector<core::PieceType>();

			bool holdEmpty = _hold[0] == 'E';
			bool holdAllowed = _hold[0] != 'X';

			if (!holdEmpty)
				pieces.push_back(charToPiece(_hold[0]));

			int max_pieces = (max_height * 10 - minos_placed) / 4 + 1;

			for (int i = 0; i < max_pieces && _queue[i] != '\0'; i++)
				pieces.push_back(charToPiece(_queue[i]));

			// sus
			//height += minos_placed % 4 == (height % 2)? 0 : 2;

			// need to clear odd number of lines
			if (minos_placed % 4 == 2) {
				if (height % 2 == 0) {
					height += 1;
				}
			}
			// need to clear even number of lines
			else {
				if (height % 2 == 1) {
					height += 1;
				}
			}

			if (height == 0) {
				height = 2;
			}

			// for completely skipping two line PC search
			//if (!twoLine && height < 3) {
			//	height += 2;
			//}

			for (; height <= max_height; height += 2) {
				if ((height * 10 - minos_placed) / 4 + 1 > pieces.size()) break;

				auto result = game == Game::PPT
					? pptfinder->run(field, pieces, height, holdEmpty, holdAllowed, !swap, searchtype, combo, b2b, twoLine, 6)
					: (game == Game::TETRIO
						? tetriofinder->run(field, pieces, height, holdEmpty, holdAllowed, !swap, searchtype, combo, b2b, twoLine, 6)
						: finder::Solution() // empty solution
					);

				if (!result.empty()) {
					solved = true;

					for (const auto& item : result) {
						out << item.pieceType << ","
							<< item.x << ","
							<< item.y << ","
							<< item.rotateType << "|";
					}

					break;
				}
			}
		}
	}

	if (!solved) out << "-1";

	std::string a = out.str();
	std::copy(a.c_str(), a.c_str() + a.length() + 1, _str);
}

// Managed code may not be run under loader lock,
// including the DLL entrypoint and calls reached from the DLL entrypoint
#pragma managed(push, off)
BOOL WINAPI DllMain(HANDLE handle, DWORD reason, LPVOID reserved) {
	if (reason == DLL_PROCESS_DETACH)
		threadPool.shutdown();

	return TRUE;
}
#pragma managed(pop)
