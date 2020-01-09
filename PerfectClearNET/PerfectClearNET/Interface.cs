using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;

namespace PerfectClearNET {
    static class Interface {
        private static bool abort = false;

        private delegate bool Callback();
        private static Callback AbortCallback;

        [DllImport("sfinder-dll.dll")]
        private static extern void set_abort(Callback func);

        private static object locker = new object();
        public static bool Running { get; private set; } = false;

        [DllImport("sfinder-dll.dll")]
        private static extern void action(string field, string queue, string hold, int height, int max_height, bool swap, int searchtype, int combo, bool b2b, StringBuilder str, int len);

        static Interface() {
            AbortCallback = new Callback(Abort);
            set_abort(AbortCallback);
        }

        public static bool Abort() => abort;
        public static void SetAbort() {
            if (Running) abort = true;
        }

        public static string Process(string field, string queue, string hold, int height, int max_height, bool swap, int search_type, int combo, bool b2b, out long time) {
            StringBuilder sb = new StringBuilder(500);

            abort = true;

            lock (locker) {
                abort = false;

                Stopwatch stopwatch = new Stopwatch();
                stopwatch.Start();

                Running = true;

                action(field, queue, hold, height, max_height, swap, search_type, combo, b2b, sb, sb.Capacity);

                Running = false;

                stopwatch.Stop();
                time = stopwatch.ElapsedMilliseconds;
            }

            return sb.ToString();
        }
    }
}
