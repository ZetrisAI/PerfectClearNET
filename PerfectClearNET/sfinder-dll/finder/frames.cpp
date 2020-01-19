#include "frames.hpp"

namespace finder {
    int getFrames(Operation& operation) {
		int x = operation.x;
		int r = operation.rotateType;

		if (operation.pieceType == core::PieceType::O) {
			r = 0;
		}

		if (operation.pieceType == core::PieceType::I) {
			if (r == 3 && x >= 5) r = 1;
			if (r == 1 && x <= 4) r = 3;

			if (operation.rotateType == 1 || operation.rotateType == 2) x--;
		}

		r = 2 - ((r + 2) % 4);
		int m = 4 - x;

		// note: we count the frame we hard drop on
		if (m == 0 && r == 0) {
			return 1;

		} else if (m >= r) {
			return m * 2;

		} else {
			return r * 2 - 1;
		}
    }
}
