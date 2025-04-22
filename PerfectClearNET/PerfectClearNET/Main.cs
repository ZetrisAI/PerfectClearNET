using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;

// Suppresses readonly suggestion
[assembly: System.Diagnostics.CodeAnalysis.SuppressMessage("Style", "IDE0044")]

// Suppresses naming rule violation
[assembly: System.Diagnostics.CodeAnalysis.SuppressMessage("Style", "IDE1006")]

namespace PerfectClearNET {
    /// <summary>
    /// The main PerfectClear class.
    /// Contains all methods for performing actions with the Perfect Clear Finder.
    /// </summary>
    public static class PerfectClear {
        /// <summary>
        /// Converts a piece's index to its regular string representation.
        /// </summary>
        public static readonly string[] ToChar = new string[7] {
            "S", "Z", "J", "L", "T", "O", "I"
        };

        /// <summary>
        /// Converts a Finder piece's index to its regular index.
        /// </summary>
        public static readonly int[] FromFinder = new int[7] {
            4, 6, 3, 2, 0, 1, 5
        };

        /// <param name="success">Whether the search was successful or not.</param>
        public delegate void FinishedEventHandler(bool success);

        /// <summary>
        /// Fires when the Perfect Clear Finder reaches the end of its search, whether naturally or after it was aborted.
        /// </summary>
        public static event FinishedEventHandler Finished;

        /// <summary>
        /// The latest search result from the most recent call to Find.
        /// </summary>
        public static List<Operation> LastSolution = new List<Operation>();

        /// <summary>
        /// The amount of time the latest search took to complete.
        /// </summary>
        public static long LastTime = 0;

        /// <summary>
        /// Checks if the Perfect Clear Finder is currently searching for solutions.
        /// </summary>
        public static bool Running { get => Interface.Running; }

        static PerfectClear() {}

        static ManualResetEvent abortWait;

        /// <summary>
        /// Aborts the currently running search, if there is one.
        /// </summary>
        public static void Abort() {
            if (Interface.Running) {
                abortWait = new ManualResetEvent(false);

                Interface.SetAbort();

                abortWait.WaitOne();
            }
        }

        static bool inited = false;

        /// <summary>
        /// Initializes the Perfect Clear Finder for the desired game.
        /// </summary>
        public static void Initialize(PerfectClearGame game) {
            if (inited) return;

            if (Interface.init_finder(game)) {
                inited = true;
            }
        }

        /// <summary>
        /// Changes the thread count.
        /// </summary>
        /// <param name="threads">Specifies the number of threads to search with.</param>
        public static void SetThreads(uint threads) => Interface.set_threads(threads);

        /// <summary>
        /// <para>Starts searching for a solution/decision for the given game state.</para>
        /// <para>Pieces should be formatted with numbers from 0 to 6 in the order of SZJLTOI. Empty state on the field should be formatted with 255.</para>
        /// <para>Since this method will begin a search in the background, it does not immediately return any data.</para>
        /// <para>When the search ends, the Finished event will fire and LastSolution will update.</para>
        /// <para>The search can be ended prematurely with the Abort method.</para>
        /// </summary>
        /// <param name="field">A 2D array consisting of the field. Should be no smaller than int[10, height].</param>
        /// <param name="queue">The piece queue, can be of any size.</param>
        /// <param name="current">The current piece.</param>
        /// <param name="hold">The piece in hold. Should be null if empty.</param>
        /// <param name="holdAllowed">Is holding is allowed in the game.</param>
        /// <param name="maxHeight">The maximum allowed height of the Perfect Clear. Used to control greed.</param>
        /// <param name="swap">Specifies if garbage blocking is enabled (from Puyo Puyo Tetris' Swap mode). If set to true, the Finder will prioritize PCs with a high combo.</param>
        /// <param name="searchType">The search priority, in order: no softdrop, T-spin, All-spin with Mini, All-spin without Mini, TETR.IO Season 2 keep B2B.</param>
        /// <param name="combo">The combo count.</param>
        /// <param name="b2b">Do you have back-to-back?</param>
        /// <param name="two_line">Whether to optimize the current Perfect Clear for a two-line follow-up.</param>
        public static async void Find(
            int[,] field, int[] queue, int current, int? hold, bool holdAllowed,
            int maxHeight, bool swap, SearchType searchType, int combo, bool b2b, bool two_line
        ) {

            int c = 0;
            int t = -1;
            string f = "";

            for (int i = 19; i >= 0; i--)
                for (int j = 0; j < 10; j++) {
                    if (field[j, i] == 255) {
                        f += '_';
                        c += 1;
                    } else {
                        f += 'X';
                        if (t == -1) t = i + 1;
                    }
                }

            if (t == -1) t = 2;

            string q = ToChar[current];

            for (int i = 0; i < queue.Length; i++)
                q += ToChar[queue[i]];

            string h = (hold == null)? "E" : ToChar[hold.Value];
            if (!holdAllowed) h = "X";

            string result = "";

            await Task.Run(() => {
                result = Interface.Process(f, q, h, t, maxHeight, swap, (int)searchType, combo, b2b, two_line, out long time);

                LastSolution = new List<Operation>();
                LastTime = time;

                bool solved = !result.Equals("-1");

                if (solved) {
                    foreach (string op in result.Split('|'))
                        if (op != "" && op != "0,-1,-1,0")
                            LastSolution.Add(new Operation(op));
                }

                Finished?.Invoke(solved);

                abortWait?.Set();
            });
        }
    }
}
