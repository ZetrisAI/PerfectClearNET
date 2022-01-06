#ifndef FINDER_CONCURRENT_PERFECT_CLEAR_HPP
#define FINDER_CONCURRENT_PERFECT_CLEAR_HPP

#include "types.hpp"
#include "perfect_clear.hpp"
#include "thread_pool.hpp"

#include "../core/moves.hpp"

namespace finder {
    // Entry point to find best perfect clear
    template<class M = core::srs::MoveGenerator>
    class ConcurrentPerfectClearFinder {
    public:
        ConcurrentPerfectClearFinder<M>(const core::Factory &factory, ThreadPool &threadPool)
                : factory_(factory), threadPool_(threadPool),
                  moveGenerator_(M(factory)), reachable_(core::srs_rotate_end::Reachable(factory)) {
        }

        // If `alwaysRegularAttack` is true, mini spin is judged as regular attack
        Solution run(
                const core::Field &field, const std::vector<core::PieceType> &pieces,
                int maxDepth, int maxLine, bool holdEmpty, bool holdAllowed, bool leastLineClears, 
                SearchTypes searchTypes, int initCombo, bool initB2b, bool alwaysRegularAttack, uint8_t lastHoldPriority, int fastSearchStartDepth
        ) {
            if (maxDepth == 1) {
                auto moveGenerator = M(factory_);
                auto finder = PerfectClearFinder<M>(factory_, moveGenerator);
                return finder.run(
                        field, pieces, maxDepth, maxLine, holdEmpty, holdAllowed, leastLineClears,
						searchTypes, initCombo, initB2b, alwaysRegularAttack, lastHoldPriority, fastSearchStartDepth
                );
            }

            assert(1 < maxDepth);

            abort();

            // Copy field
            auto freeze = core::Field(field);

            // original
            // Initialize configure
            auto movePool = std::vector<std::vector<core::Move>>{};
            auto scoredMovePool = std::vector<std::vector<core::ScoredMove>>{};
            const auto originalConfigure = Configure{
                    pieces,
                    movePool,
                    scoredMovePool,
                    maxDepth,
                    fastSearchStartDepth,
                    static_cast<int>(pieces.size()),
					holdAllowed,
                    leastLineClears,
                    alwaysRegularAttack,
                    lastHoldPriority,
            };

            // Reset status
            runnerStatus_.resume();

            switch (searchTypes) {
                case SearchTypes::Fast: {
                    using Candidate = FastCandidate;
                    using Record = FastRecord;

                    // Create candidate
                    auto candidate = holdEmpty
                                     ? Candidate{0, -1, maxLine, 0, 0, 0, 0,
                                                 initCombo, initCombo, 0}
                                     : Candidate{1, 0, maxLine, 0, 0, 0, 0,
                                                 initCombo, initCombo, 0};

                    // premove
                    auto moves = std::vector<core::Move>{};
                    auto preOperations = std::vector<PreOperation<Candidate>>{};
                    premove(
							originalConfigure, freeze, candidate,
                            moveGenerator_, reachable_, moves, preOperations
                    );

                    // Find solution by concurrent
                    Recorder<Candidate, Record> recorder{};
                    boost::mutex mutex;

                    auto futures = std::vector<boost::future<bool>>(preOperations.size());

                    for (int index = 0; index < futures.size(); ++index) {
                        auto preOperation = preOperations[index];
                        Callable<bool> callable = [&, preOperation](const TaskStatus &taskStatus) {
                            if (taskStatus.notWorking()) {
                                return false;
                            }

                            // Initialize moves
                            auto movePool = std::vector<std::vector<core::Move>>{};
                            for (int index = 0; index < maxDepth; ++index) {
                                movePool.emplace_back();
                            }

                            auto scoredMovePool = std::vector<std::vector<core::ScoredMove>>{};
                            for (int index = 0; index < maxDepth; ++index) {
                                scoredMovePool.emplace_back();
                            }

                            // Initialize configure
                            const auto configure = Configure{
                                    pieces,
                                    movePool,
                                    scoredMovePool,
                                    maxDepth,
                                    fastSearchStartDepth,
                                    static_cast<int>(pieces.size()),
									holdAllowed,
                                    leastLineClears,
                                    alwaysRegularAttack,
                                    lastHoldPriority,
                            };

                            auto moveGenerator = M(factory_);
                            auto reachable = core::srs_rotate_end::Reachable(factory_);
                            auto finder = PCFindRunner<M, Candidate, Record>(
                                    factory_, moveGenerator, reachable
                            );

                            Record record;
                            {
                                boost::lock_guard<boost::mutex> guard(mutex);
                                record = recorder.best();
                            }

                            if (record.solution.empty()) {
                                record = finder.runRecord(configure, preOperation.field, preOperation.candidate);
                            } else {
                                record = finder.runRecord(
                                        configure, preOperation.field, preOperation.candidate, record
                                );
                            }

                            if (record.solution.empty()) {
                                return false;
                            }

                            record.solution[0].pieceType = preOperation.pieceType;
                            record.solution[0].rotateType = preOperation.rotateType;
                            record.solution[0].x = preOperation.x;
                            record.solution[0].y = preOperation.y;

                            auto newRecord = Candidate{
                                    record.currentIndex,
                                    record.holdIndex, record.leftLine,
                                    record.depth, record.softdropCount,
                                    record.holdCount, record.lineClearCount,
                                    record.currentCombo, record.maxCombo,
                                    record.frames
                            };

                            {
                                boost::lock_guard<boost::mutex> guard(mutex);
                                if (recorder.shouldUpdate(originalConfigure, newRecord)) {
                                    recorder.update(originalConfigure, newRecord, record.solution);
                                }
                            }

                            return true;
                        };
                        futures[index] = threadPool_.execute(callable);
                    }

                    // Wait
                    for (auto &future : futures) {
                        future.get();
                    }

                    // Return solution
                    auto best = recorder.best();
                    return best.solution.empty() ? kNoSolution : std::vector<Operation>(best.solution);
                }
                case SearchTypes::TSpin: {
                    assert(!alwaysRegularAttack);  // Support no mini only

                    using Candidate = TSpinCandidate;
                    using Record = TSpinRecord;

                    // Count up T
                    int leftNumOfT = std::count(pieces.begin(), pieces.end(), core::PieceType::T);

                    // Create candidate
                    auto candidate = holdEmpty
                                     ? Candidate{0, -1, maxLine, 0, 0, 0, 0,
                                                 initCombo, initCombo, 0, initB2b, leftNumOfT, 0}
                                     : Candidate{1, 0, maxLine, 0, 0, 0, 0,
                                                 initCombo, initCombo, 0, initB2b, leftNumOfT, 0};

                    // premove
                    auto moves = std::vector<core::Move>{};
                    auto firstCandidates = std::vector<PreOperation<Candidate>>{};
                    premove(
							originalConfigure, freeze, candidate,
                            moveGenerator_, reachable_, moves, firstCandidates
                    );

                    // Find solution by concurrent
                    Recorder<Candidate, Record> recorder{};
                    boost::mutex mutex;

                    auto futures = std::vector<boost::future<bool>>(firstCandidates.size());

                    for (int index = 0; index < futures.size(); ++index) {
                        auto preOperation = firstCandidates[index];
                        Callable<bool> callable = [&, preOperation](const TaskStatus &taskStatus) {
                            if (taskStatus.notWorking()) {
                                return false;
                            }

                            // Initialize moves
                            auto movePool = std::vector<std::vector<core::Move>>{};
                            for (int index = 0; index < maxDepth; ++index) {
                                movePool.emplace_back();
                            }

                            auto scoredMovePool = std::vector<std::vector<core::ScoredMove>>{};
                            for (int index = 0; index < maxDepth; ++index) {
                                scoredMovePool.emplace_back();
                            }

                            // Initialize configure
                            const auto configure = Configure{
                                    pieces,
                                    movePool,
                                    scoredMovePool,
                                    maxDepth,
                                    fastSearchStartDepth,
                                    static_cast<int>(pieces.size()),
									holdAllowed,
                                    leastLineClears,
                                    alwaysRegularAttack,
                                    lastHoldPriority,
                            };

                            auto moveGenerator = M(factory_);
                            auto reachable = core::srs_rotate_end::Reachable(factory_);
                            auto finder = PCFindRunner<M, Candidate, Record>(
									factory_, moveGenerator, reachable
							);

                            Record record;
                            {
                                boost::lock_guard<boost::mutex> guard(mutex);
                                record = recorder.best();
                            }

                            if (record.solution.empty()) {
                                record = finder.runRecord(configure, preOperation.field, preOperation.candidate);
                            } else {
                                record = finder.runRecord(
                                        configure, preOperation.field, preOperation.candidate, record
                                );
                            }

                            if (record.solution.empty()) {
                                return false;
                            }

                            record.solution[0].pieceType = preOperation.pieceType;
                            record.solution[0].rotateType = preOperation.rotateType;
                            record.solution[0].x = preOperation.x;
                            record.solution[0].y = preOperation.y;

                            auto newRecord = Candidate{
                                    record.currentIndex,
                                    record.holdIndex, record.leftLine,
                                    record.depth, record.softdropCount,
                                    record.holdCount, record.lineClearCount,
                                    record.currentCombo, record.maxCombo,
                                    record.tSpinAttack, record.b2b,
                                    record.leftNumOfT, record.frames
                            };

                            {
                                boost::lock_guard<boost::mutex> guard(mutex);
                                if (recorder.shouldUpdate(originalConfigure, newRecord)) {
                                    recorder.update(originalConfigure, newRecord, record.solution);
                                }
                            }

                            return true;
                        };
                        futures[index] = threadPool_.execute(callable);
                    }

                    // Wait
                    for (auto &future : futures) {
                        future.get();
                    }

                    // Return solution
                    auto best = recorder.best();
                    return best.solution.empty() ? kNoSolution : std::vector<Operation>(best.solution);
                }
                case SearchTypes::AllSpins: {
                    using Candidate = AllSpinsCandidate;
                    using Record = AllSpinsRecord;

                    // Create candidate
                    auto candidate = holdEmpty
                                     ? Candidate{0, -1, maxLine, 0, 0, 0, 0,
                                                 initCombo, initCombo, 0, initB2b, 0}
                                     : Candidate{1, 0, maxLine, 0, 0, 0, 0,
                                                 initCombo, initCombo, 0, initB2b, 0};

                    // premove
                    auto moves = std::vector<core::Move>{};
                    auto firstCandidates = std::vector<PreOperation<Candidate>>{};
                    premove(
							originalConfigure, freeze, candidate,
                            moveGenerator_, reachable_, moves, firstCandidates
                    );

                    // Find solution by concurrent
                    Recorder<Candidate, Record> recorder{};
                    boost::mutex mutex;

                    auto futures = std::vector<boost::future<bool>>(firstCandidates.size());

                    for (int index = 0; index < futures.size(); ++index) {
                        auto preOperation = firstCandidates[index];
                        Callable<bool> callable = [&, preOperation](const TaskStatus &taskStatus) {
                            if (taskStatus.notWorking()) {
                                return false;
                            }

                            // Initialize moves
                            auto movePool = std::vector<std::vector<core::Move>>{};
                            for (int index = 0; index < maxDepth; ++index) {
                                movePool.emplace_back();
                            }

                            auto scoredMovePool = std::vector<std::vector<core::ScoredMove>>{};
                            for (int index = 0; index < maxDepth; ++index) {
                                scoredMovePool.emplace_back();
                            }

                            // Initialize configure
                            const auto configure = Configure{
                                    pieces,
                                    movePool,
                                    scoredMovePool,
                                    maxDepth,
                                    fastSearchStartDepth,
                                    static_cast<int>(pieces.size()),
									holdAllowed,
                                    leastLineClears,
                                    alwaysRegularAttack,
                                    lastHoldPriority,
                            };

                            auto moveGenerator = M(factory_);
                            auto reachable = core::srs_rotate_end::Reachable(factory_);
                            auto finder = PCFindRunner<M, Candidate, Record>(
                                    factory_, moveGenerator, reachable
                            );

                            Record record;
                            {
                                boost::lock_guard<boost::mutex> guard(mutex);
                                record = recorder.best();
                            }

                            if (record.solution.empty()) {
                                record = finder.runRecord(configure, preOperation.field, preOperation.candidate);
                            } else {
                                record = finder.runRecord(
                                        configure, preOperation.field, preOperation.candidate, record
                                );
                            }

                            if (record.solution.empty()) {
                                return false;
                            }

                            record.solution[0].pieceType = preOperation.pieceType;
                            record.solution[0].rotateType = preOperation.rotateType;
                            record.solution[0].x = preOperation.x;
                            record.solution[0].y = preOperation.y;

                            auto newRecord = Candidate{
                                    record.currentIndex,
                                    record.holdIndex, record.leftLine,
                                    record.depth, record.softdropCount,
                                    record.holdCount, record.lineClearCount,
                                    record.currentCombo, record.maxCombo,
                                    record.spinAttack, record.b2b,
                                    record.frames
                            };

                            {
                                boost::lock_guard<boost::mutex> guard(mutex);
                                if (recorder.shouldUpdate(originalConfigure, newRecord)) {
                                    recorder.update(originalConfigure, newRecord, record.solution);
                                }
                            }

                            return true;
                        };
                        futures[index] = threadPool_.execute(callable);
                    }

                    // Wait
                    for (auto &future : futures) {
                        future.get();
                    }

                    // Return solution
                    auto best = recorder.best();
                    return best.solution.empty() ? kNoSolution : std::vector<Operation>(best.solution);
                }
                default: {
                    assert(false);
                    throw std::runtime_error("Illegal search types: value=" + std::to_string(searchTypes));
                }
            }
        }

        // searchType refers to code
        Solution run(
                const core::Field &field, const std::vector<core::PieceType> &pieces,
                int maxLine, bool holdEmpty, bool holdAllowed, bool leastLineClears, int searchType,
                int initCombo, bool initB2b, bool twoLineFollowUp, int numApplyFastSearch
        ) {
            int numOfSpace = core::FIELD_WIDTH * maxLine - field.getNumOfBlocks();
            if (numOfSpace % 4 != 0) {
                return kNoSolution;
            }

            int maxDepth = numOfSpace / 4;

            // Check last hold that can take 2 PC
            uint8_t lastHoldPriority = 0U;
            if (maxDepth + 5 <= pieces.size() && twoLineFollowUp) {
                std::vector<core::PieceType> nextPieces(pieces.cbegin() + maxDepth, pieces.cend());
                if (holdEmpty && canTake2LinePC(nextPieces)) {
                    lastHoldPriority |= 0b10000000U;
                }

                for (unsigned int pieceType = 0; pieceType < 7; ++pieceType) {
                    nextPieces[0] = static_cast<core::PieceType>(pieceType);
                    if (canTake2LinePC(nextPieces)) {
                        lastHoldPriority |= 1U << pieceType;
                    }
                }
            }

            if (lastHoldPriority == 0U) {
                lastHoldPriority = 0b11111111U;
            }

            int fastSearchStartDepth = numApplyFastSearch < maxDepth ? maxDepth - numApplyFastSearch : 0;

            // Decide parameters
            switch (searchType) {
				case 0: {
					// No softdrop is top priority
					return run(
						field, pieces, maxDepth, maxLine, holdEmpty, holdAllowed, leastLineClears, SearchTypes::Fast, initCombo, initB2b,
						false, lastHoldPriority, fastSearchStartDepth
					);
				}
				case 1: {
					// T-Spin is top priority (mini is zero attack)
					return run(
						field, pieces, maxDepth, maxLine, holdEmpty, holdAllowed, leastLineClears, SearchTypes::TSpin, initCombo, initB2b,
						false, lastHoldPriority, fastSearchStartDepth
					);
				}
				case 2: {
					// All-Spins is top priority (all spins are judged as regular attack)
					return run(
						field, pieces, maxDepth, maxLine, holdEmpty, holdAllowed, leastLineClears, SearchTypes::AllSpins, initCombo, initB2b,
						true, lastHoldPriority, fastSearchStartDepth
					);
				}
				case 3: {
					// All-Spins is top priority (mini is zero attack)
					return run(
						field, pieces, maxDepth, maxLine, holdEmpty, holdAllowed, leastLineClears, SearchTypes::AllSpins, initCombo, initB2b,
						false, lastHoldPriority, fastSearchStartDepth
					);
				}
                default: {
                    throw std::runtime_error("Illegal search type: value=" + std::to_string(searchType));
                }
            }
        }

		Solution run(
                const core::Field &field, const std::vector<core::PieceType> &pieces,
                int maxLine, bool holdEmpty, bool holdAllowed, int searchType, bool leastLineClears,
                int initCombo, bool initB2b, bool twoLineFollowUp
        ) {
            return run(
                    field, pieces, maxLine, holdEmpty, holdAllowed, leastLineClears, searchType,
                    initCombo, initB2b, twoLineFollowUp, INT_MAX
            );
        }

        Solution run(
                const core::Field &field, const std::vector<core::PieceType> &pieces,
				int maxLine, bool holdEmpty, bool holdAllowed
        ) {
            return run(
                    field, pieces, maxLine, holdEmpty, holdAllowed, SearchTypes::TSpin, true, 0, true, false
            );
        }

        void abort() {
            runnerStatus_.abort();
            threadPool_.abort();
        }

    private:
        template<class C>
        void premove(
                const Configure &configure,
                const core::Field &field,
                const C &candidate,
                M &moveGenerator,
                core::srs_rotate_end::Reachable &reachable,
                std::vector<core::Move> &moves,
                std::vector<PreOperation<C>> &output
        ) const {
            auto mover = Mover<M, C>(factory_, moveGenerator, reachable);

            int pieceSize = configure.pieces.size();

            auto currentIndex = candidate.currentIndex;
            assert(0 <= currentIndex && currentIndex <= pieceSize);
            auto holdIndex = candidate.holdIndex;
            assert(-1 <= holdIndex && holdIndex < pieceSize);

            auto holdCount = candidate.holdCount;

            bool canUseCurrent = currentIndex < pieceSize;
            if (canUseCurrent) {
                assert(currentIndex < configure.pieces.size());
                auto &current = configure.pieces[currentIndex];

                moves.clear();
                mover.premove(
						configure.alwaysRegularAttack, configure.maxDepth, field, candidate,
                        moves, current, currentIndex + 1, holdIndex, holdCount, output
                );
            }

			if (configure.holdAllowed) {
				if (0 <= holdIndex) {
					assert(holdIndex < configure.pieces.size());

					// Hold exists
					if (!canUseCurrent || configure.pieces[currentIndex] != configure.pieces[holdIndex]) {
						auto& hold = configure.pieces[holdIndex];

						moves.clear();
						mover.premove(
							configure.alwaysRegularAttack, configure.maxDepth, field, candidate,
							moves, hold, currentIndex + 1, currentIndex, holdCount + 1, output
						);
					}
				}
				else {
					assert(canUseCurrent);

					// Empty hold
					int nextIndex = currentIndex + 1;
					assert(nextIndex < configure.pieces.size() + 1);

					if (nextIndex < pieceSize && configure.pieces[currentIndex] != configure.pieces[nextIndex]) {
						assert(nextIndex < configure.pieces.size());
						auto& next = configure.pieces[nextIndex];

						moves.clear();
						mover.premove(
							configure.alwaysRegularAttack, configure.maxDepth, field, candidate,
							moves, next, nextIndex + 1, currentIndex, holdCount + 1, output
						);
					}
				}
			}

            std::sort(output.begin(), output.end(), [](const PreOperation<C> &left, const PreOperation<C> &right) {
                return left.score < right.score;
            });
        }

        const core::Factory &factory_;
        ThreadPool &threadPool_;
        M moveGenerator_;
        core::srs_rotate_end::Reachable reachable_;
        RunnerStatus runnerStatus_{};
    };
}

#endif //FINDER_CONCURRENT_PERFECT_CLEAR_HPP