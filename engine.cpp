#include <iostream>
#include <vector>
#include <chrono>
#include "game.h"
#include "utilities.h"
#include "engine.h"
#include "getopt.h"
#include "tests.h"
#include "interface.h"

using namespace std;

class Error {

    public:

    Error(const string & msg_val) : msg(msg_val) {}
    Error() : msg("ERROR\n") {}

    void what() const { 
        cerr << msg;
    }

    private:

    const string msg;
};


Options getOptions(int argc, char ** argv) {

    int getopt;
    int option_index = 0;
    Options toreturn;
    static struct option long_options[] = {
        {"test", no_argument, nullptr, 't'},
        {"diff", no_argument, nullptr, 'd'},
        {nullptr, 0, nullptr, '\0'},
    };

    while ((getopt = getopt_long(argc, argv, "td", long_options, &option_index)) != -1) {
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
} // getOptions


int main(int argc, char ** argv) {

    try {

        Options options;
        options = getOptions(argc, argv);

        if (options.test) {
            // run tests
            Timer t;
            t.start();
            run_tests();
            t.stop();

            cout << "\nTime taken: " << t.timeMs() << " Ms" << endl;
        }

        else if (options.diff_check) {
            // diff check for debugging
            string file1 = "5_plies.txt";
            string file2 = "output.txt";

            find_missing_positions(file1, file2);
        }

        else {
            // run program
            Game * game = new Game;
            char choice;
            cout << "Do you want the white side to be played by the engine? (y/n): \n";
            cin >> choice;
            game->whiteIsEngine = (choice == 'y' || choice == 'Y');
            cout << "Do you want the black side to be played by the engine? (y/n): \n";
            cin >> choice;
            game->blackIsEngine = (choice == 'y' || choice == 'Y');
            game->go();
            delete game;
        }
    }
    catch (const Error & error) {
        error.what();
        return 1;
    }

    return 0;
}

// make all to compile
// ./engine to run program
// ./engine -t to run tests
