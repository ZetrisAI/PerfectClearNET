#include "main.hpp"

#pragma unmanaged

DLL void set_abort(Callback handler) {
	Abort = handler;
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
	int max_height, bool swap, int searchtype, int combo, bool b2b,
	char* _str, int _len
) {

	bool solved = false;
	std::stringstream out;

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

		height += minos_placed % 4 == (height % 2)? 0 : 2;

		for (; height <= max_height; height += 2) {
			if ((height * 10 - minos_placed) / 4 + 1 > pieces.size()) break;

			auto result = pcfinder.run(field, pieces, height, holdEmpty, holdAllowed, !swap, searchtype, combo, b2b, true, 6);

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

	if (!solved) out << "-1";

	std::string a = out.str();
	std::copy(a.c_str(), a.c_str() + a.length() + 1, _str);
}

BOOL WINAPI DllMain(HANDLE handle, DWORD reason, LPVOID reserved) {
	if (reason == DLL_PROCESS_DETACH)
		threadPool.shutdown();

	return TRUE;
}