#include <fstream>
#include <iostream>
#include <cassert>
#include <unordered_set> 

using namespace std;


void find_missing_positions(const std::string file1, const std::string file2) {
    unordered_set<string> lines_5_plies;
    unordered_set<string> lines_output;

    // Read the lines from 5_plies.txt
    ifstream inputFile1(file1);
    if (!inputFile1) {
        cerr << "Error: Could not open " << file1 << endl;
        return;
    }
    
    string line;
    while (getline(inputFile1, line)) {
        lines_5_plies.insert(line);
    }
    inputFile1.close();

    // Read the lines from output.txt
    ifstream inputFile2(file2);
    if (!inputFile2) {
        cerr << "Error: Could not open " << file2 << endl;
        return;
    }
    while (getline(inputFile2, line)) {
        lines_output.insert(line);
    }
    inputFile2.close();

    // Find and print lines in 5_plies.txt not in output.txt
    cout << "Lines in 5_plies.txt not found in output.txt:" << endl;
    for (const auto &pline : lines_5_plies) {
        if (lines_output.find(pline) == lines_output.end()) {
            cout << pline << endl;
        }
    }
}
