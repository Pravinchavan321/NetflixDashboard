#include <iostream>
#include <map>
#include <string>
#include <ctime>
#include <vector>
#include <numeric>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <cstdlib>

using json = nlohmann::json;

// Data structure to store mood entries
std::map<std::string, int> mood_log;

// Callback function for curl to capture response
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Function to get current timestamp
std::string get_timestamp() {
    time_t now = time(nullptr);
    char buf[20];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return std::string(buf);
}

// Log user's mood
void log_mood() {
    int mood_score;
    std::cout << "Enter your mood (1-10, 1=very low, 10=very high): ";
    std::cin >> mood_score;
    if (std::cin.fail() || mood_score < 1 || mood_score > 10) {
        std::cout << "Invalid input. Please enter a number between 1 and 10.\n";
        std::cin.clear();
        std::cin.ignore(10000, '\n');
        return;
    }
    std::string timestamp = get_timestamp();
    mood_log[timestamp] = mood_score;
    std::cout << "Mood logged: " << mood_score << " at " << timestamp << "\n";
}

// Analyze recent moods
double analyze_mood() {
    if (mood_log.empty()) return -1.0;
    std::vector<int> recent_moods;
    auto it = mood_log.rbegin();
    for (int i = 0; i < 3 && it != mood_log.rend(); ++i, ++it) {
        recent_moods.push_back(it->second);
    }
    if (recent_moods.empty()) return -1.0;
    return std::accumulate(recent_moods.begin(), recent_moods.end(), 0.0) / recent_moods.size();
}

// Get activity suggestion from OpenAI API
std::string get_openai_suggestion(double mood_score) {
    CURL* curl = curl_easy_init();
    std::string response_string;
    if (!curl) {
        return "Error initializing curl.";
    }

    // Get API key from environment variable
    const char* api_key = std::getenv("OPENAI_API_KEY");
    if (!api_key) {
        return "OpenAI API key not found.";
    }

    // Construct prompt
    std::string prompt = "User's mood is " + std::to_string(mood_score) + "/10. Suggest a simple, positive activity to improve their mental health. Keep the suggestion concise.";
    json request_body = {
        {"model", "gpt-3.5-turbo"},
        {"messages", {{{"role", "user"}, {"content", prompt}}}},
        {"max_tokens", 50}
    };

    // Set up curl
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/chat/completions");
    std::string auth_header = "Authorization: Bearer " + std::string(api_key);
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth_header.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.dump().c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

    // Perform request
    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return "Failed to connect to OpenAI API.";
    }

    // Parse response
    try {
        json response_json = json::parse(response_string);
        if (response_json.contains("choices") && !response_json["choices"].empty()) {
            return response_json["choices"][0]["message"]["content"].get<std::string>();
        }
    } catch (const std::exception& e) {
        return "Error parsing OpenAI response.";
    }
    return "No suggestion available.";
}

// Suggest activity based on mood
void suggest_activity(double mood_score) {
    if (mood_score < 0) {
        std::cout << "No mood data available. Please log your mood first.\n";
        return;
    }
    std::cout << "Average recent mood: " << mood_score << "\n";
    if (mood_score <= 3) {
        std::cout << "You seem to be feeling low. Here's a suggestion:\n";
    } else if (mood_score <= 7) {
        std::cout << "You seem to be feeling okay. Here's a suggestion:\n";
    } else {
        std::cout << "You're feeling great! Here's a suggestion:\n";
    }
    std::string suggestion = get_openai_suggestion(mood_score);
    std::cout << suggestion << "\n";
}

int main() {
    std::cout << "Mental Health Simulator\n";
    while (true) {
        std::cout << "\n1. Log Mood\n2. Get Activity Suggestion\n3. Exit\nChoose an option: ";
        int choice;
        std::cin >> choice;
        if (std::cin.fail()) {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            std::cout << "Invalid input. Please enter a number.\n";
            continue;
        }
        switch (choice) {
            case 1:
                log_mood();
                break;
            case 2:
                suggest_activity(analyze_mood());
                break;
            case 3:
                std::cout << "Goodbye!\n";
                return 0;
            default:
                std::cout << "Invalid option. Try again.\n";
        }
    }
}