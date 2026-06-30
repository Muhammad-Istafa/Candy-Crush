/*
  Candy Crush (console) - Linux-friendly version
  Adapted from the user's provided code to compile on Linux
*/

#include <iostream>
#include <ctime>
#include <chrono>
#include <string>
#include <fstream>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <thread>
#include <cstdlib>
using namespace std;


const int mrow = 10;
const int mcol = 10;
const int max_candytypes = 7;
char board[mrow][mcol];
int current_row = 8;
int current_col = 8;
int current_candytypes = 5;
int score = 0;
int moves = 20;
int timelimit = 60;
string hs_names[11];
int hs_scores[11];
int hs_count = 0;

chrono::steady_clock::time_point start;

// Terminal helpers (Linux)
int kbhit() {
    timeval tv{0,0};
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    return select(STDIN_FILENO + 1, &rfds, nullptr, nullptr, &tv) > 0;
}

int getch_noblock() {
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    char c = 0;
    if (read(STDIN_FILENO, &c, 1) < 0) c = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return c;
}


/* Declarations (kept from original) */
void getuserselection(int& row, int& col);
int swapcandies(int r, int c, char direction);
char getswapdirection();
void loadboard();
bool findmatches(int& score);
void displayboard(int move, int score, int& timelimit);
void refillboard();
void applygravity();
string colorCandy(char c);
void playMatchSound();
int menu();
void saveGame(int currentRemainingTime);
bool loadSavedGame();
void loadHighScores();
void saveHighScores();
void updateHighScores(int newScore);
void viewHighScores();


int main() {
    char playAgain = 'y';

    score = 0;
    moves = 20;

    do {
        int choice = menu();

        if (choice == 5) {
            cout << "Exiting game. Goodbye!\n";
            return 0;
        }

        switch (choice) {
        case 1:
            current_row = 8;
            current_col = 8;
            timelimit = 60;
            current_candytypes = 5;
            break;

        case 2:
            current_row = 10;
            current_col = 10;
            timelimit = 40;
            current_candytypes = 7;
            break;
        case 3:
            viewHighScores();
            continue;
        case 4:
            if (loadSavedGame()) {
                break;
            }
            else {
                continue;
            }
        default:
            cout << "Invalid choice. Exiting." << endl;
            return 0;
        }

        start = chrono::steady_clock::now();
        loadboard();
        displayboard(moves, score, timelimit);

        while (true) {
            auto now = chrono::steady_clock::now();
            int remainingTime = timelimit - chrono::duration_cast<chrono::seconds>(now - start).count();
            if (kbhit()) {
                int ch = getch_noblock();
                if (ch == 'p' || ch == 'P') {
                    saveGame(remainingTime);
                    std::cout << "\nGame Paused and Saved.\n";
                    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
                    break;
                }
            }

            if (remainingTime <= 0) {
                cout << "\nTIME COLLAPSED! Your time is over.\n";
                cout << "Final Score: " << score << endl;
                updateHighScores(score);
                break;
            }

            if (moves <= 0) {
                cout << "\nOUT OF MOVES!\n";
                cout << "Remaining Time: " << remainingTime << " seconds\n";
                cout << "Final Score: " << score << endl;
                updateHighScores(score);
                break;
            }

            int r, c;
            bool matchFound = false;

            getuserselection(r, c);
            char direction = getswapdirection();
            if (direction == 'P') {
                saveGame(remainingTime);
                break;
            }
            moves = swapcandies(r, c, direction);

            do {
                matchFound = findmatches(score);
                if (matchFound) {
                    applygravity();
                    refillboard();
                    displayboard(moves, score, timelimit);
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }
            } while (matchFound);

            displayboard(moves, score, timelimit);
        }

        cout << "\nDo you want to play again? (Y/N): ";
        cin >> playAgain;

    } while (playAgain == 'y' || playAgain == 'Y');
    cout << "\nThanks for playing!\n";
    cout << "Press Enter to exit...";
    cin.ignore();
    cin.get();
    return 0;
}


string colorCandy(char c) {
    switch (c) {
    case '@': return "\033[38;5;217m@\033[0m"; // pastel pink
    case '!': return "\033[38;5;226m!\033[0m"; // pastel yellow
    case '$': return "\033[38;5;51m$\033[0m";  // pastel cyan
    case '%': return "\033[38;5;208m%\033[0m"; // pastel orange
    case '&': return "\033[38;5;177m&\033[0m"; // pastel purple
    case '#': return "\033[38;5;118m#\033[0m";
    case ' ': return "\033[38;5;196m\033[0m";
    case '-': return " ";
    default: return string(1, c);
    }
}

void playMatchSound() {
    cout << '\a' << flush;
}

void applygravity() {
    char temp[mrow][mcol];

    for (int i = 0; i < current_row; i++)
        for (int j = 0; j < current_col; j++)
            temp[i][j] = board[i][j];

    bool moved;
    do {
        moved = false;
        for (int i = 1; i < current_row; i++)
            for (int j = 0; j < current_col; j++)
                if (temp[i][j] == '-' && temp[i - 1][j] != '-') {
                    swap(temp[i][j], temp[i - 1][j]);
                    moved = true;
                }
    } while (moved);

    for (int i = 0; i < current_row; i++)
        for (int j = 0; j < current_col; j++)
            board[i][j] = temp[i][j];
}

void refillboard() {
    char candies[7] = { '@','!','$','%','&', '#', '*' };

    for (int i = 0; i < current_row; i++)
        for (int j = 0; j < current_col; j++)
            if (board[i][j] == '-')
                board[i][j] = candies[rand() % current_candytypes];
}

bool findmatches(int& score) {
    char temp[mrow][mcol];
    bool flag = false;

    for (int i = 0; i < current_row; i++)
        for (int j = 0; j < current_col; j++)
            temp[i][j] = board[i][j];

    for (int i = 0; i < current_row; i++) {
        for (int j = 0; j < current_col; j++) {
            char c = board[i][j];
            if (c == '-') continue;

            bool isLShape = false;

            // Rotation 1 (Bottom-Right L)
            if (i <= current_row - 3 && j <= current_col - 3) {
                if (board[i][j] == c && board[i][j + 1] == c && board[i][j + 2] == c &&
                    board[i + 1][j] == c && board[i + 2][j] == c) {

                    temp[i][j] = temp[i][j + 1] = temp[i][j + 2] = '-';
                    temp[i + 1][j] = temp[i + 2][j] = '-';
                    flag = true; isLShape = true;
                    playMatchSound(); score += 25;
                }
            }
            // Rotation 2 (Bottom-Left L)
            else if (i <= current_row - 3 && j >= 2) {
                if (board[i][j] == c && board[i][j - 1] == c && board[i][j - 2] == c &&
                    board[i + 1][j] == c && board[i + 2][j] == c) {

                    temp[i][j] = temp[i][j - 1] = temp[i][j - 2] = '-';
                    temp[i + 1][j] = temp[i + 2][j] = '-';
                    flag = true; isLShape = true;
                    playMatchSound(); score += 25;
                }
            }
            // Rotation 3 (Top-Right L)
            else if (i >= 2 && j <= current_col - 3) {
                if (board[i][j] == c && board[i - 1][j] == c && board[i - 2][j] == c &&
                    board[i][j + 1] == c && board[i][j + 2] == c) {

                    temp[i][j] = temp[i - 1][j] = temp[i - 2][j] = '-';
                    temp[i][j + 1] = temp[i][j + 2] = '-';
                    flag = true; isLShape = true;
                    playMatchSound(); score += 25;
                }
            }
            // Rotation 4 (Top-Left L)
            else if (i >= 2 && j >= 2) {
                if (board[i][j] == c && board[i - 1][j] == c && board[i - 2][j] == c &&
                    board[i][j - 1] == c && board[i][j - 2] == c) {

                    temp[i][j] = temp[i - 1][j] = temp[i - 2][j] = '-';
                    temp[i][j - 1] = temp[i][j - 2] = '-';
                    flag = true; isLShape = true;
                    playMatchSound(); score += 25;
                }
            }

            if (isLShape) continue;

            // Horizontal 3+
            if (j <= current_col - 5 && board[i][j + 1] == c && board[i][j + 2] == c && board[i][j + 3] == c && board[i][j + 4] == c) {
                temp[i][j] = temp[i][j + 1] = temp[i][j + 2] = temp[i][j + 3] = temp[i][j + 4] = '-';
                flag = true; playMatchSound(); score += 20;
            }
            // Horizontal 4
            else if (j <= current_col - 4 && board[i][j + 1] == c && board[i][j + 2] == c && board[i][j + 3] == c) {
                temp[i][j] = temp[i][j + 1] = temp[i][j + 2] = temp[i][j + 3] = '-';
                flag = true; playMatchSound(); score += 15;
            }
            // Horizontal 5
            else if (j <= current_col - 3 && board[i][j + 1] == c && board[i][j + 2] == c) {
                temp[i][j] = temp[i][j + 1] = temp[i][j + 2] = '-';
                flag = true; playMatchSound(); score += 10;
            }

            // Vertical 3+
            if (i <= current_row - 5 && board[i + 1][j] == c && board[i + 2][j] == c && board[i + 3][j] == c && board[i + 4][j] == c) {
                temp[i][j] = temp[i + 1][j] = temp[i + 2][j] = temp[i + 3][j] = temp[i + 4][j] = '-';
                flag = true; playMatchSound(); score += 20;
            }
            // Vertical 4
            else if (i <= current_row - 4 && board[i + 1][j] == c && board[i + 2][j] == c && board[i + 3][j] == c) {
                temp[i][j] = temp[i + 1][j] = temp[i + 2][j] = temp[i + 3][j] = '-';
                flag = true; playMatchSound(); score += 15;
            }
            // Vertical 5
            else if (i <= current_row - 3 && board[i + 1][j] == c && board[i + 2][j] == c) {
                temp[i][j] = temp[i + 1][j] = temp[i + 2][j] = '-';
                flag = true; playMatchSound(); score += 10;
            }
        }
    }

    for (int i = 0; i < current_row; i++)
        for (int j = 0; j < current_col; j++)
            board[i][j] = temp[i][j];

    return flag;
}

void getuserselection(int& row, int& col) {
    do {
        cout << "Enter Row (0-" << current_row - 1 << "): ";
        while (!(cin >> row)) { cin.clear(); cin.ignore(100, '\n'); cout << "Invalid. Enter Row: "; }
        cout << "Enter Column (0-" << current_col - 1 << "): ";
        while (!(cin >> col)) { cin.clear(); cin.ignore(100, '\n'); cout << "Invalid. Enter Column: "; }

        if (row < 0 || row >= current_row || col < 0 || col >= current_col) {
            cout << "Invalid coordinates! Try again.\n";
        }
    } while (row < 0 || row >= current_row || col < 0 || col >= current_col);
}

char getswapdirection() {
    cout << "Press arrow key to swap Or P to pause ...\n";
    int key = getch_noblock();
    if (key == 'p' || key == 'P') return 'P';
    if (key == 27) { // ESC sequence
        int k1 = getch_noblock();
        int k2 = getch_noblock();
        if (k2 == 'A') return 'U';
        if (k2 == 'B') return 'D';
        if (k2 == 'D') return 'L';
        if (k2 == 'C') return 'R';
    }
    return 'X';
}

int swapcandies(int row, int col, char direction) {
    int nrow = row, ncol = col;
    if (direction == 'U') nrow--;
    if (direction == 'D') nrow++;
    if (direction == 'L') ncol--;
    if (direction == 'R') ncol++;

    if (nrow < 0 || nrow >= current_row || ncol < 0 || ncol >= current_col) {
        cout << "Invalid swap! Out of bounds.\n";
        return moves;
    }

    moves--;
    swap(board[row][col], board[nrow][ncol]);
    return moves;
}

void loadboard() {
    srand((unsigned)time(nullptr));
    char candies[7] = { '@','!','$','%','&', '#', '*' };

    for (int i = 0; i < current_row; i++) {
        for (int j = 0; j < current_col; j++) {
            char c;
            do {
                c = candies[rand() % current_candytypes];
            } while ((i >= 2 && board[i - 1][j] == c && board[i - 2][j] == c) ||
                (j >= 2 && board[i][j - 1] == c && board[i][j - 2] == c));
            board[i][j] = c;
        }
    }
}

void displayboard(int move, int score, int& timelimit) {
    system("clear");
    auto now = chrono::steady_clock::now();
    int remaining = timelimit - chrono::duration_cast<chrono::seconds>(now - start).count();
    if (remaining < 0) remaining = 0;

    cout << "\nCandy Crush (" << current_row << "x" << current_col << ")\tMoves: " << move << "\tScore: " << score << "\tTime: " << remaining << "s\n";

    cout << "    ";
    for (int j = 0; j < current_col; j++) cout << j << "   ";
    cout << "\n   ";
    for (int j = 0; j < current_col; j++) cout << "";
    cout << endl;

    for (int i = 0; i < current_row; i++) {
        cout << i << " | ";
        for (int j = 0; j < current_col; j++)
            cout << colorCandy(board[i][j]) << "   ";
        cout << endl << endl;
    }
}

int menu() {
    int choice;
    system("clear");
    cout << "=======================================" << endl;
    cout << "          CANDY CRUSH GAME MENU        " << endl;
    cout << "=======================================" << endl;
    cout << "1. Play Easy Mode (8x8, 60s)" << endl;
    cout << "2. Play Hard Mode (10x10, 40s)" << endl;
    cout << "3. View High Scores" << endl;
    cout << "4. Load Saved Game" << endl;
    cout << "5. Exit" << endl;
    cout << "=======================================" << endl;
    cout << "Enter your choice: ";

    while (!(cin >> choice) || choice < 1 || choice > 5) {
        cin.clear();
        cin.ignore(100, '\n');
        cout << "Invalid choice. Please enter a number between 1 and 5: ";
    }
    return choice;
}

void saveGame(int currentRemainingTime) {
    ofstream file("savegame.txt");
    if (file.is_open()) {
        file << score << " " << moves << " " << currentRemainingTime << " "
            << current_row << " " << current_col << " " << current_candytypes << endl;

        for (int i = 0; i < current_row; i++) {
            for (int j = 0; j < current_col; j++) {
                file << board[i][j] << " ";
            }
            file << endl;
        }

        file.close();
        cout << "\nGame Saved Successfully! Press Enter to continue...";
        cin.ignore(); cin.get();
    }
    else {
        cout << "Error saving game!";
    }
}

bool loadSavedGame() {
    ifstream file("savegame.txt");
    
    if (!file.is_open()) {
        cout << "No saved game found!" << endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        return false;
    }

    file >> score >> moves >> timelimit >> current_row >> current_col >> current_candytypes;

    for (int i = 0; i < current_row; i++) {
        for (int j = 0; j < current_col; j++) {
            file >> board[i][j];
        }
    }
    
    file.close();
    return true;
}

void loadHighScores() {
    ifstream file("highestScore.txt");
    if (!file.is_open()) return;

    hs_count = 0;
    while (hs_count < 10 && file >> hs_names[hs_count] >> hs_scores[hs_count]) {
        hs_count++;
    }
    file.close();
}

void saveHighScores() {
    ofstream file("highestScore.txt");
    if (!file.is_open()) {
        cout << "Error saving high scores!" << endl;
        return;
    }
    int limit = (hs_count < 10) ? hs_count : 10;
    for (int i = 0; i < limit; i++) {
        file << hs_names[i] << " " << hs_scores[i] << endl;
    }
    file.close();
}

void updateHighScores(int newScore) {
    loadHighScores();

    cout << "\nCongratulations! You finished with " << newScore << " points.\n";
    cout << "Enter your name : ";
    cin >> hs_names[hs_count];
    hs_scores[hs_count] = newScore;
    hs_count++;

    for (int i = 0; i < hs_count - 1; i++) {
        for (int j = 0; j < hs_count - i - 1; j++) {
            if (hs_scores[j] < hs_scores[j + 1]) {
                int tempScore = hs_scores[j];
                hs_scores[j] = hs_scores[j + 1];
                hs_scores[j + 1] = tempScore;
                string tempName = hs_names[j];
                hs_names[j] = hs_names[j + 1];
                hs_names[j + 1] = tempName;
            }
        }
    }
    if (hs_count > 10) hs_count = 10;

    saveHighScores();
    cout << "High Score Saved!\n";
}

void viewHighScores() {
    loadHighScores();

    system("clear");
    cout << "=======================================" << endl;
    cout << "          TOP 10 HIGH SCORES           " << endl;
    cout << "=======================================" << endl;

    if (hs_count == 0) {
        cout << "No high scores yet!" << endl;
    }
    else {
        cout << "Rank\tName\t\tScore" << endl;
        cout << "----\t----\t\t-----" << endl;
        for (int i = 0; i < hs_count; i++) {
            cout << i + 1 << ".\t" << hs_names[i] << "\t\t" << hs_scores[i] << endl;
        }
    }
    cout << "\n";
    cout << "Press Enter to continue...";
    cin.ignore(); cin.get();
}
