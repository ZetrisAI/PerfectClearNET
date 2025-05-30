#include "perfect_clear.hpp"

namespace finder {
    int extractLastHoldPriority(uint8_t priority, core::PieceType hold) {
        int slide = hold != core::PieceType::Empty ? hold : 7;
        uint8_t bit = priority >> static_cast<unsigned>(slide);
        return bit & 1U;
    }

    int compareToLastHoldPriority(uint8_t priority, int bestBit, core::PieceType newHold) {
        // Priority is given when the least significant bit is 1

        int newBit = extractLastHoldPriority(priority, newHold);

        // When the least significant bits are different
        if (0 < (newBit ^ bestBit)) {
            // Return true when giving priority to new
            return 0 < newBit ? 1 : -1;
        }

        return 0;
    }

    // For fast search
    void Recorder<FastCandidate, FastRecord>::clear() {
        best_ = FastRecord{
                std::vector<Operation>{},
                core::PieceType::Empty,
                INT_MAX,
                INT_MAX,
                INT_MAX,
                0,
                INT_MAX,
                INT_MAX,
                INT_MAX,
                INT_MAX,
                0,
                0,
                0,
        };
    }

    void Recorder<FastCandidate, FastRecord>::update(
            const Configure &configure, const FastCandidate &current, const Solution &solution
    ) {
        auto hold = 0 <= current.holdIndex ? configure.pieces[current.holdIndex] : core::PieceType::Empty;
        best_ = FastRecord{
                // new
                solution, hold, extractLastHoldPriority(configure.lastHoldPriority, hold),
                // from candidate
                current.currentIndex, current.holdIndex, current.leftLine, current.depth,
                current.softdropCount, current.holdCount, current.lineClearCount,
                current.currentCombo, current.maxCombo, current.frames
        };
    }

    void Recorder<FastCandidate, FastRecord>::update(const FastRecord &record) {
        best_ = FastRecord{record};
    }

    bool Recorder<FastCandidate, FastRecord>::isWorseThanBest(
            bool leastLineClears, const FastCandidate &current
    ) const {
        if (best_.holdPriority == 0) {
            return false;
        }

        return best_.softdropCount < current.softdropCount;
    }

	bool shouldUpdateFrames(
		const FastRecord& oldRecord, const FastCandidate& newRecord
	) {
		int newFrames = newRecord.holdCount + newRecord.frames;
		int oldFrames = oldRecord.holdCount + oldRecord.frames;

		return newFrames == oldFrames
			? newRecord.holdCount < oldRecord.holdCount
			: newFrames < oldFrames;
	}

    bool shouldUpdateLeastLineClear(
            const FastRecord &oldRecord, const FastCandidate &newRecord
    ) {
        if (newRecord.softdropCount != oldRecord.softdropCount) {
            return newRecord.softdropCount < oldRecord.softdropCount;
        }

        if (newRecord.lineClearCount != oldRecord.lineClearCount) {
            return newRecord.lineClearCount < oldRecord.lineClearCount;
        }

        return shouldUpdateFrames(oldRecord, newRecord);
    }

    bool shouldUpdateMostLineClear(
            const FastRecord &oldRecord, const FastCandidate &newRecord
    ) {
        if (newRecord.softdropCount != oldRecord.softdropCount) {
            return newRecord.softdropCount < oldRecord.softdropCount;
        }

        if (newRecord.maxCombo != oldRecord.maxCombo) {
            return oldRecord.maxCombo < newRecord.maxCombo;
        }

        if (newRecord.lineClearCount != oldRecord.lineClearCount) {
            return oldRecord.lineClearCount < newRecord.lineClearCount;
        }

        return shouldUpdateFrames(oldRecord, newRecord);
    }

    bool Recorder<FastCandidate, FastRecord>::shouldUpdate(
            const Configure &configure, const FastCandidate &newRecord
    ) const {
        if (best_.solution.empty()) {
            return true;
        }

        core::PieceType newHold = 0 <= newRecord.holdIndex
                                  ? configure.pieces[newRecord.holdIndex]
                                  : core::PieceType::Empty;
        auto compare = compareToLastHoldPriority(configure.lastHoldPriority, best_.holdPriority, newHold);
        if (compare != 0) {
            return 0 < compare;
        }

        if (configure.leastLineClears) {
            return shouldUpdateLeastLineClear(best_, newRecord);
        } else {
            return shouldUpdateMostLineClear(best_, newRecord);
        }
    }


    // For T-Spin search
    void Recorder<TSpinCandidate, TSpinRecord>::clear() {
        best_ = TSpinRecord{
                std::vector<Operation>{},
                core::PieceType::Empty,
                INT_MAX,
                INT_MAX,
                INT_MAX,
                0,
                INT_MAX,
                INT_MAX,
                INT_MAX,
                INT_MAX,
                0,
                0,
                0,
                false,
                0,
                0,
        };
    }

    void Recorder<TSpinCandidate, TSpinRecord>::update(
            const Configure &configure, const TSpinCandidate &current, const Solution &solution
    ) {
        auto hold = 0 <= current.holdIndex ? configure.pieces[current.holdIndex] : core::PieceType::Empty;
        best_ = TSpinRecord{
                // new
                solution, hold, extractLastHoldPriority(configure.lastHoldPriority, hold),
                // from candidate
                current.currentIndex, current.holdIndex, current.leftLine, current.depth,
                current.softdropCount, current.holdCount, current.lineClearCount,
                current.currentCombo, current.maxCombo, current.tSpinAttack, current.b2b, current.leftNumOfT,
        };
    }

    void Recorder<TSpinCandidate, TSpinRecord>::update(const TSpinRecord &record) {
        best_ = TSpinRecord{record};
    }

    bool Recorder<TSpinCandidate, TSpinRecord>::isWorseThanBest(
            bool leastLineClears, const TSpinCandidate &current
    ) const {
        if (best_.holdPriority == 0) {
            return false;
        }

        if (current.leftNumOfT == 0) {
            if (current.tSpinAttack != best_.tSpinAttack) {
                return current.tSpinAttack < best_.tSpinAttack;
            }

            return best_.softdropCount < current.softdropCount;
        }

        return false;
    }

	bool shouldUpdateFrames(
		const TSpinRecord& oldRecord, const TSpinCandidate& newRecord
	) {
		int newFrames = newRecord.holdCount + newRecord.frames;
		int oldFrames = oldRecord.holdCount + oldRecord.frames;

		return newFrames == oldFrames
			? newRecord.holdCount < oldRecord.holdCount
			: newFrames < oldFrames;
	}

    bool shouldUpdateLeastLineClear(
            const TSpinRecord &oldRecord, const TSpinCandidate &newRecord
    ) {
        if (newRecord.tSpinAttack != oldRecord.tSpinAttack) {
            return oldRecord.tSpinAttack < newRecord.tSpinAttack;
        }

        if (newRecord.softdropCount != oldRecord.softdropCount) {
            return newRecord.softdropCount < oldRecord.softdropCount;
        }

        if (newRecord.lineClearCount != oldRecord.lineClearCount) {
            return newRecord.lineClearCount < oldRecord.lineClearCount;
        }

        return shouldUpdateFrames(oldRecord, newRecord);
    }

    bool shouldUpdateMostLineClear(
            const TSpinRecord &oldRecord, const TSpinCandidate &newRecord
    ) {
        if (newRecord.tSpinAttack != oldRecord.tSpinAttack) {
            return oldRecord.tSpinAttack < newRecord.tSpinAttack;
        }

        if (newRecord.softdropCount != oldRecord.softdropCount) {
            return newRecord.softdropCount < oldRecord.softdropCount;
        }

        if (newRecord.maxCombo != oldRecord.maxCombo) {
            return oldRecord.maxCombo < newRecord.maxCombo;
        }

        if (newRecord.lineClearCount != oldRecord.lineClearCount) {
            return oldRecord.lineClearCount < newRecord.lineClearCount;
        }

        return shouldUpdateFrames(oldRecord, newRecord);
    }

    bool Recorder<TSpinCandidate, TSpinRecord>::shouldUpdate(
            const Configure &configure, const TSpinCandidate &newRecord
    ) const {
        if (best_.solution.empty()) {
            return true;
        }

        core::PieceType newHold = 0 <= newRecord.holdIndex
                                  ? configure.pieces[newRecord.holdIndex]
                                  : core::PieceType::Empty;
        auto compare = compareToLastHoldPriority(configure.lastHoldPriority, best_.holdPriority, newHold);
        if (compare != 0) {
            return 0 < compare;
        }

        if (configure.leastLineClears) {
            return shouldUpdateLeastLineClear(best_, newRecord);
        } else {
            return shouldUpdateMostLineClear(best_, newRecord);
        }
    }


    // For all spins search
    void Recorder<AllSpinsCandidate, AllSpinsRecord>::clear() {
        best_ = AllSpinsRecord{
                std::vector<Operation>{},
                core::PieceType::Empty,
                INT_MAX,
                INT_MAX,
                INT_MAX,
                0,
                INT_MAX,
                INT_MAX,
                INT_MAX,
                INT_MAX,
                0,
                0,
                0,
                false,
                0,
        };
    }

    void Recorder<AllSpinsCandidate, AllSpinsRecord>::update(
            const Configure &configure, const AllSpinsCandidate &current, const Solution &solution
    ) {
        auto hold = 0 <= current.holdIndex ? configure.pieces[current.holdIndex] : core::PieceType::Empty;
        best_ = AllSpinsRecord{
                // new
                solution, hold, extractLastHoldPriority(configure.lastHoldPriority, hold),
                // from candidate
                current.currentIndex, current.holdIndex, current.leftLine, current.depth,
                current.softdropCount, current.holdCount, current.lineClearCount,
                current.currentCombo, current.maxCombo, current.spinAttack, current.b2b, current.frames
        };
    }

    void Recorder<AllSpinsCandidate, AllSpinsRecord>::update(const AllSpinsRecord &record) {
        best_ = AllSpinsRecord{record};
    }

    bool Recorder<AllSpinsCandidate, AllSpinsRecord>::isWorseThanBest(
            bool leastLineClears, const AllSpinsCandidate &current
    ) const {
        // There is a high possibility of spin attack until the last piece. so, it's difficult to prune along the way
        return false;
    }

	bool shouldUpdateFrames(
		const AllSpinsRecord& oldRecord, const AllSpinsCandidate& newRecord
	) {
		int newFrames = newRecord.holdCount + newRecord.frames;
		int oldFrames = oldRecord.holdCount + oldRecord.frames;

		return newFrames == oldFrames
			? newRecord.holdCount < oldRecord.holdCount
			: newFrames < oldFrames;
	}

    bool shouldUpdateLeastLineClear(
            const AllSpinsRecord &oldRecord, const AllSpinsCandidate &newRecord
    ) {
        if (newRecord.spinAttack != oldRecord.spinAttack) {
            return oldRecord.spinAttack < newRecord.spinAttack;
        }

        if (newRecord.softdropCount != oldRecord.softdropCount) {
            return newRecord.softdropCount < oldRecord.softdropCount;
        }

        if (newRecord.lineClearCount != oldRecord.lineClearCount) {
            return newRecord.lineClearCount < oldRecord.lineClearCount;
        }

        return shouldUpdateFrames(oldRecord, newRecord);
    }

    bool shouldUpdateMostLineClear(
            const AllSpinsRecord &oldRecord, const AllSpinsCandidate &newRecord
    ) {
        if (newRecord.spinAttack != oldRecord.spinAttack) {
            return oldRecord.spinAttack < newRecord.spinAttack;
        }

        if (newRecord.softdropCount != oldRecord.softdropCount) {
            return newRecord.softdropCount < oldRecord.softdropCount;
        }

        if (newRecord.maxCombo != oldRecord.maxCombo) {
            return oldRecord.maxCombo < newRecord.maxCombo;
        }

        if (newRecord.lineClearCount != oldRecord.lineClearCount) {
            return oldRecord.lineClearCount < newRecord.lineClearCount;
        }

        return shouldUpdateFrames(oldRecord, newRecord);
    }

    bool Recorder<AllSpinsCandidate, AllSpinsRecord>::shouldUpdate(
            const Configure &configure, const AllSpinsCandidate &newRecord
    ) const {
        if (best_.solution.empty()) {
            return true;
        }

        core::PieceType newHold = 0 <= newRecord.holdIndex
                                  ? configure.pieces[newRecord.holdIndex]
                                  : core::PieceType::Empty;
        auto compare = compareToLastHoldPriority(configure.lastHoldPriority, best_.holdPriority, newHold);
        if (compare != 0) {
            return 0 < compare;
        }

        if (configure.leastLineClears) {
            return shouldUpdateLeastLineClear(best_, newRecord);
        } else {
            return shouldUpdateMostLineClear(best_, newRecord);
        }
    }
    
    // For TETR.IO Season 2 search
    void Recorder<TETRIOS2Candidate, TETRIOS2Record>::clear() {
        best_ = TETRIOS2Record{
                std::vector<Operation>{},
                core::PieceType::Empty,
                INT_MAX,
                INT_MAX,
                INT_MAX,
                0,
                INT_MAX,
                INT_MAX,
                INT_MAX,
                INT_MAX,
                0,
                0,
                0,
                0,
                0,
				false,
                false,
        };
    }

    void Recorder<TETRIOS2Candidate, TETRIOS2Record>::update(
            const Configure &configure, const TETRIOS2Candidate &current, const Solution &solution
    ) {
        auto hold = 0 <= current.holdIndex ? configure.pieces[current.holdIndex] : core::PieceType::Empty;
        best_ = TETRIOS2Record{
                // new
                solution, hold, extractLastHoldPriority(configure.lastHoldPriority, hold),
                // from candidate
                current.currentIndex, current.holdIndex, current.leftLine, current.depth,
                current.softdropCount, current.holdCount, current.lineClearCount, current.currentCombo,
				current.maxCombo, current.spinAttack, current.b2b, current.frames, current.isClean, current.isFlatI
        };
    }

    void Recorder<TETRIOS2Candidate, TETRIOS2Record>::update(const TETRIOS2Record &record) {
        best_ = TETRIOS2Record{record};
    }

    bool Recorder<TETRIOS2Candidate, TETRIOS2Record>::isWorseThanBest(
            bool leastLineClears, const TETRIOS2Candidate &current
    ) const {
        // There is a high possibility of spin attack until the last piece. so, it's difficult to prune along the way
        return false;
    }

	bool shouldUpdateFrames(
		const TETRIOS2Record& oldRecord, const TETRIOS2Candidate& newRecord
	) {
		int newFrames = newRecord.holdCount + newRecord.frames;
		int oldFrames = oldRecord.holdCount + oldRecord.frames;

		return newFrames == oldFrames
			? newRecord.holdCount < oldRecord.holdCount
			: newFrames < oldFrames;
	}

    bool shouldUpdateMostLineClear(
            const TETRIOS2Record &oldRecord, const TETRIOS2Candidate &newRecord
    ) {
        // non-Spin endings result in really hard to deal with boards if we are forced to tank garbage
        // so we really really really never want to take one of those unless it's the only option
		// flat I ending boards are not as bad and they allow for 4 B2B per 4L PC so they are worth it
		bool newIsSafe = newRecord.isClean || newRecord.isFlatI;
		bool oldIsSafe = oldRecord.isClean || oldRecord.isFlatI;

		if (newIsSafe != oldIsSafe) {
			return newIsSafe;
		}

        // prefer larger B2B per PC
        if (newRecord.b2b != oldRecord.b2b) {
			return oldRecord.b2b < newRecord.b2b;
        }

        // prefer larger spin attack, but add slight preference for keeping a clean ending
        int newScore = newRecord.spinAttack + (newRecord.isClean ? 2 : 0);
		int oldScore = oldRecord.spinAttack + (oldRecord.isClean ? 2 : 0);

		if (newScore != oldScore) {
			return oldScore < newScore;
		}

        if (newRecord.isClean != oldRecord.isClean) {
            return newRecord.isClean;
        }

        if (newRecord.spinAttack != oldRecord.spinAttack) {
            return oldRecord.spinAttack < newRecord.spinAttack;
        }

        if (newRecord.maxCombo != oldRecord.maxCombo) {
            return oldRecord.maxCombo < newRecord.maxCombo;
        }

        if (newRecord.lineClearCount != oldRecord.lineClearCount) {
            return oldRecord.lineClearCount < newRecord.lineClearCount;
        }

        // Irrelevant for TETR.IO
        //if (newRecord.softdropCount != oldRecord.softdropCount) {
        //    return newRecord.softdropCount < oldRecord.softdropCount;
        //}

        return shouldUpdateFrames(oldRecord, newRecord);
    }

    bool Recorder<TETRIOS2Candidate, TETRIOS2Record>::shouldUpdate(
            const Configure &configure, const TETRIOS2Candidate &newRecord
    ) const {
        if (best_.solution.empty()) {
            return true;
        }

        core::PieceType newHold = 0 <= newRecord.holdIndex
                                  ? configure.pieces[newRecord.holdIndex]
                                  : core::PieceType::Empty;
        auto compare = compareToLastHoldPriority(configure.lastHoldPriority, best_.holdPriority, newHold);
        if (compare != 0) {
            return 0 < compare;
        }

        // always want to do MostLineClear
        return shouldUpdateMostLineClear(best_, newRecord);
    }
}