#ifndef FINDER_PERFECT_HPP
#define FINDER_PERFECT_HPP

#include <atomic>

#include "types.hpp"
#include "spins.hpp"
#include "two_lines_pc.hpp"
#include "frames.hpp"

#include "../core/piece.hpp"
#include "../core/moves.hpp"
#include "../core/types.hpp"

namespace finder {
    namespace {
        inline bool validate(const core::Field &field, int maxLine) {
            int sum = maxLine - field.getBlockOnX(0, maxLine);
            for (int x = 1; x < core::FIELD_WIDTH; x++) {
                int emptyCountInColumn = maxLine - field.getBlockOnX(x, maxLine);
                if (field.isWallBetween(x, maxLine)) {
                    if (sum % 4 != 0)
                        return false;
                    sum = emptyCountInColumn;
                } else {
                    sum += emptyCountInColumn;
                }
            }

            return sum % 4 == 0;
        }

        inline int calcScore(const core::Field &field, const bool harddrop) {
            return field.getNumOfHoles() * 10 + !harddrop;
        }

        inline void toScoredMove(
                const std::vector<core::Move> &moves,
                const core::Factory &factory, const core::PieceType pieceType, const core::Field &field,
                std::vector<core::ScoredMove> &scoredMoves
        ) {
            for (const auto &move : moves) {
                auto &blocks = factory.get(pieceType, move.rotateType);

                auto freeze = core::Field(field);
                freeze.put(blocks, move.x, move.y);

                int score = calcScore(freeze, move.harddrop);
                int numCleared = freeze.clearLineReturnNum();

                scoredMoves.push_back({
                                              freeze,
                                              move,
                                              numCleared,
                                              score,
                                      });
            }

            std::sort(scoredMoves.begin(), scoredMoves.end(),
                      [](const core::ScoredMove &first, const core::ScoredMove &second) {
                          return first.score < second.score;
                      });
        }
    }

    enum SearchTypes {
        Fast = 0,
        TSpin = 1,
        AllSpins = 2,
        TETRIOS2 = 3
    };

    template<bool Allow180, bool AllowSoftdropTap, class M, class C>
    class Mover;

    template<class C, class R>
    class Recorder;

    // Main finder implementation
    template<bool Allow180 = false, bool AllowSoftdropTap = true, class M = core::srs::MoveGenerator<Allow180, AllowSoftdropTap>, class C = TSpinCandidate, class R = TSpinRecord>
    class PCFindRunner {
    public:
        PCFindRunner(
                const core::Factory &factory, M &moveGenerator, core::srs_rotate_end::Reachable<Allow180, AllowSoftdropTap> &reachable
        ) : mover(Mover<Allow180, AllowSoftdropTap, M, C>(factory, moveGenerator, reachable)), recorder(Recorder<C, R>()) {}

        PCFindRunner(
                PCFindRunner &&rhs
        ) : mover(std::move(rhs.mover)), recorder(std::move(rhs.recorder)) {}

        Solution run(const Configure &configure, const core::Field &field, const C &candidate) {
            auto best = runRecord(configure, field, candidate);
            return best.solution.empty() ? kNoSolution : std::vector<Operation>(best.solution);
        }

        R runRecord(const Configure &configure, const core::Field &field, const C &candidate) {
            recorder.clear();

            // Initialize solution
            Solution solution(configure.maxDepth);
            std::fill(solution.begin(), solution.end(), Operation{
                    core::PieceType::T, core::RotateType::Spawn, -1, -1
            });

            // Execute
            search(configure, field, candidate, solution);

            return recorder.best();
        }

        R runRecord(const Configure &configure, const core::Field &field, const C &candidate, const R &initRecord) {
            recorder.update(initRecord);

            // Initialize solution
            Solution solution(configure.maxDepth);
            std::fill(solution.begin(), solution.end(), Operation{
                    core::PieceType::T, core::RotateType::Spawn, -1, -1
            });

            // Execute
            search(configure, field, candidate, solution);

            return recorder.best();
        }

        void search(const Configure &configure, const core::Field &field, const C &candidate, Solution &solution) {
            if (Abort() || recorder.isWorseThanBest(configure.leastLineClears, candidate)) {
                return;
            }

            auto depth = candidate.depth;

            auto &pieces = configure.pieces;
            auto &moves = configure.movePool[depth];
            auto &scoredMoves = configure.scoredMovePool[depth];

            auto currentIndex = candidate.currentIndex;
            assert(0 <= currentIndex && currentIndex <= configure.pieceSize);
            auto holdIndex = candidate.holdIndex;
            assert(-1 <= holdIndex && holdIndex < configure.pieceSize);

            auto holdCount = candidate.holdCount;

            bool canUseCurrent = currentIndex < configure.pieceSize;
            if (canUseCurrent) {
                assert(currentIndex < pieces.size());
                auto &current = pieces[currentIndex];

                moves.clear();
                scoredMoves.clear();
                mover.move(
                        configure, field, candidate, solution,
                        moves, scoredMoves, current, currentIndex + 1, holdIndex, holdCount, this
                );
            }

			if (!configure.holdAllowed) return;

            if (0 <= holdIndex) {
                assert(holdIndex < pieces.size());

                // Hold exists
                if (!canUseCurrent || pieces[currentIndex] != pieces[holdIndex]) {
                    auto &hold = pieces[holdIndex];

                    moves.clear();
                    scoredMoves.clear();
                    mover.move(
                            configure, field, candidate, solution,
                            moves, scoredMoves, hold, currentIndex + 1, currentIndex, holdCount + 1, this
                    );
                }
            } else {
                assert(canUseCurrent);

                // Empty hold
                int nextIndex = currentIndex + 1;
                assert(nextIndex < pieces.size() + 1);

                if (nextIndex < configure.pieceSize && pieces[currentIndex] != pieces[nextIndex]) {
                    assert(nextIndex < pieces.size());
                    auto &next = pieces[nextIndex];

                    moves.clear();
                    scoredMoves.clear();
                    mover.move(
                            configure, field, candidate, solution,
                            moves, scoredMoves, next, nextIndex + 1, currentIndex, holdCount + 1, this
                    );
                }
            }
        }

        void accept(const Configure &configure, const C &current, const Solution &solution) {
            if (recorder.shouldUpdate(configure, current)) {
                recorder.update(configure, current, solution);
            }
        }

    private:
        Mover<Allow180, AllowSoftdropTap, M, C> mover;
        Recorder<C, R> recorder;
    };

    // Mover implementations
    template<bool Allow180, bool AllowSoftdropTap, class M>
    class Mover<Allow180, AllowSoftdropTap, M, TSpinCandidate> {
    public:
        Mover(
                const core::Factory &factory, M &moveGenerator, core::srs_rotate_end::Reachable<Allow180, AllowSoftdropTap> &reachable
        ) : factory(factory), moveGenerator(moveGenerator), reachable(reachable) {
        }

        void move(
                const Configure &configure,
                const core::Field &field,
                const TSpinCandidate &candidate,
                Solution &solution,
                std::vector<core::Move> &moves,
                std::vector<core::ScoredMove> &scoredMoves,
                core::PieceType pieceType,
                int nextIndex,
                int nextHoldIndex,
                int nextHoldCount,
                PCFindRunner<Allow180, AllowSoftdropTap, M> *finder
        ) {
            assert(0 < candidate.leftLine);

            auto lastDepth = candidate.depth == configure.maxDepth - 1;
            auto nextLeftNumOfT = pieceType == core::PieceType::T ? candidate.leftNumOfT - 1 : candidate.leftNumOfT;

            moveGenerator.search(moves, field, pieceType, candidate.leftLine);

            if (configure.fastSearchStartDepth <= candidate.depth) {
                for (const auto &move : moves) {
                    auto &blocks = factory.get(pieceType, move.rotateType);

                    auto freeze = core::Field(field);
                    freeze.put(blocks, move.x, move.y);

                    int numCleared = freeze.clearLineReturnNum();

                    auto &operation = solution[candidate.depth];
                    operation.pieceType = pieceType;
                    operation.rotateType = move.rotateType;
                    operation.x = move.x;
                    operation.y = move.y;

                    int tSpinAttack = !lastDepth ? getAttackIfTSpin<Allow180, AllowSoftdropTap>(
                            moveGenerator, reachable, factory, field, pieceType, move, numCleared, candidate.b2b
                    ) : 0;

                    int nextSoftdropCount = move.harddrop ? candidate.softdropCount : candidate.softdropCount + 1;
                    int nextLineClearCount = 0 < numCleared ? candidate.lineClearCount + 1 : candidate.lineClearCount;
                    int nextCurrentCombo = 0 < numCleared ? candidate.currentCombo + 1 : 0;
                    int nextMaxCombo = candidate.maxCombo < nextCurrentCombo ? nextCurrentCombo : candidate.maxCombo;
                    int nextTSpinAttack = candidate.tSpinAttack + tSpinAttack;
                    bool nextB2b = 0 < numCleared ? (tSpinAttack != 0 || numCleared == 4) : candidate.b2b;
                    int nextFrames = candidate.frames + getFrames(operation);

                    auto nextDepth = candidate.depth + 1;

                    int nextLeftLine = candidate.leftLine - numCleared;
                    if (nextLeftLine == 0) {
                        auto bestCandidate = TSpinCandidate{
                                nextIndex, nextHoldIndex, nextLeftLine, nextDepth,
                                nextSoftdropCount, nextHoldCount, nextLineClearCount, nextCurrentCombo, nextMaxCombo,
                                nextTSpinAttack, nextB2b, nextLeftNumOfT, nextFrames
                        };
                        finder->accept(configure, bestCandidate, solution);
                        return;
                    }

                    if (configure.maxDepth <= nextDepth) {
                        continue;
                    }

                    if (!validate(freeze, nextLeftLine)) {
                        continue;
                    }

                    auto nextCandidate = TSpinCandidate{
                            nextIndex, nextHoldIndex, nextLeftLine, nextDepth,
                            nextSoftdropCount, nextHoldCount, nextLineClearCount, nextCurrentCombo, nextMaxCombo,
                            nextTSpinAttack, nextB2b, nextLeftNumOfT, nextFrames
                    };
                    finder->search(configure, freeze, nextCandidate, solution);
                }
            } else {
                toScoredMove(moves, factory, pieceType, field, scoredMoves);

                for (const auto &s : scoredMoves) {
                    auto &operation = solution[candidate.depth];
                    operation.pieceType = pieceType;
                    operation.rotateType = s.move.rotateType;
                    operation.x = s.move.x;
                    operation.y = s.move.y;

                    int tSpinAttack = !lastDepth ? getAttackIfTSpin<Allow180, AllowSoftdropTap>(
                            moveGenerator, reachable, factory, field, pieceType, s.move, s.numCleared, candidate.b2b
                    ) : 0;

                    int nextSoftdropCount = s.move.harddrop ? candidate.softdropCount : candidate.softdropCount + 1;
                    int nextLineClearCount = 0 < s.numCleared ? candidate.lineClearCount + 1 : candidate.lineClearCount;
                    int nextCurrentCombo = 0 < s.numCleared ? candidate.currentCombo + 1 : 0;
                    int nextMaxCombo = candidate.maxCombo < nextCurrentCombo ? nextCurrentCombo : candidate.maxCombo;
                    int nextTSpinAttack = candidate.tSpinAttack + tSpinAttack;
                    bool nextB2b = 0 < s.numCleared ? (tSpinAttack != 0 || s.numCleared == 4) : candidate.b2b;
                    int nextFrames = candidate.frames + getFrames(operation);

                    auto nextDepth = candidate.depth + 1;

                    int nextLeftLine = candidate.leftLine - s.numCleared;
                    if (nextLeftLine == 0) {
                        auto bestCandidate = TSpinCandidate{
                                nextIndex, nextHoldIndex, nextLeftLine, nextDepth,
                                nextSoftdropCount, nextHoldCount, nextLineClearCount, nextCurrentCombo, nextMaxCombo,
                                nextTSpinAttack, nextB2b, nextLeftNumOfT, nextFrames
                        };
                        finder->accept(configure, bestCandidate, solution);
                        return;
                    }

                    if (configure.maxDepth <= nextDepth) {
                        continue;
                    }

                    if (!validate(s.field, nextLeftLine)) {
                        continue;
                    }

                    auto nextCandidate = TSpinCandidate{
                            nextIndex, nextHoldIndex, nextLeftLine, nextDepth,
                            nextSoftdropCount, nextHoldCount, nextLineClearCount, nextCurrentCombo, nextMaxCombo,
                            nextTSpinAttack, nextB2b, nextLeftNumOfT, nextFrames
                    };
                    finder->search(configure, s.field, nextCandidate, solution);
                }
            }
        }

        void premove(
                bool alwaysRegularAttack,
                int maxDepth,
                const core::Field &field,
                const TSpinCandidate &candidate,
                std::vector<core::Move> &moves,
                core::PieceType pieceType,
                int nextIndex,
                int nextHoldIndex,
                int nextHoldCount,
                std::vector<PreOperation<TSpinCandidate>> &output
        ) {
            assert(0 < candidate.leftLine);

            auto nextLeftNumOfT = pieceType == core::PieceType::T ? candidate.leftNumOfT - 1 : candidate.leftNumOfT;

            moveGenerator.search(moves, field, pieceType, candidate.leftLine);

            for (const auto &move : moves) {
                auto &blocks = factory.get(pieceType, move.rotateType);

                auto freeze = core::Field(field);
                freeze.put(blocks, move.x, move.y);

                int numCleared = freeze.clearLineReturnNum();

				auto operation = Operation{
					pieceType,
					move.rotateType,
					move.x,
					move.y
				};

                int tSpinAttack = getAttackIfTSpin(
                        moveGenerator, reachable, factory, field, pieceType, move, numCleared, candidate.b2b
                );

                int nextSoftdropCount = move.harddrop ? candidate.softdropCount : candidate.softdropCount + 1;
                int nextLineClearCount = 0 < numCleared ? candidate.lineClearCount + 1 : candidate.lineClearCount;
                int nextCurrentCombo = 0 < numCleared ? candidate.currentCombo + 1 : 0;
                int nextMaxCombo = candidate.maxCombo < nextCurrentCombo ? nextCurrentCombo : candidate.maxCombo;
                int nextTSpinAttack = candidate.tSpinAttack + tSpinAttack;
                bool nextB2b = 0 < numCleared ? (tSpinAttack != 0 || numCleared == 4) : candidate.b2b;
				int nextFrames = candidate.frames + getFrames(operation);

                auto nextDepth = candidate.depth + 1;

                int nextLeftLine = candidate.leftLine - numCleared;

                if (!validate(freeze, nextLeftLine)) {
                    continue;
                }

                int score = calcScore(freeze, move.harddrop);
                auto preoperation = PreOperation<TSpinCandidate>{
                        freeze,
                        {
                                nextIndex, nextHoldIndex, nextLeftLine, nextDepth,
                                nextSoftdropCount, nextHoldCount, nextLineClearCount, nextCurrentCombo, nextMaxCombo,
                                nextTSpinAttack, nextB2b, nextLeftNumOfT, nextFrames
                        },
                        pieceType,
                        move.rotateType,
                        move.x,
                        move.y,
                        move.harddrop,
                        numCleared,
                        score,
                };
                output.push_back(preoperation);
            }
        }

    private:
        const core::Factory &factory;
        M &moveGenerator;
        core::srs_rotate_end::Reachable<Allow180, AllowSoftdropTap> reachable;
    };

    template<bool Allow180, bool AllowSoftdropTap, class M>
    class Mover<Allow180, AllowSoftdropTap, M, FastCandidate> {
    public:
        Mover(
                const core::Factory &factory, M &moveGenerator, core::srs_rotate_end::Reachable<Allow180, AllowSoftdropTap> &reachable
        ) : factory(factory), moveGenerator(moveGenerator), reachable(reachable) {
        }

        void move(
                const Configure &configure,
                const core::Field &field,
                const FastCandidate &candidate,
                Solution &solution,
                std::vector<core::Move> &moves,
                std::vector<core::ScoredMove> &scoredMoves,
                core::PieceType pieceType,
                int nextIndex,
                int nextHoldIndex,
                int nextHoldCount,
                PCFindRunner<Allow180, AllowSoftdropTap, M, FastCandidate, FastRecord> *finder
        ) {
            assert(0 < candidate.leftLine);

            moveGenerator.search(moves, field, pieceType, candidate.leftLine);

            if (configure.fastSearchStartDepth <= candidate.depth) {
                for (const auto &move : moves) {
                    auto &blocks = factory.get(pieceType, move.rotateType);

                    auto freeze = core::Field(field);
                    freeze.put(blocks, move.x, move.y);

                    int numCleared = freeze.clearLineReturnNum();

                    auto &operation = solution[candidate.depth];
                    operation.pieceType = pieceType;
                    operation.rotateType = move.rotateType;
                    operation.x = move.x;
                    operation.y = move.y;

                    int nextSoftdropCount = move.harddrop ? candidate.softdropCount : candidate.softdropCount + 1;
                    int nextLineClearCount = 0 < numCleared ? candidate.lineClearCount + 1 : candidate.lineClearCount;
                    int nextCurrentCombo = 0 < numCleared ? candidate.currentCombo + 1 : 0;
                    int nextMaxCombo = candidate.maxCombo < nextCurrentCombo ? nextCurrentCombo : candidate.maxCombo;
				    int nextFrames = candidate.frames + getFrames(operation);

                    auto nextDepth = candidate.depth + 1;

                    int nextLeftLine = candidate.leftLine - numCleared;
                    if (nextLeftLine == 0) {
                        auto bestCandidate = FastCandidate{
                                nextIndex, nextHoldIndex, nextLeftLine, nextDepth,
                                nextSoftdropCount, nextHoldCount, nextLineClearCount,
                                nextCurrentCombo, nextMaxCombo, nextFrames
                        };
                        finder->accept(configure, bestCandidate, solution);
                        return;
                    }

                    if (configure.maxDepth <= nextDepth) {
                        continue;
                    }

                    if (!validate(freeze, nextLeftLine)) {
                        continue;
                    }

                    auto nextCandidate = FastCandidate{
                            nextIndex, nextHoldIndex, nextLeftLine, nextDepth,
                            nextSoftdropCount, nextHoldCount, nextLineClearCount,
                            nextCurrentCombo, nextMaxCombo, nextFrames
                    };
                    finder->search(configure, freeze, nextCandidate, solution);
                }
            } else {
                toScoredMove(moves, factory, pieceType, field, scoredMoves);

                for (const auto &s : scoredMoves) {
                    auto &operation = solution[candidate.depth];
                    operation.pieceType = pieceType;
                    operation.rotateType = s.move.rotateType;
                    operation.x = s.move.x;
                    operation.y = s.move.y;

                    int nextSoftdropCount = s.move.harddrop ? candidate.softdropCount : candidate.softdropCount + 1;
                    int nextLineClearCount = 0 < s.numCleared ? candidate.lineClearCount + 1 : candidate.lineClearCount;
                    int nextCurrentCombo = 0 < s.numCleared ? candidate.currentCombo + 1 : 0;
                    int nextMaxCombo = candidate.maxCombo < nextCurrentCombo ? nextCurrentCombo : candidate.maxCombo;
                    int nextFrames = candidate.frames + getFrames(operation);

                    auto nextDepth = candidate.depth + 1;

                    int nextLeftLine = candidate.leftLine - s.numCleared;
                    if (nextLeftLine == 0) {
                        auto bestCandidate = FastCandidate{
                                nextIndex, nextHoldIndex, nextLeftLine, nextDepth,
                                nextSoftdropCount, nextHoldCount, nextLineClearCount,
                                nextCurrentCombo, nextMaxCombo, nextFrames
                        };
                        finder->accept(configure, bestCandidate, solution);
                        return;
                    }

                    if (configure.maxDepth <= nextDepth) {
                        continue;
                    }

                    if (!validate(s.field, nextLeftLine)) {
                        continue;
                    }

                    auto nextCandidate = FastCandidate{
                            nextIndex, nextHoldIndex, nextLeftLine, nextDepth,
                            nextSoftdropCount, nextHoldCount, nextLineClearCount,
                                nextCurrentCombo, nextMaxCombo, nextFrames
                    };
                    finder->search(configure, s.field, nextCandidate, solution);
                }
            }
        }

        void premove(
                bool alwaysRegularAttack,
                int maxDepth,
                const core::Field &field,
                const FastCandidate &candidate,
                std::vector<core::Move> &moves,
                core::PieceType pieceType,
                int nextIndex,
                int nextHoldIndex,
                int nextHoldCount,
                std::vector<PreOperation<FastCandidate>> &output
        ) {
            assert(0 < candidate.leftLine);

            moveGenerator.search(moves, field, pieceType, candidate.leftLine);

            for (const auto &move : moves) {
                auto &blocks = factory.get(pieceType, move.rotateType);

                auto freeze = core::Field(field);
                freeze.put(blocks, move.x, move.y);

                int numCleared = freeze.clearLineReturnNum();

				auto operation = Operation{
					pieceType,
					move.rotateType,
					move.x,
					move.y
				};

                int nextSoftdropCount = move.harddrop ? candidate.softdropCount : candidate.softdropCount + 1;
                int nextLineClearCount = 0 < numCleared ? candidate.lineClearCount + 1 : candidate.lineClearCount;
                int nextCurrentCombo = 0 < numCleared ? candidate.currentCombo + 1 : 0;
                int nextMaxCombo = candidate.maxCombo < nextCurrentCombo ? nextCurrentCombo : candidate.maxCombo;
                int nextFrames = candidate.frames + getFrames(operation);

                auto nextDepth = candidate.depth + 1;

                int nextLeftLine = candidate.leftLine - numCleared;

                if (!validate(freeze, nextLeftLine)) {
                    continue;
                }

                int score = calcScore(freeze, move.harddrop);
                auto preoperation = PreOperation<FastCandidate>{
                        freeze,
                        {
                                nextIndex, nextHoldIndex, nextLeftLine, nextDepth,
                                nextSoftdropCount, nextHoldCount, nextLineClearCount,
                                nextCurrentCombo, nextMaxCombo, nextFrames
                        },
                        pieceType,
                        move.rotateType,
                        move.x,
                        move.y,
                        move.harddrop,
                        numCleared,
                        score,
                };
                output.push_back(preoperation);
            }
        }

    private:
        const core::Factory &factory;
        M &moveGenerator;
        core::srs_rotate_end::Reachable<Allow180, AllowSoftdropTap> reachable;
    };

    template<bool Allow180, bool AllowSoftdropTap, class M>
    class Mover<Allow180, AllowSoftdropTap, M, AllSpinsCandidate> {
    public:
        Mover(
                const core::Factory &factory, M &moveGenerator, core::srs_rotate_end::Reachable<Allow180, AllowSoftdropTap> &reachable
        ) : factory(factory), moveGenerator(moveGenerator), reachable(reachable) {
        }

        void move(
                const Configure &configure,
                const core::Field &field,
                const AllSpinsCandidate &candidate,
                Solution &solution,
                std::vector<core::Move> &moves,
                std::vector<core::ScoredMove> &scoredMoves,
                core::PieceType pieceType,
                int nextIndex,
                int nextHoldIndex,
                int nextHoldCount,
                PCFindRunner<Allow180, AllowSoftdropTap, M, AllSpinsCandidate, AllSpinsRecord> *finder
        ) {
            assert(0 < candidate.leftLine);

            auto getAttack = configure.alwaysRegularAttack ? getAttackIfAllSpins<true, Allow180, AllowSoftdropTap> : getAttackIfAllSpins<false, Allow180, AllowSoftdropTap>;

            moveGenerator.search(moves, field, pieceType, candidate.leftLine);

            auto lastDepth = candidate.depth == configure.maxDepth - 1;

            if (configure.fastSearchStartDepth <= candidate.depth) {
                for (const auto &move : moves) {
                    auto &blocks = factory.get(pieceType, move.rotateType);

                    auto freeze = core::Field(field);
                    freeze.put(blocks, move.x, move.y);

                    int numCleared = freeze.clearLineReturnNum();

                    auto &operation = solution[candidate.depth];
                    operation.pieceType = pieceType;
                    operation.rotateType = move.rotateType;
                    operation.x = move.x;
                    operation.y = move.y;

                    int spinAttack = getAttack(
                            moveGenerator, reachable, factory, field, pieceType, move, numCleared, candidate.b2b
                    );

                    // Even if spin with the final piece, the attack is not actually sent (Send only 10 lines by PC; for PPT)
                    // However, B2B will continue, so add 1 line attack
                    if (0 < spinAttack && lastDepth) {
                        spinAttack = 1;
                    }

                    int nextSoftdropCount = move.harddrop ? candidate.softdropCount : candidate.softdropCount + 1;
                    int nextLineClearCount = 0 < numCleared ? candidate.lineClearCount + 1 : candidate.lineClearCount;
                    int nextCurrentCombo = 0 < numCleared ? candidate.currentCombo + 1 : 0;
                    int nextMaxCombo = candidate.maxCombo < nextCurrentCombo ? nextCurrentCombo : candidate.maxCombo;
                    int nextTSpinAttack = candidate.spinAttack + spinAttack;
                    bool nextB2b = 0 < numCleared ? (spinAttack != 0 || numCleared == 4) : candidate.b2b;
                    int nextFrames = candidate.frames + getFrames(operation);

                    auto nextDepth = candidate.depth + 1;

                    int nextLeftLine = candidate.leftLine - numCleared;
                    if (nextLeftLine == 0) {
                        auto bestCandidate = AllSpinsCandidate{
                                nextIndex, nextHoldIndex, nextLeftLine, nextDepth,
                                nextSoftdropCount, nextHoldCount, nextLineClearCount, nextCurrentCombo, nextMaxCombo,
                                nextTSpinAttack, nextB2b, nextFrames
                        };
                        finder->accept(configure, bestCandidate, solution);
                        return;
                    }

                    if (configure.maxDepth <= nextDepth) {
                        continue;
                    }

                    if (!validate(freeze, nextLeftLine)) {
                        continue;
                    }

                    auto nextCandidate = AllSpinsCandidate{
                            nextIndex, nextHoldIndex, nextLeftLine, nextDepth,
                            nextSoftdropCount, nextHoldCount, nextLineClearCount, nextCurrentCombo, nextMaxCombo,
                            nextTSpinAttack, nextB2b, nextFrames
                    };
                    finder->search(configure, freeze, nextCandidate, solution);
                }
            } else {
                toScoredMove(moves, factory, pieceType, field, scoredMoves);

                for (const auto &s : scoredMoves) {
                    auto &operation = solution[candidate.depth];
                    operation.pieceType = pieceType;
                    operation.rotateType = s.move.rotateType;
                    operation.x = s.move.x;
                    operation.y = s.move.y;

                    int spinAttack = getAttack(
                            moveGenerator, reachable, factory, field, pieceType, s.move, s.numCleared, candidate.b2b
                    );

                    // Even if spin with the final piece, the attack is not actually sent (Send only 10 lines by PC; for PPT)
                    // However, B2B will continue, so add 1 line attack
                    if (0 < spinAttack && lastDepth) {
                        spinAttack = 1;
                    }

                    int nextSoftdropCount = s.move.harddrop ? candidate.softdropCount : candidate.softdropCount + 1;
                    int nextLineClearCount = 0 < s.numCleared ? candidate.lineClearCount + 1 : candidate.lineClearCount;
                    int nextCurrentCombo = 0 < s.numCleared ? candidate.currentCombo + 1 : 0;
                    int nextMaxCombo = candidate.maxCombo < nextCurrentCombo ? nextCurrentCombo : candidate.maxCombo;
                    int nextTSpinAttack = candidate.spinAttack + spinAttack;
                    bool nextB2b = 0 < s.numCleared ? (spinAttack != 0 || s.numCleared == 4) : candidate.b2b;
                    int nextFrames = candidate.frames + getFrames(operation);

                    auto nextDepth = candidate.depth + 1;

                    int nextLeftLine = candidate.leftLine - s.numCleared;
                    if (nextLeftLine == 0) {
                        auto bestCandidate = AllSpinsCandidate{
                                nextIndex, nextHoldIndex, nextLeftLine, nextDepth,
                                nextSoftdropCount, nextHoldCount, nextLineClearCount, nextCurrentCombo, nextMaxCombo,
                                nextTSpinAttack, nextB2b, nextFrames
                        };
                        finder->accept(configure, bestCandidate, solution);
                        return;
                    }

                    if (configure.maxDepth <= nextDepth) {
                        continue;
                    }

                    if (!validate(s.field, nextLeftLine)) {
                        continue;
                    }

                    auto nextCandidate = AllSpinsCandidate{
                            nextIndex, nextHoldIndex, nextLeftLine, nextDepth,
                            nextSoftdropCount, nextHoldCount, nextLineClearCount, nextCurrentCombo, nextMaxCombo,
                            nextTSpinAttack, nextB2b, nextFrames
                    };
                    finder->search(configure, s.field, nextCandidate, solution);
                }
            }
        }

        void premove(
                bool alwaysRegularAttack,
                int maxDepth,
                const core::Field &field,
                const AllSpinsCandidate &candidate,
                std::vector<core::Move> &moves,
                core::PieceType pieceType,
                int nextIndex,
                int nextHoldIndex,
                int nextHoldCount,
                std::vector<PreOperation<AllSpinsCandidate>> &output
        ) {
            assert(0 < candidate.leftLine);

            auto getAttack = alwaysRegularAttack ? getAttackIfAllSpins<true, Allow180, AllowSoftdropTap> : getAttackIfAllSpins<false, Allow180, AllowSoftdropTap>;

            moveGenerator.search(moves, field, pieceType, candidate.leftLine);

            auto lastDepth = candidate.depth == maxDepth - 1;

            for (const auto &move : moves) {
                auto &blocks = factory.get(pieceType, move.rotateType);

                auto freeze = core::Field(field);
                freeze.put(blocks, move.x, move.y);

                int numCleared = freeze.clearLineReturnNum();

				auto operation = Operation{
					pieceType,
					move.rotateType,
					move.x,
					move.y
				};

                int spinAttack = getAttack(
                        moveGenerator, reachable, factory, field, pieceType, move, numCleared, candidate.b2b
                );

                // Even if spin with the final piece, the attack is not actually sent (Send only 10 lines by PC; for PPT)
                // However, B2B will continue, so add 1 line attack
                if (0 < spinAttack && lastDepth) {
                    spinAttack = 1;
                }

                int nextSoftdropCount = move.harddrop ? candidate.softdropCount : candidate.softdropCount + 1;
                int nextLineClearCount = 0 < numCleared ? candidate.lineClearCount + 1 : candidate.lineClearCount;
                int nextCurrentCombo = 0 < numCleared ? candidate.currentCombo + 1 : 0;
                int nextMaxCombo = candidate.maxCombo < nextCurrentCombo ? nextCurrentCombo : candidate.maxCombo;
                int nextTSpinAttack = candidate.spinAttack + spinAttack;
                bool nextB2b = 0 < numCleared ? (spinAttack != 0 || numCleared == 4) : candidate.b2b;
				int nextFrames = candidate.frames + getFrames(operation);

                auto nextDepth = candidate.depth + 1;

                int nextLeftLine = candidate.leftLine - numCleared;

                if (!validate(freeze, nextLeftLine)) {
                    continue;
                }

                int score = calcScore(freeze, move.harddrop);
                auto preoperation = PreOperation<AllSpinsCandidate>{
                        freeze,
                        {
                                nextIndex, nextHoldIndex, nextLeftLine, nextDepth,
                                nextSoftdropCount, nextHoldCount, nextLineClearCount, nextCurrentCombo, nextMaxCombo,
                                nextTSpinAttack, nextB2b, nextFrames
                        },
                        pieceType,
                        move.rotateType,
                        move.x,
                        move.y,
                        move.harddrop,
                        numCleared,
                        score,
                };
                output.push_back(preoperation);
            }
        }

    private:
        const core::Factory &factory;
        M &moveGenerator;
        core::srs_rotate_end::Reachable<Allow180, AllowSoftdropTap> reachable;
    };
    
    template<bool Allow180, bool AllowSoftdropTap, class M>
    class Mover<Allow180, AllowSoftdropTap, M, TETRIOS2Candidate> {
    public:
        Mover(
                const core::Factory &factory, M &moveGenerator, core::srs_rotate_end::Reachable<Allow180, AllowSoftdropTap> &reachable
        ) : factory(factory), moveGenerator(moveGenerator), reachable(reachable) {
        }

        void move(
                const Configure &configure,
                const core::Field &field,
                const TETRIOS2Candidate &candidate,
                Solution &solution,
                std::vector<core::Move> &moves,
                std::vector<core::ScoredMove> &scoredMoves,
                core::PieceType pieceType,
                int nextIndex,
                int nextHoldIndex,
                int nextHoldCount,
                PCFindRunner<Allow180, AllowSoftdropTap, M, TETRIOS2Candidate, TETRIOS2Record> *finder
        ) {
            assert(0 < candidate.leftLine);

            auto getAttack = configure.alwaysRegularAttack ? getAttackIfAllSpins<true, Allow180, AllowSoftdropTap> : getAttackIfAllSpins<false, Allow180, AllowSoftdropTap>;

            moveGenerator.search(moves, field, pieceType, candidate.leftLine);

            auto lastDepth = candidate.depth == configure.maxDepth - 1;

            if (configure.fastSearchStartDepth <= candidate.depth) {
                for (const auto &move : moves) {
                    auto &blocks = factory.get(pieceType, move.rotateType);

                    auto freeze = core::Field(field);
                    freeze.put(blocks, move.x, move.y);

                    int numCleared = freeze.clearLineReturnNum();

                    auto &operation = solution[candidate.depth];
                    operation.pieceType = pieceType;
                    operation.rotateType = move.rotateType;
                    operation.x = move.x;
                    operation.y = move.y;

                    // First check if it's a full T-spin
                    int spinAttack = getAttackIfTSpin(
                        moveGenerator, reachable, factory, field, pieceType, move, numCleared, candidate.b2b
                    );

                    // TMinis are ignored this way
                    bool isTSpin = spinAttack >= 2;

                    // Check All-Spins otherwise
                    if (!isTSpin) {
                        spinAttack = getAttack(
                            moveGenerator, reachable, factory, field, pieceType, move, numCleared, candidate.b2b
                        );
                    }

                    // Count Tetrises as spins
					if (numCleared == 4) {
						spinAttack = 4 + (candidate.b2b > 0 ? 1 : 0);
					}

                    // Treat as spin if it's the last clear for the PC.
                    // B2B still charges and the attack sent out is the same as if there is a spin
                    bool isSpin = numCleared == candidate.leftLine || spinAttack > 0;

                    //! Clears must be spins
					if (numCleared > 0 && !isSpin) {
                        continue;
					}

                    // For two-line PC, disallow taking the double
                    if (candidate.leftLine == 2 && numCleared == 2 && candidate.lineClearCount == 0) {
                        continue;
                    }

                    // Can the PC still be downstacked while keeping B2B even if garbage was tanked?
                    bool nextIsClean = numCleared == candidate.leftLine && spinAttack > 0;
                    bool nextIsFlatI = numCleared == candidate.leftLine && numCleared == 1;

                    // Correct damage values for non-T-Spins
                    if (isSpin && !isTSpin) {
                        const int attackValues[] = { 0, 0, 1, 2, 4 };
                        spinAttack = attackValues[numCleared] + (candidate.b2b > 0 ? 1 : 0);
                    }

                    int nextSoftdropCount = move.harddrop ? candidate.softdropCount : candidate.softdropCount + 1;
                    int nextLineClearCount = 0 < numCleared ? candidate.lineClearCount + 1 : candidate.lineClearCount;
                    int nextCurrentCombo = 0 < numCleared ? candidate.currentCombo + 1 : 0;
                    int nextMaxCombo = candidate.maxCombo < nextCurrentCombo ? nextCurrentCombo : candidate.maxCombo;
                    int nextSpinAttack = candidate.spinAttack + spinAttack;
                    int nextB2b = 0 < numCleared ? candidate.b2b + 1 : candidate.b2b;
                    int nextFrames = candidate.frames + getFrames(operation);

                    auto nextDepth = candidate.depth + 1;

                    int nextLeftLine = candidate.leftLine - numCleared;
                    if (nextLeftLine == 0) {
                        auto bestCandidate = TETRIOS2Candidate{
                                nextIndex, nextHoldIndex, nextLeftLine, nextDepth,
                                nextSoftdropCount, nextHoldCount, nextLineClearCount, nextCurrentCombo, nextMaxCombo,
                                nextSpinAttack, nextB2b, nextFrames, nextIsClean, nextIsFlatI
                        };
                        finder->accept(configure, bestCandidate, solution);
                        return;
                    }

                    if (configure.maxDepth <= nextDepth) {
                        continue;
                    }

                    if (!validate(freeze, nextLeftLine)) {
                        continue;
                    }

                    auto nextCandidate = TETRIOS2Candidate{
                            nextIndex, nextHoldIndex, nextLeftLine, nextDepth,
                            nextSoftdropCount, nextHoldCount, nextLineClearCount, nextCurrentCombo, nextMaxCombo,
                            nextSpinAttack, nextB2b, nextFrames, nextIsClean, nextIsFlatI
                    };
                    finder->search(configure, freeze, nextCandidate, solution);
                }
            } else {
                toScoredMove(moves, factory, pieceType, field, scoredMoves);

                for (const auto &s : scoredMoves) {
                    auto &operation = solution[candidate.depth];
                    operation.pieceType = pieceType;
                    operation.rotateType = s.move.rotateType;
                    operation.x = s.move.x;
                    operation.y = s.move.y;

                    // First check if it's a full T-spin
                    int spinAttack = getAttackIfTSpin(
                        moveGenerator, reachable, factory, field, pieceType, s.move, s.numCleared, candidate.b2b
                    );

                    // TMinis are ignored this way
                    bool isTSpin = spinAttack >= 2;

                    // Check All-Spins otherwise
                    if (!isTSpin) {
                        spinAttack = getAttack(
                            moveGenerator, reachable, factory, field, pieceType, s.move, s.numCleared, candidate.b2b
                        );
                    }

                    // Count Tetrises as spins
                    if (s.numCleared == 4) {
                        spinAttack = 4 + (candidate.b2b > 0 ? 1 : 0);
                    }

                    // Treat as spin if it's the last clear for the PC.
                    // B2B still charges and the attack sent out is the same as if there is a spin
                    bool isSpin = s.numCleared == candidate.leftLine || spinAttack > 0;

                    //! Clears must be spins
                    if (s.numCleared > 0 && !isSpin) {
                        continue;
                    }

                    // For two-line PC, disallow taking the double
                    if (candidate.leftLine == 2 && s.numCleared == 2 && candidate.lineClearCount == 0) {
                        continue;
                    }

                    // Can the PC still be downstacked while keeping B2B even if garbage was tanked?
                    bool nextIsClean = s.numCleared == candidate.leftLine && spinAttack > 0;
                    bool nextIsFlatI = s.numCleared == candidate.leftLine && s.numCleared == 1;

                    // Correct damage values for non-T-Spins
                    if (isSpin && !isTSpin) {
                        const int attackValues[] = { 0, 0, 1, 2, 4 };
                        spinAttack = attackValues[s.numCleared] + (candidate.b2b > 0 ? 1 : 0);
                    }

                    int nextSoftdropCount = s.move.harddrop ? candidate.softdropCount : candidate.softdropCount + 1;
                    int nextLineClearCount = 0 < s.numCleared ? candidate.lineClearCount + 1 : candidate.lineClearCount;
                    int nextCurrentCombo = 0 < s.numCleared ? candidate.currentCombo + 1 : 0;
                    int nextMaxCombo = candidate.maxCombo < nextCurrentCombo ? nextCurrentCombo : candidate.maxCombo;
                    int nextSpinAttack = candidate.spinAttack + spinAttack;
                    int nextB2b = 0 < s.numCleared ? candidate.b2b + 1 : candidate.b2b;
                    int nextFrames = candidate.frames + getFrames(operation);

                    auto nextDepth = candidate.depth + 1;

                    int nextLeftLine = candidate.leftLine - s.numCleared;
                    if (nextLeftLine == 0) {
                        auto bestCandidate = TETRIOS2Candidate{
                                nextIndex, nextHoldIndex, nextLeftLine, nextDepth,
                                nextSoftdropCount, nextHoldCount, nextLineClearCount, nextCurrentCombo, nextMaxCombo,
                                nextSpinAttack, nextB2b, nextFrames, nextIsClean, nextIsFlatI
                        };
                        finder->accept(configure, bestCandidate, solution);
                        return;
                    }

                    if (configure.maxDepth <= nextDepth) {
                        continue;
                    }

                    if (!validate(s.field, nextLeftLine)) {
                        continue;
                    }

                    auto nextCandidate = TETRIOS2Candidate{
                            nextIndex, nextHoldIndex, nextLeftLine, nextDepth,
                            nextSoftdropCount, nextHoldCount, nextLineClearCount, nextCurrentCombo, nextMaxCombo,
                            nextSpinAttack, nextB2b, nextFrames, nextIsClean, nextIsFlatI
                    };
                    finder->search(configure, s.field, nextCandidate, solution);
                }
            }
        }

        void premove(
                bool alwaysRegularAttack,
                int maxDepth,
                const core::Field &field,
                const TETRIOS2Candidate &candidate,
                std::vector<core::Move> &moves,
                core::PieceType pieceType,
                int nextIndex,
                int nextHoldIndex,
                int nextHoldCount,
                std::vector<PreOperation<TETRIOS2Candidate>> &output
        ) {
            assert(0 < candidate.leftLine);

            auto getAttack = alwaysRegularAttack ? getAttackIfAllSpins<true, Allow180, AllowSoftdropTap> : getAttackIfAllSpins<false, Allow180, AllowSoftdropTap>;

            moveGenerator.search(moves, field, pieceType, candidate.leftLine);

            auto lastDepth = candidate.depth == maxDepth - 1;

            for (const auto &move : moves) {
                auto &blocks = factory.get(pieceType, move.rotateType);

                auto freeze = core::Field(field);
                freeze.put(blocks, move.x, move.y);

                int numCleared = freeze.clearLineReturnNum();

				auto operation = Operation{
					pieceType,
					move.rotateType,
					move.x,
					move.y
				};

                // First check if it's a full T-spin
                int spinAttack = getAttackIfTSpin(
                    moveGenerator, reachable, factory, field, pieceType, move, numCleared, candidate.b2b
                );

                // TMinis are ignored this way
                bool isTSpin = spinAttack >= 2;

                // Check All-Spins otherwise
                if (!isTSpin) {
                    spinAttack = getAttack(
                        moveGenerator, reachable, factory, field, pieceType, move, numCleared, candidate.b2b
                    );
                }

                // Count Tetrises as spins
                if (numCleared == 4) {
                    spinAttack = 4 + (candidate.b2b > 0 ? 1 : 0);
                }

                // Treat as spin if it's the last clear for the PC.
                // B2B still charges and the attack sent out is the same as if there is a spin
                bool isSpin = numCleared == candidate.leftLine || spinAttack > 0;

                //! Clears must be spins
                if (numCleared > 0 && !isSpin) {
                    continue;
                }

                // For two-line PC, disallow taking the double
                if (candidate.leftLine == 2 && numCleared == 2 && candidate.lineClearCount == 0) {
                    continue;
                }

                // Can the PC still be downstacked while keeping B2B even if garbage was tanked?
                bool nextIsClean = numCleared == candidate.leftLine && spinAttack > 0;
                bool nextIsFlatI = numCleared == candidate.leftLine && numCleared == 1;

                // Correct damage values for non-T-Spins
                if (isSpin && !isTSpin) {
                    const int attackValues[] = { 0, 0, 1, 2, 4 };
                    spinAttack = attackValues[numCleared] + (candidate.b2b > 0 ? 1 : 0);
                }

                int nextSoftdropCount = move.harddrop ? candidate.softdropCount : candidate.softdropCount + 1;
                int nextLineClearCount = 0 < numCleared ? candidate.lineClearCount + 1 : candidate.lineClearCount;
                int nextCurrentCombo = 0 < numCleared ? candidate.currentCombo + 1 : 0;
                int nextMaxCombo = candidate.maxCombo < nextCurrentCombo ? nextCurrentCombo : candidate.maxCombo;
                int nextSpinAttack = candidate.spinAttack + spinAttack;
                int nextB2b = 0 < numCleared ? candidate.b2b + 1 : candidate.b2b;
                int nextFrames = candidate.frames + getFrames(operation);

                auto nextDepth = candidate.depth + 1;

                int nextLeftLine = candidate.leftLine - numCleared;

                if (!validate(freeze, nextLeftLine)) {
                    continue;
                }

                int score = calcScore(freeze, move.harddrop);
                auto preoperation = PreOperation<TETRIOS2Candidate>{
                        freeze,
                        {
                                nextIndex, nextHoldIndex, nextLeftLine, nextDepth,
                                nextSoftdropCount, nextHoldCount, nextLineClearCount, nextCurrentCombo, nextMaxCombo,
                                nextSpinAttack, nextB2b, nextFrames, nextIsClean, nextIsFlatI
                        },
                        pieceType,
                        move.rotateType,
                        move.x,
                        move.y,
                        move.harddrop,
                        numCleared,
                        score,
                };
                output.push_back(preoperation);
            }
        }

    private:
        const core::Factory &factory;
        M &moveGenerator;
        core::srs_rotate_end::Reachable<Allow180, AllowSoftdropTap> reachable;
    };

    // Recorder defines
    template<>
    class Recorder<TSpinCandidate, TSpinRecord> {
    public:
        void clear();

        void update(
                const Configure &configure, const TSpinCandidate &current, const Solution &solution
        );

        void update(const TSpinRecord &record);

        [[nodiscard]] bool isWorseThanBest(bool leastLineClears, const TSpinCandidate &current) const;

        [[nodiscard]] bool shouldUpdate(const Configure &configure, const TSpinCandidate &newRecord) const;

        [[nodiscard]] const TSpinRecord &best() const {
            return best_;
        }

    private:
        TSpinRecord best_;
    };

    template<>
    class Recorder<FastCandidate, FastRecord> {
    public:
        void clear();

        void update(
                const Configure &configure, const FastCandidate &current, const Solution &solution
        );

        void update(const FastRecord &record);

        [[nodiscard]] bool isWorseThanBest(bool leastLineClears, const FastCandidate &current) const;

        [[nodiscard]] bool shouldUpdate(const Configure &configure, const FastCandidate &newRecord) const;

        [[nodiscard]] const FastRecord &best() const {
            return best_;
        }

    private:
        FastRecord best_;
    };

    template<>
    class Recorder<AllSpinsCandidate, AllSpinsRecord> {
    public:
        void clear();

        void update(
                const Configure &configure, const AllSpinsCandidate &current, const Solution &solution
        );

        void update(const AllSpinsRecord &record);

        [[nodiscard]] bool isWorseThanBest(bool leastLineClears, const AllSpinsCandidate &current) const;

        [[nodiscard]] bool shouldUpdate(const Configure &configure, const AllSpinsCandidate &newRecord) const;

        [[nodiscard]] const AllSpinsRecord &best() const {
            return best_;
        }

    private:
        AllSpinsRecord best_;
    };

    template<>
    class Recorder<TETRIOS2Candidate, TETRIOS2Record> {
    public:
        void clear();

        void update(
            const Configure& configure, const TETRIOS2Candidate& current, const Solution& solution
        );

        void update(const TETRIOS2Record& record);

        [[nodiscard]] bool isWorseThanBest(bool leastLineClears, const TETRIOS2Candidate& current) const;

        [[nodiscard]] bool shouldUpdate(const Configure& configure, const TETRIOS2Candidate& newRecord) const;

        [[nodiscard]] const TETRIOS2Record& best() const {
            return best_;
        }

    private:
        TETRIOS2Record best_;
    };

    // Entry point to find best perfect clear
    template<bool Allow180 = false, bool AllowSoftdropTap = true, class M = core::srs::MoveGenerator<Allow180, AllowSoftdropTap>>
    class PerfectClearFinder {
    public:
        PerfectClearFinder(const core::Factory &factory, M &moveGenerator)
                : factory(factory), moveGenerator(moveGenerator), reachable(core::srs_rotate_end::Reachable<Allow180, AllowSoftdropTap>(factory)) {
        }

        // If `alwaysRegularAttack` is true, mini spin is judged as regular attack
        Solution run(
                const core::Field &field, const std::vector<core::PieceType> &pieces,
                int maxDepth, int maxLine, bool holdEmpty, bool holdAllowed, bool leastLineClears, 
                SearchTypes searchTypes, int initCombo, bool initB2b, bool alwaysRegularAttack, uint8_t lastHoldPriority, int fastSearchStartDepth
        ) {
            assert(1 <= maxDepth);

            // Copy field
            auto freeze = core::Field(field);

            // Initialize moves
            std::vector<std::vector<core::Move>> movePool(maxDepth);
            for (int index = 0; index < maxDepth; ++index) {
                movePool[index] = std::vector<core::Move>{};
            }

            std::vector<std::vector<core::ScoredMove>> scoredMovePool(maxDepth);
            for (int index = 0; index < maxDepth; ++index) {
                scoredMovePool[index] = std::vector<core::ScoredMove>{};
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

            switch (searchTypes) {
                case SearchTypes::Fast: {
                    // Create candidate
                    auto candidate = holdEmpty
                                     ? FastCandidate{0, -1, maxLine, 0, 0, 0, 0,
                                                     initCombo, initCombo, 0}
                                     : FastCandidate{1, 0, maxLine, 0, 0, 0, 0,
                                                     initCombo, initCombo, 0};

                    auto finder = PCFindRunner<Allow180, AllowSoftdropTap, M, FastCandidate, FastRecord>(
                            factory, moveGenerator, reachable
                    );
                    return finder.run(configure, freeze, candidate);
                }
                case SearchTypes::TSpin: {
                    assert(!alwaysRegularAttack);  // Support no mini only

                    // Count up T
                    int leftNumOfT = std::count(pieces.begin(), pieces.end(), core::PieceType::T);

                    // Create candidate
                    auto candidate = holdEmpty
                                     ? TSpinCandidate{0, -1, maxLine, 0, 0, 0, 0,
                                                      initCombo, initCombo, 0, initB2b, leftNumOfT, 0}
                                     : TSpinCandidate{1, 0, maxLine, 0, 0, 0, 0,
                                                      initCombo, initCombo, 0, initB2b, leftNumOfT, 0};

                    auto finder = PCFindRunner<Allow180, AllowSoftdropTap, M>(
                            factory, moveGenerator, reachable
                    );
                    return finder.run(configure, freeze, candidate);
                }
                case SearchTypes::AllSpins: {
                    // Create candidate
                    auto candidate = holdEmpty
                                     ? AllSpinsCandidate{0, -1, maxLine, 0, 0, 0, 0,
                                                         initCombo, initCombo, 0, initB2b, 0}
                                     : AllSpinsCandidate{1, 0, maxLine, 0, 0, 0, 0,
                                                         initCombo, initCombo, 0, initB2b, 0};

                    auto finder = PCFindRunner<Allow180, AllowSoftdropTap, M, AllSpinsCandidate, AllSpinsRecord>(
                            factory, moveGenerator, reachable
                    );
                    return finder.run(configure, freeze, candidate);
                }
                case SearchTypes::TETRIOS2: {
                    // Create candidate
                    auto candidate = holdEmpty
                                     ? TETRIOS2Candidate{0, -1, maxLine, 0, 0, 0, 0, initCombo,
                                                         initCombo, 0, initB2b? 1 : 0, 0, false, false}
                                     : TETRIOS2Candidate{1, 0, maxLine, 0, 0, 0, 0,  initCombo,
                                                         initCombo, 0, initB2b? 1 : 0, 0, false, false};

                    auto finder = PCFindRunner<Allow180, AllowSoftdropTap, M, TETRIOS2Candidate, TETRIOS2Record>(
                            factory, moveGenerator, reachable
                    );
                    return finder.run(configure, freeze, candidate);
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
                case 4: {
                    // TETR.IO Season 2:
                    // All-Spins is top priority (all spins are judged as regular attack)
                    // Non-Spin skims are bad
                    // Two-line PCs are only good if there is a Spin-Single followed by a Single
                    return run(
                        field, pieces, maxDepth, maxLine, holdEmpty, holdAllowed, leastLineClears, SearchTypes::TETRIOS2, initCombo, initB2b,
                        true, lastHoldPriority, fastSearchStartDepth
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
            status.abort();
        }

    private:
        const core::Factory &factory;
        M &moveGenerator;
        core::srs_rotate_end::Reachable<Allow180, AllowSoftdropTap> reachable;
    };
}

#endif //FINDER_PERFECT_HPP
