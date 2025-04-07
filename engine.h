#ifndef ENGINE_H
#define ENGINE_H

struct Options {
    bool test = false;
    bool diff_check = false;
};


class Timer {
    private:
        std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
        std::chrono::time_point<std::chrono::high_resolution_clock> end_time;
        bool running = false;

    public:
        // Start the timer
        void start() {
            start_time = std::chrono::high_resolution_clock::now();
            running = true;
        }

        // Stop the timer
        void stop() {
            if (!running) {
                std::cerr << "Timer was not running!" << std::endl;
                return;
            }
            end_time = std::chrono::high_resolution_clock::now();
            running = false;
        }

        // Get elapsed time in milliseconds
        double timeMs() const {
            auto end = running ? std::chrono::high_resolution_clock::now() : end_time;
            return std::chrono::duration<double, std::milli>(end - start_time).count();
        }

        // Get elapsed time in seconds
        double timeS() const {
            auto end = running ? std::chrono::high_resolution_clock::now() : end_time;
            return std::chrono::duration<double>(end - start_time).count();
        }
};


int evaluate_board(const Board& board);
Options getOptions(int argc, char ** argv);

#endif
