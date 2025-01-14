#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <memory>
#include <iostream>
#include <sstream>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

class Client {
private:
    boost::asio::io_context _io_context;
    boost::asio::ssl::context _ssl_context;
    std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> ssl_stream;

    std::string _host, _port, _clientId, _secreatKey;

public:
    Client(const std::string& host, const std::string& port, const std::string& clientId, const std::string& secreatKey);
    ~Client();
    
    void connect();
    nlohmann::json sendRequest(const std::string& endpoint, const std::string& method, const nlohmann::json& payload);

    // void placeOrder(const std::string& instrument_name, double amount, double price, const std::string& order_type);
    // void modifyOrder(const std::string& order_id, double amount, double price);
    // void cancelOrder(const std::string& order_id);
    // void ListOrders();
    // void viewCurrentPositions();
};


#endif // CLIENT_HPP
