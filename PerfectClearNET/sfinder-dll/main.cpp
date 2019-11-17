#include "main.hpp"

#pragma unmanaged

DLL void set_abort(Callback handler) {
	Abort = handler;
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

DLL void action(const char* _field, const char* _queue, const char* _hold, int height, int max_height, bool swap, int searchtype, int combo, char* _str, int _len) {
	auto field = core::createField(_field);

	if (max_height < 0) max_height = 0;
	if (max_height > 20) max_height = 20;

	auto pieces = std::vector<core::PieceType>();

	bool holdEmpty = _hold[0] == 'E';
	bool holdAllowed = _hold[0] != 'X';

	if (!holdEmpty)
		pieces.push_back(charToPiece(_hold[0]));
	
	int max_pieces = (5 * max_height + 3) >> 1;

	for (int i = 0; i < max_pieces && _queue[i] != '\0'; i++)
		pieces.push_back(charToPiece(_queue[i]));
		
	std::stringstream out;

	bool solved = false;

	for (int i = height; i <= max_height; i += 2) {
		auto result = pcfinder.run(field, pieces, pieces.size(), i, holdEmpty, holdAllowed, !swap, searchtype, combo);

		if (!result.empty()) {
			solved = true;

			for (const auto &item : result) {
				out << item.pieceType << ","
					<< item.x << ","
					<< item.y << ","
					<< item.rotateType << "|";
			}

			i = 100;
		}
	}

	if (!solved) out << "-1";

	std::string a = out.str();
	std::copy(a.c_str(), a.c_str() + a.length() + 1, _str);
}

BOOL WINAPI DllMain(HANDLE handle, DWORD reason, LPVOID reserved) {
	return TRUE;
}