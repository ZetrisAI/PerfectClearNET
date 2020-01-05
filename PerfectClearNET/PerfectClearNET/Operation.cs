using System;
using System.Collections.Generic;
using System.Linq;

namespace PerfectClearNET {
    /// <summary>
    /// A performable in-game placement described with the piece's final position.
    /// </summary>
    public class Operation {
#pragma warning disable 0169
        /// <summary>
        /// Gets which piece should be used in the Operation.
        /// </summary>
        public int Piece { get; private set; }

        /// <summary>
        /// Gets the X position of the piece.
        /// </summary>
        public int X { get; private set; }

        /// <summary>
        /// Gets the Y position of the piece.
        /// </summary>
        public int Y { get; private set; }

        /// <summary>
        /// Gets the rotation of the piece.
        /// </summary>
        public int R { get; private set; }
#pragma warning restore 0169

        /// <summary>
        /// Creates a custom Operation from raw Perfect Clear Finder output.
        /// </summary>
        /// <param name="input">The raw Perfect Clear Finder output string which resulted from the search.</param>
        public Operation(string input) {
            List<int> parsed = (from i in input.Split(',') select Convert.ToInt32(i)).ToList();

            Piece = PerfectClear.FromFinder[parsed[0]];
            X = parsed[1];
            R = parsed[3];

            Y = 23 - parsed[2] - Convert.ToInt32(Piece == 6 && R == 3);
        }

        /// <summary>
        /// Returns a human-readable string representation of the Operation.
        /// </summary>
        public override string ToString() => $"{PerfectClear.ToChar[Piece]}={X},{Y},{R}";
    }
}
