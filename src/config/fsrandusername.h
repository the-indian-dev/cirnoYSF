#ifndef FSRANDUSERNAME_H
#define FSRANDUSERNAME_H

#include <vector>
#include <string>
#include <random>

class UsernameGenerator {
public:
    /**
     * @brief Constructs the UsernameGenerator and initializes the word lists
     *        and random number engine.
     */
    UsernameGenerator();

    /**
     * @brief Generates a single random username.
     * @return A std::string containing the generated username in the format
     *         "AdjectiveNounNumber" (e.g., "SleepyPanda1234") that is
     *         guaranteed to be less than 16 characters long.
     */
    std::string generate();

private:
    // --- Member Variables ---

    // Lists of words to be used for username generation
    std::vector<std::string> adjectives;
    std::vector<std::string> nouns;

    // The random number engine
    std::mt19937 rng;

    // --- Private Helper Methods ---

    const std::string& getRandomElement(const std::vector<std::string>& vec);
    int getRandomNumber(int min, int max);
};

#endif // FSRANDUSERNAME_H
