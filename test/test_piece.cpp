#include "gtest/gtest.h"
#include "core/types.hpp"
#include "core/piece.hpp"

namespace {
    using namespace core;
    using namespace std::literals::string_literals;

    class FactoryTest : public ::testing::Test {
    };

    TEST_F(FactoryTest, getPiece) {
        auto factory = Factory::create();

        {
            const Piece &piece = factory.get(PieceType::T);
            EXPECT_EQ(piece.name, "T"s);
        }

        {
            const Piece &piece = factory.get(PieceType::I);
            EXPECT_EQ(piece.name, "I"s);
        }

        {
            const Piece &piece = factory.get(PieceType::L);
            EXPECT_EQ(piece.name, "L"s);
        }

        {
            const Piece &piece = factory.get(PieceType::J);
            EXPECT_EQ(piece.name, "J"s);
        }

        {
            const Piece &piece = factory.get(PieceType::S);
            EXPECT_EQ(piece.name, "S"s);
        }

        {
            const Piece &piece = factory.get(PieceType::Z);
            EXPECT_EQ(piece.name, "Z"s);
        }

        {
            const Piece &piece = factory.get(PieceType::O);
            EXPECT_EQ(piece.name, "O"s);
        }
    }

    TEST_F(FactoryTest, getBlocks) {
        auto factory = Factory::create();

        {
            const Blocks &blocks = factory.get(PieceType::T, RotateType::Spawn);
            EXPECT_EQ(blocks.minX, -1);
            EXPECT_EQ(blocks.maxX, 1);
            EXPECT_EQ(blocks.minY, 0);
            EXPECT_EQ(blocks.maxY, 1);
        }

        {
            const Blocks &blocks = factory.get(PieceType::T, RotateType::Right);
            EXPECT_EQ(blocks.minX, 0);
            EXPECT_EQ(blocks.maxX, 1);
            EXPECT_EQ(blocks.minY, -1);
            EXPECT_EQ(blocks.maxY, 1);
        }

        {
            const Blocks &blocks = factory.get(PieceType::O, RotateType::Reverse);
            EXPECT_EQ(blocks.minX, -1);
            EXPECT_EQ(blocks.maxX, 0);
            EXPECT_EQ(blocks.minY, -1);
            EXPECT_EQ(blocks.maxY, 0);
        }
    }
}
