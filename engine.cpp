#include <iostream>

#include "engine.h"
#include "getopt.h"
#include "tests.h"
#include "uci.h"
#include "utilities.h"

using namespace std;

Options getOptions(int argc, char **argv) {
    int getopt;
    int option_index = 0;
    Options toreturn;
    static struct option long_options[] = {
        {"test", no_argument, nullptr, 't'},
        {"diff", no_argument, nullptr, 'd'},
        {nullptr, 0, nullptr, '\0'},
    };

    while ((getopt = getopt_long(argc, argv, "td", long_options,
                                 &option_index)) != -1) {
        switch (getopt) {
        case 't': {
            toreturn.test = true;
            break;
        }
        case 'd': {
            toreturn.diff_check = true;
            break;
        }
        default:
            cerr << "Unknown command line option" << '\n';
            throw Error();
            break;
        }
    }

    return toreturn;
}

int main(int argc, char **argv) {
    try {
        Options options = getOptions(argc, argv);

        if (options.test) {
            Timer t;
            t.start();
            run_tests();
            t.stop();
            cout << "\nTime taken: " << t.timeMs() << " Ms" << endl;
        } else if (options.diff_check) {
            string file1 = "5_plies.txt";
            string file2 = "output.txt";
            find_missing_positions(file1, file2);
        } else {
            run_uci();
        }
    } catch (const Error &error) {
        error.what();
        return 1;
    }

    return 0;
}
