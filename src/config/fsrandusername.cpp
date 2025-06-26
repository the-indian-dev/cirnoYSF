#include "fsrandusername.h"
#include <random> // For std::random_device

// Constructor implementation
UsernameGenerator::UsernameGenerator()
    : adjectives{
        "Happy", "Curious", "Sleepy", "Clever", "Brave", "Quiet", "Sunny", "Lucky",
        "Witty", "Gentle", "Silent", "Eager", "Calm", "Proud", "Swift", "Wise",
        "Bold", "Fierce", "Cozy", "Tiny", "Red", "Cool"
      },
      nouns{
        "Panda", "Robot", "Dragon", "Kitten", "Wizard", "Forest", "Star", "Ocean",
        "Fox", "Wolf", "Eagle", "River", "Comet", "Shadow", "Mage", "Sprite",
        "Tiger", "Lion", "Gem", "Cube", "Byte", "Pixel"
      }
{
    // Seed the random number generator using a non-deterministic source
    std::random_device rd;
    rng.seed(rd());
}

// Private helper to get a random element from a vector
const std::string& UsernameGenerator::getRandomElement(const std::vector<std::string>& vec) {
    std::uniform_int_distribution<size_t> dist(0, vec.size() - 1);
    return vec[dist(rng)];
}

// Private helper to get a random number in a range
int UsernameGenerator::getRandomNumber(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng);
}

std::string UsernameGenerator::generate() {
    // We use a loop to ensure the generated username always meets the length requirement.
    // In practice, this loop will rarely run more than once with typical word lists.
    while (true) {
        // 1. Pick a random adjective and noun
        const std::string& adjective = getRandomElement(adjectives);
        const std::string& noun = getRandomElement(nouns);

        // 2. Generate a random number (e.g., between 100 and 9999)
        int number = getRandomNumber(100, 999);
        std::string numberStr = std::to_string(number);

        // 3. Concatenate the adjective, noun, and number
        std::string username = adjective + noun + numberStr;

        // 4. Check if the length is valid (< 16 characters)
        if (username.length() < 16) {
            // If it's valid, return it. This exits the loop.
            return username;
        }

        // If the username is too long, the loop will continue and try again.
    }
}
