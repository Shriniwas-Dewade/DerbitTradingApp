#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <memory>
#include <iostream>
#include <sstream>
#include <list>
#include <unordered_map>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/asio/ssl.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

class Client {
private:
    boost::asio::io_context _io_context;
    boost::asio::ssl::context _ssl_context;
    boost::beast::websocket::stream<boost::asio::ip::tcp::socket> _ws;
    std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> ssl_stream;

    std::unique_ptr<boost::asio::steady_timer> ping_timer;
    std::thread ping_thread;
    std::atomic<bool> is_running;

    std::string _host, _port, _clientId, _secreatKey, _accessToken;

    std::unordered_map<std::string, std::string> payload_cache;
    std::list<std::string> cache_keys;
    
    static constexpr size_t max_cache_size = 100;
    static constexpr size_t max_payload_size = 500;

    void addToCache(const std::string& key, const std::string& payload);
    std::string getFromCache(const std::string& key);

public:
    Client(const std::string& host, const std::string& port, const std::string& clientId, const std::string& secreatKey);
    ~Client();
    
    void connect();
    void authenticate();
    nlohmann::json sendRequest(const std::string& endpoint, const std::string& method, const nlohmann::json& payload);
    std::string getAccessToken();
    void printMenu();
    void logLatency(const std::chrono::duration<double>& duration);
    void setAccessToken(std::string &token);

    void pingServer();
    void startPing();
    void stopPing();

    void placeOrder(const std::string& instrument_name, double amount, double price, const std::string& order_type);
    void modifyOrder(const std::string& order_id, double amount, double price);
    void cancelOrder(const std::string& order_id);
    void getOrderBook(const std::string& instrument_name);
    void viewCurrentPositions();

    void initWebSocket();
    void subscribeToMarketData(const std::string& symbol);
    void streamMarketData();

    static const nlohmann::json payload;

    nlohmann::json getCachedPayload(const std::string& endpoint, const std::string& method, const nlohmann::json& params);
};


#endif // CLIENT_HPP
