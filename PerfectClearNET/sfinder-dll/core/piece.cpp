#include <cassert>

#include "piece.hpp"

namespace core {
    namespace {
        const uint64_t VALID_BOARD_RANGE = 0xfffffffffffffffL;

        constexpr std::array<Transform, 4> tTransforms{
            Transform{Offset{0, 0}, RotateType::Spawn},
            Transform{Offset{0, 0}, RotateType::Right},
            Transform{Offset{0, 0}, RotateType::Reverse},
            Transform{Offset{0, 0}, RotateType::Left},
        };
        constexpr std::array<Transform, 4> iTransforms{
            Transform{Offset{0, 0}, RotateType::Spawn},
            Transform{Offset{0, -1}, RotateType::Left},
            Transform{Offset{-1, 0}, RotateType::Spawn},
            Transform{Offset{0, 0}, RotateType::Left},
        };
        constexpr std::array<Transform, 4> sTransforms{
            Transform{Offset{0, 0}, RotateType::Spawn},
            Transform{Offset{1, 0}, RotateType::Left},
            Transform{Offset{0, -1}, RotateType::Spawn},
            Transform{Offset{0, 0}, RotateType::Left},
        };
        constexpr std::array<Transform, 4> zTransforms{
            Transform{Offset{0, 0}, RotateType::Spawn},
            Transform{Offset{0, 0}, RotateType::Right},
            Transform{Offset{0, -1}, RotateType::Spawn},
            Transform{Offset{-1, 0}, RotateType::Right},
        };
        constexpr std::array<Transform, 4> oTransforms{
            Transform{Offset{0, 0}, RotateType::Spawn},
            Transform{Offset{0, -1}, RotateType::Spawn},
            Transform{Offset{-1, -1}, RotateType::Spawn},
            Transform{Offset{-1, 0}, RotateType::Spawn},
        };

        std::array<Point, 4> rotateRight_(std::array<Point, 4> points) {
            return std::array<Point, 4>{
                    Point{points[0].y, -points[0].x},
                    Point{points[1].y, -points[1].x},
                    Point{points[2].y, -points[2].x},
                    Point{points[3].y, -points[3].x},
            };
        }

        std::array<Point, 4> rotateLeft_(std::array<Point, 4> points) {
            return std::array<Point, 4>{
                    Point{-points[0].y, points[0].x},
                    Point{-points[1].y, points[1].x},
                    Point{-points[2].y, points[2].x},
                    Point{-points[3].y, points[3].x},
            };
        }

        std::array<Point, 4> rotateReverse_(std::array<Point, 4> points) {
            return std::array<Point, 4>{
                    Point{-points[0].x, -points[0].y},
                    Point{-points[1].x, -points[1].y},
                    Point{-points[2].x, -points[2].y},
                    Point{-points[3].x, -points[3].y},
            };
        }

        uint64_t getXMask(int x, int y) {
            assert(0 <= x && x < FIELD_WIDTH);
            assert(0 <= y && y < MAX_FIELD_HEIGHT);

            return 1LLU << (x + y * FIELD_WIDTH);
        }

        Collider mergeCollider(const Collider &prev, const Bitboard mask, int height, int lowerY) {
            auto collider = Collider{prev};
            assert(0 <= lowerY && lowerY + height <= MAX_FIELD_HEIGHT);

            int index = lowerY / 6;
            int localY = lowerY - 6 * index;
            if (6 < localY + height) {
                // Over
                collider.boards[index] |= (mask << (localY * FIELD_WIDTH)) & VALID_BOARD_RANGE;
                collider.boards[index + 1] |= mask >> ((6 - localY) * FIELD_WIDTH);
            } else {
                // Fit in the lower 6
                collider.boards[index] |= mask << (localY * FIELD_WIDTH);
            }

            return collider;
        }
    }

    Blocks Blocks::create(const RotateType rotateType, const std::array<Point, 4> &points) {
        MinMax minmaxX = std::minmax({points[0].x, points[1].x, points[2].x, points[3].x});
        MinMax minmaxY = std::minmax({points[0].y, points[1].y, points[2].y, points[3].y});

        // Left align
        Bitboard mask = 0;
        for (const auto &point : points) {
            mask |= getXMask(point.x - minmaxX.first, point.y - minmaxY.first);
        }

        // Create colliders for harddrop
        std::array<Collider, MAX_FIELD_HEIGHT> harddropColliders{};
        int height = minmaxY.second - minmaxY.first + 1;
        int max = MAX_FIELD_HEIGHT - height;
        harddropColliders[max] = mergeCollider(Collider{}, mask, height, max);
        for (int index = max - 1; 0 <= index; --index) {
            harddropColliders[index] = mergeCollider(harddropColliders[index + 1], mask, height, index);
        }

        return Blocks(rotateType, points, mask, harddropColliders, minmaxX, minmaxY);
    }

    template<size_t OffsetSizeRotate90>
    Piece Piece::create(
            const PieceType pieceType,
            const std::string &name,
            const std::array<Point, 4> &points,
            const std::array<std::array<Offset, OffsetSizeRotate90>, 4> &offsets,
            const std::array<Transform, 4> &transforms
    ) {
        return create<OffsetSizeRotate90, 0>(pieceType, name, points,offsets, {}, transforms);
    }

    template<size_t OffsetSizeRotate90, size_t OffsetSizeRotate180>
    Piece Piece::create(
        PieceType pieceType,
        const std::string &name,
        const std::array<Point, 4> &points,
        const std::array<std::array<Offset, OffsetSizeRotate90>, 4> &offsets,
        const std::array<Offset, 24> &rotate180Offsets,
        const std::array<Transform, 4> &transforms
    ) {
        std::array<Offset, 20> rightOffsets{};
        for (int rotate = 0; rotate < 4; ++rotate) {
            const auto &from = offsets[rotate];
            const auto &to = offsets[(rotate + 1) % 4];

            assert(from.size() == to.size());

            auto size = from.size();
            for (int index = 0; index < 5; ++index) {
                if (index < size) {
                    rightOffsets[rotate * 5 + index] = {from[index].x - to[index].x, from[index].y - to[index].y};
                } else {
                    rightOffsets[rotate * 5 + index] = {0, 0};
                }
            }
        }

        std::array<Offset, 20> leftOffsets{};
        for (int rotate = 0; rotate < 4; ++rotate) {
            const auto &from = offsets[rotate];
            const auto &to = offsets[(rotate + 3) % 4];

            assert(from.size() == to.size());

            auto size = from.size();
            for (int index = 0; index < 5; ++index) {
                if (index < size) {
                    leftOffsets[rotate * 5 + index] = {from[index].x - to[index].x, from[index].y - to[index].y};
                } else {
                    leftOffsets[rotate * 5 + index] = {0, 0};
                }
            }
        }

        return create<OffsetSizeRotate90, OffsetSizeRotate180>(
            pieceType, name, points, rightOffsets, leftOffsets, rotate180Offsets, transforms
        );
    }

    template <size_t OffsetSizeRotate90, size_t OffsetSizeRotate180>
    Piece Piece::create(
        const PieceType pieceType,
        const std::string &name,
        const std::array<Point, 4> &points,
        const std::array<Offset, 20> &cwOffsets,
        const std::array<Offset, 20> &ccwOffsets,
        const std::array<Offset, 24> &rotate180Offsets,
        const std::array<Transform, 4> &transforms
    ) {
        const Blocks &spawn = Blocks::create(RotateType::Spawn, points);
        const Blocks &right = Blocks::create(RotateType::Right, rotateRight_(points));
        const Blocks &reverse = Blocks::create(RotateType::Reverse, rotateReverse_(points));
        const Blocks &left = Blocks::create(RotateType::Left, rotateLeft_(points));

        int32_t uniqueRotate = 0;
        for (int rotate = 0; rotate < 4; ++rotate) {
            const auto &transform = transforms[rotate];
            uniqueRotate |= 1 << transform.toRotate;
        }

        // Find same shape rotate
        std::array<int32_t, 4> sameShapeRotates{};
        for (int rotate = 0; rotate < 4; ++rotate) {
            int32_t sameRotate = 0;
            for (int target = 0; target < 4; ++target) {
                if (rotate == transforms[target].toRotate) {
                    sameRotate |= 1 << target;
                }
            }
            sameShapeRotates[rotate] = sameRotate;
        }

        // Update all rotates that have the same shape
        for (int rotate = 0; rotate < 4; ++rotate) {
            RotateType afterRotate = transforms[rotate].toRotate;
            if (rotate != afterRotate) {
                sameShapeRotates[rotate] = sameShapeRotates[afterRotate];
            }
        }

        return Piece(pieceType, name, std::array<Blocks, 4>{
            spawn, right, reverse, left
        }, cwOffsets, ccwOffsets, rotate180Offsets, OffsetSizeRotate90, OffsetSizeRotate180, transforms, uniqueRotate, sameShapeRotates);
    }

    BlocksMask Blocks::mask(int leftX, int lowerY) const {
        assert(0 <= leftX && leftX <= FIELD_WIDTH - width);
        assert(0 <= lowerY && lowerY < 6);

        if (6 < lowerY + height) {
            // Over
            const auto slide = mask_ << leftX;
            return {
                    (slide << (lowerY * FIELD_WIDTH)) & VALID_BOARD_RANGE, slide >> ((6 - lowerY) * FIELD_WIDTH)
            };
        } else {
            // Fit in the lower 6
            return {
                    mask_ << (lowerY * FIELD_WIDTH + leftX), 0
            };
        }
    }

    Collider Blocks::harddrop(int leftX, int lowerY) const {
        assert(0 <= leftX && leftX <= FIELD_WIDTH - width);
        assert(0 <= lowerY && lowerY < MAX_FIELD_HEIGHT);

        auto &collider = harddropColliders[lowerY];
        return Collider{
                collider.boards[0] << leftX,
                collider.boards[1] << leftX,
                collider.boards[2] << leftX,
                collider.boards[3] << leftX,
        };
    }

    Factory Factory::create() {
        using namespace std::literals::string_literals;

        constexpr auto iOffsets = std::array<std::array<Offset, 5>, 4>{
                std::array<Offset, 5>{Offset{0, 0}, {-1, 0}, {2, 0}, {-1, 0}, {2, 0}},
                std::array<Offset, 5>{Offset{-1, 0}, {0, 0}, {0, 0}, {0, 1}, {0, -2}},
                std::array<Offset, 5>{Offset{-1, 1}, {1, 1}, {-2, 1}, {1, 0}, {-2, 0}},
                std::array<Offset, 5>{Offset{0, 1}, {0, 1}, {0, 1}, {0, -1}, {0, 2}},
        };

        constexpr auto oOffsets = std::array<std::array<Offset, 1>, 4>{
                std::array<Offset, 1>{Offset{0, 0}},
                std::array<Offset, 1>{Offset{0, -1}},
                std::array<Offset, 1>{Offset{-1, -1}},
                std::array<Offset, 1>{Offset{-1, 0}},
        };

        constexpr auto otherOffsets = std::array<std::array<Offset, 5>, 4>{
                std::array<Offset, 5>{Offset{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
                std::array<Offset, 5>{Offset{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}},
                std::array<Offset, 5>{Offset{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
                std::array<Offset, 5>{Offset{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}},
        };

        const auto t = Piece::create(PieceType::T, "T"s, std::array<Point, 4>{
            Point{0, 0}, {-1, 0}, {1, 0}, {0, 1},
        }, otherOffsets, tTransforms);

        const auto i = Piece::create(PieceType::I, "I"s, std::array<Point, 4>{
            Point{0, 0}, {-1, 0}, {1, 0}, {2, 0}
        }, iOffsets, iTransforms);

        const auto l = Piece::create(PieceType::L, "L"s, std::array<Point, 4>{
                Point{0, 0}, {-1, 0}, {1, 0}, {1, 1}
        }, otherOffsets, tTransforms);

        const auto j = Piece::create(PieceType::J, "J"s, std::array<Point, 4>{
                Point{0, 0}, {-1, 0}, {1, 0}, {-1, 1}
        }, otherOffsets, tTransforms);

        const auto s = Piece::create(PieceType::S, "S"s, std::array<Point, 4>{
            Point{0, 0}, {-1, 0}, {0, 1}, {1, 1}
        }, otherOffsets, sTransforms);

        const auto z = Piece::create(PieceType::Z, "Z"s, std::array<Point, 4>{
            Point{0, 0}, {1, 0}, {0, 1}, {-1, 1}
        }, otherOffsets, zTransforms);

        const auto o = Piece::create(PieceType::O, "O"s, std::array<Point, 4>{
            Point{0, 0}, {1, 0}, {0, 1}, {1, 1}
        }, oOffsets, oTransforms);

        return create(t, i, l, j, s, z, o);
    }

    Factory Factory::createForSRSPlus() {
        using namespace std::literals::string_literals;

        constexpr std::array<Offset, 20> iCwOffsets{
            // from Spawn
            Offset{1, 0}, {2, 0},{ -1, 0},{-1, -1},{ 2,2},
            // from Right
            Offset{0, -1}, {-1, -1},{ 2, -1},{-1,1},{ 2, -2},
            // from Reverse
            Offset{-1, 0}, { 1, 0},{-2, 0},{ 1,1},{-2, -2},
            // from Left
            Offset{0, 1}, {1, 1},{ -2, 1},{ 2, -1},{-2,2},
        };
        constexpr std::array<Offset, 20> iCcwOffsets{
            // from Spawn
            Offset{0, -1}, { -1, -1},{2, -1},{ 2, -2},{-1,2},
            // from Right
            Offset{-1, 0}, { -2, 0},{1, 0},{-2, -2},{ 1,1},
            // from Reverse
            Offset{0, 1}, {-2, 1},{ 1, 1},{-2,2},{ 1, -1},
            // from Left
            Offset{1, 0}, { 2, 0},{-1, 0},{ 2,2},{-1, -1},
        };

        constexpr auto oOffsets = std::array<std::array<Offset, 1>, 4>{
                std::array<Offset, 1>{Offset{0, 0}},
                std::array<Offset, 1>{Offset{0, -1}},
                std::array<Offset, 1>{Offset{-1, -1}},
                std::array<Offset, 1>{Offset{-1, 0}},
        };

        constexpr auto otherOffsets = std::array<std::array<Offset, 5>, 4>{
                std::array<Offset, 5>{Offset{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
                std::array<Offset, 5>{Offset{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}},
                std::array<Offset, 5>{Offset{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
                std::array<Offset, 5>{Offset{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}},
        };

        constexpr std::array<Offset, 24> oRotate180Offsets{
            // from Spawn
            Offset{1, 1}, {},{},{},{},{},
            // from Right
            Offset{1, -1}, {},{},{},{},{},
            // from Reverse
            Offset{-1, -1}, {},{},{},{},{},
            // from Left
            Offset{-1, 1}, {},{},{},{},{},
        };

        constexpr std::array<Offset, 24> otherRotate180Offsets{
            // from Spawn
            Offset{0, 0}, { 0, 1},{1, 1},{ -1, 1},{1, 0},{-1,0},
            // from Right
            Offset{0, 0}, { 1, 0},{1, 2},{1, 1},{ 0,2},{0,1},
            // from Reverse
            Offset{0, 0}, { 0, -1},{-1, -1},{ 1, -1},{-1, 0},{1,0},
            // from Left
            Offset{0, 0}, { -1, 0},{-1, 2},{ -1,1},{0, 2},{0,1},
        };

        constexpr auto i0To2Offset = Offset{1, -1};
        constexpr auto iRToLOffset = Offset{-1, -1};
        const std::array<Offset, 24> iRotate180Offsets{
            // from Spawn
            otherRotate180Offsets[0] + i0To2Offset, otherRotate180Offsets[1] + i0To2Offset, otherRotate180Offsets[2] + i0To2Offset,
            otherRotate180Offsets[3] + i0To2Offset, otherRotate180Offsets[4] + i0To2Offset, otherRotate180Offsets[5] + i0To2Offset,
            // from Right
            otherRotate180Offsets[6] + iRToLOffset, otherRotate180Offsets[7] + iRToLOffset, otherRotate180Offsets[8] + iRToLOffset,
            otherRotate180Offsets[9] + iRToLOffset, otherRotate180Offsets[10] + iRToLOffset, otherRotate180Offsets[11] + iRToLOffset,
            // from Reverse
            otherRotate180Offsets[12] - i0To2Offset, otherRotate180Offsets[13] - i0To2Offset, otherRotate180Offsets[14] - i0To2Offset,
            otherRotate180Offsets[15] - i0To2Offset, otherRotate180Offsets[16] - i0To2Offset, otherRotate180Offsets[17] - i0To2Offset,
            // from Left
            otherRotate180Offsets[18] - iRToLOffset, otherRotate180Offsets[19] - iRToLOffset, otherRotate180Offsets[20] - iRToLOffset,
            otherRotate180Offsets[21] - iRToLOffset, otherRotate180Offsets[22] - iRToLOffset, otherRotate180Offsets[23] - iRToLOffset,
        };

        const auto t = Piece::create<5, 6>(PieceType::T, "T"s, std::array<Point, 4>{
            Point{0, 0}, {-1, 0}, {1, 0}, {0, 1},
        }, otherOffsets, otherRotate180Offsets, tTransforms);

        const auto i = Piece::create<5, 6>(PieceType::I, "I"s, std::array<Point, 4>{
            Point{0, 0}, {-1, 0}, {1, 0}, {2, 0}
        }, iCwOffsets, iCcwOffsets, iRotate180Offsets, iTransforms);

        const auto l = Piece::create<5, 6>(PieceType::L, "L"s, std::array<Point, 4>{
                Point{0, 0}, {-1, 0}, {1, 0}, {1, 1}
        }, otherOffsets, otherRotate180Offsets, tTransforms);

        const auto j = Piece::create<5, 6>(PieceType::J, "J"s, std::array<Point, 4>{
                Point{0, 0}, {-1, 0}, {1, 0}, {-1, 1}
        }, otherOffsets, otherRotate180Offsets, tTransforms);

        const auto s = Piece::create<5, 6>(PieceType::S, "S"s, std::array<Point, 4>{
            Point{0, 0}, {-1, 0}, {0, 1}, {1, 1}
        }, otherOffsets, otherRotate180Offsets, sTransforms);

        const auto z = Piece::create<5, 6>(PieceType::Z, "Z"s, std::array<Point, 4>{
            Point{0, 0}, {1, 0}, {0, 1}, {-1, 1}
        }, otherOffsets, otherRotate180Offsets, zTransforms);

        const auto o = Piece::create<1, 1>(PieceType::O, "O"s, std::array<Point, 4>{
            Point{0, 0}, {1, 0}, {0, 1}, {1, 1}
        }, oOffsets, oRotate180Offsets, oTransforms);

        return create(t, i, l, j, s, z, o);
    }

    Factory Factory::create(
        const Piece& t,
        const Piece& i,
        const Piece& l,
        const Piece& j,
        const Piece& s,
        const Piece& z,
        const Piece& o
    ) {
        const std::array<Piece, 7> pieces{
            t, i, l, j, s, z, o
        };
        const std::array<Blocks, 4 * 7> blocks{
            t.blocks[RotateType::Spawn], t.blocks[RotateType::Right],
            t.blocks[RotateType::Reverse], t.blocks[RotateType::Left],

            i.blocks[RotateType::Spawn], i.blocks[RotateType::Right],
            i.blocks[RotateType::Reverse], i.blocks[RotateType::Left],

            l.blocks[RotateType::Spawn], l.blocks[RotateType::Right],
            l.blocks[RotateType::Reverse], l.blocks[RotateType::Left],

            j.blocks[RotateType::Spawn], j.blocks[RotateType::Right],
            j.blocks[RotateType::Reverse], j.blocks[RotateType::Left],

            s.blocks[RotateType::Spawn], s.blocks[RotateType::Right],
            s.blocks[RotateType::Reverse], s.blocks[RotateType::Left],

            z.blocks[RotateType::Spawn], z.blocks[RotateType::Right],
            z.blocks[RotateType::Reverse], z.blocks[RotateType::Left],

            o.blocks[RotateType::Spawn], o.blocks[RotateType::Right],
            o.blocks[RotateType::Reverse], o.blocks[RotateType::Left],
        };
        return Factory{pieces, blocks};
    }

    const Piece &Factory::get(PieceType piece) const {
        return pieces[piece];
    }

    const Blocks &Factory::get(PieceType piece, RotateType rotate) const {
        int index = piece * 4 + rotate;
        assert(0 <= index && index < blocks.size());
        return blocks[index];
    }
}
