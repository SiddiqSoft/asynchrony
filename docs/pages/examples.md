/**
 * @page examples Examples
 *
 * @section ex_http_requests HTTP Request Queue
 *
 * Process HTTP requests asynchronously:
 *
 * ```cpp
 * #include "siddiqsoft/simple_pool.hpp"
 * #include <curl/curl.h>
 *
 * struct HttpRequest {
 *     std::string url;
 *     std::string method;
 *     std::string body;
 * };
 *
 * int main() {
 *     siddiqsoft::simple_pool<HttpRequest> pool{
 *         [](auto&& req) {
 *             CURL* curl = curl_easy_init();
 *             if (curl) {
 *                 curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());
 *                 curl_easy_perform(curl);
 *                 curl_easy_cleanup(curl);
 *             }
 *         }
 *     };
 *
 *     // Queue requests
 *     for (int i = 0; i < 100; ++i) {
 *         pool.queue(HttpRequest{
 *             "https://api.example.com/data",
 *             "POST",
 *             "data=" + std::to_string(i)
 *         });
 *     }
 *
 *     std::this_thread::sleep_for(std::chrono::seconds(5));
 *     return 0;
 * }
 * ```
 *
 * @section ex_database_operations Database Operations
 *
 * Process database queries with a connection pool:
 *
 * ```cpp
 * #include "siddiqsoft/simple_pool.hpp"
 * #include "siddiqsoft/resource_pool.hpp"
 *
 * class DbConnection {
 * public:
 *     void execute(const std::string& query) {
 *         // Execute query
 *     }
 * };
 *
 * struct DbQuery {
 *     std::string sql;
 *     std::vector<std::string> params;
 * };
 *
 * int main() {
 *     siddiqsoft::resource_pool<DbConnection> connPool;
 *     siddiqsoft::simple_pool<DbQuery> queryPool{
 *         [&connPool](auto&& query) {
 *             auto conn = connPool.checkout();
 *             conn.execute(query.sql);
 *             connPool.checkin(std::move(conn));
 *         }
 *     };
 *
 *     // Queue queries
 *     for (int i = 0; i < 50; ++i) {
 *         queryPool.queue(DbQuery{
 *             "SELECT * FROM users WHERE id = ?",
 *             {std::to_string(i)}
 *         });
 *     }
 *
 *     std::this_thread::sleep_for(std::chrono::seconds(3));
 *     return 0;
 * }
 * ```
 *
 * @section ex_file_processing File Processing
 *
 * Process multiple files in parallel:
 *
 * ```cpp
 * #include "siddiqsoft/roundrobin_pool.hpp"
 * #include <fstream>
 *
 * struct FileTask {
 *     std::string filename;
 *     std::string operation;  // "read", "write", "process"
 * };
 *
 * int main() {
 *     siddiqsoft::roundrobin_pool<FileTask> pool{
 *         [](auto&& task) {
 *             if (task.operation == "read") {
 *                 std::ifstream file(task.filename);
 *                 std::string line;
 *                 while (std::getline(file, line)) {
 *                     // Process line
 *                 }
 *             }
 *         }
 *     };
 *
 *     // Queue file operations
 *     for (int i = 0; i < 100; ++i) {
 *         pool.queue(FileTask{
 *             "data_" + std::to_string(i) + ".txt",
 *             "read"
 *         });
 *     }
 *
 *     std::this_thread::sleep_for(std::chrono::seconds(5));
 *     return 0;
 * }
 * ```
 *
 * @section ex_monitoring System Monitoring
 *
 * Monitor system metrics periodically:
 *
 * ```cpp
 * #include "siddiqsoft/periodic_worker.hpp"
 * #include <iostream>
 * #include <chrono>
 *
 * int main() {
 *     siddiqsoft::periodic_worker<> monitor{
 *         []() {
 *             auto now = std::chrono::system_clock::now();
 *             auto time = std::chrono::system_clock::to_time_t(now);
 *             std::cout << "System check at: " << std::ctime(&time);
 *             // Perform monitoring tasks
 *         },
 *         std::chrono::seconds(10),  // Every 10 seconds
 *         "system-monitor"
 *     };
 *
 *     // Keep the program running
 *     std::this_thread::sleep_for(std::chrono::minutes(1));
 *     return 0;
 * }
 * ```
 *
 * @section ex_event_processing Event Processing
 *
 * Process events from a queue:
 *
 * ```cpp
 * #include "siddiqsoft/simple_pool.hpp"
 *
 * enum class EventType { USER_LOGIN, USER_LOGOUT, DATA_UPDATE };
 *
 * struct Event {
 *     EventType type;
 *     std::string userId;
 *     std::string data;
 *     std::chrono::system_clock::time_point timestamp;
 * };
 *
 * int main() {
 *     siddiqsoft::simple_pool<Event> eventProcessor{
 *         [](auto&& event) {
 *             switch (event.type) {
 *                 case EventType::USER_LOGIN:
 *                     std::cout << "User " << event.userId << " logged in\n";
 *                     break;
 *                 case EventType::USER_LOGOUT:
 *                     std::cout << "User " << event.userId << " logged out\n";
 *                     break;
 *                 case EventType::DATA_UPDATE:
 *                     std::cout << "Data updated: " << event.data << "\n";
 *                     break;
 *             }
 *         }
 *     };
 *
 *     // Simulate events
 *     for (int i = 0; i < 1000; ++i) {
 *         eventProcessor.queue(Event{
 *             EventType::DATA_UPDATE,
 *             "user_" + std::to_string(i % 10),
 *             "value_" + std::to_string(i),
 *             std::chrono::system_clock::now()
 *         });
 *     }
 *
 *     std::this_thread::sleep_for(std::chrono::seconds(5));
 *     return 0;
 * }
 * ```
 *
 * @section ex_producer_consumer Producer-Consumer Pattern
 *
 * Implement a producer-consumer pattern:
 *
 * ```cpp
 * #include "siddiqsoft/simple_pool.hpp"
 * #include <queue>
 * #include <mutex>
 *
 * struct DataItem {
 *     int id;
 *     std::vector<int> data;
 * };
 *
 * int main() {
 *     std::queue<DataItem> buffer;
 *     std::mutex bufferMutex;
 *
 *     // Consumer pool
 *     siddiqsoft::simple_pool<DataItem> consumers{
 *         [](auto&& item) {
 *             // Process item
 *             std::cout << "Processing item " << item.id << "\n";
 *         }
 *     };
 *
 *     // Producer thread
 *     std::thread producer{[&]() {
 *         for (int i = 0; i < 100; ++i) {
 *             DataItem item{i, {1, 2, 3, 4, 5}};
 *             consumers.queue(std::move(item));
 *             std::this_thread::sleep_for(std::chrono::milliseconds(10));
 *         }
 *     }};
 *
 *     producer.join();
 *     std::this_thread::sleep_for(std::chrono::seconds(2));
 *     return 0;
 * }
 * ```
 *
 * @section ex_priority_workers Priority-Based Workers
 *
 * Use different priority levels for different types of work:
 *
 * ```cpp
 * #include "siddiqsoft/simple_worker.hpp"
 *
 * struct Task {
 *     std::string name;
 *     int priority;
 * };
 *
 * int main() {
 *     // High priority worker for critical tasks
 *     siddiqsoft::simple_worker<Task, 5> criticalWorker{
 *         [](auto&& task) {
 *             std::cout << "Critical: " << task.name << std::endl;
 *         }
 *     };
 *
 *     // Normal priority worker for regular tasks
 *     siddiqsoft::simple_worker<Task, 0> normalWorker{
 *         [](auto&& task) {
 *             std::cout << "Normal: " << task.name << std::endl;
 *         }
 *     };
 *
 *     // Low priority worker for background tasks
 *     siddiqsoft::simple_worker<Task, -5> backgroundWorker{
 *         [](auto&& task) {
 *             std::cout << "Background: " << task.name << std::endl;
 *         }
 *     };
 *
 *     criticalWorker.queue(Task{"urgent", 5});
 *     normalWorker.queue(Task{"normal", 0});
 *     backgroundWorker.queue(Task{"background", -5});
 *
 *     std::this_thread::sleep_for(std::chrono::seconds(1));
 *     return 0;
 * }
 * ```
 *
 * @section ex_json_monitoring JSON-Based Monitoring
 *
 * Monitor worker state using JSON serialization:
 *
 * ```cpp
 * #include "siddiqsoft/simple_pool.hpp"
 * #include <nlohmann/json.hpp>
 *
 * int main() {
 *     siddiqsoft::simple_pool<int> pool{
 *         [](auto&& item) {
 *             std::this_thread::sleep_for(std::chrono::milliseconds(100));
 *         }
 *     };
 *
 *     // Queue some work
 *     for (int i = 0; i < 50; ++i) {
 *         pool.queue(i);
 *     }
 *
 *     // Monitor pool state
 *     for (int i = 0; i < 5; ++i) {
 *         auto state = pool.toJson();
 *         std::cout << "Pool state: " << state.dump(2) << std::endl;
 *         std::this_thread::sleep_for(std::chrono::milliseconds(500));
 *     }
 *
 *     return 0;
 * }
 * ```
 */
