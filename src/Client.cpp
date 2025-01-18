#include "Client.hpp"
#include <boost/beast/websocket.hpp>
#include <boost/beast/core.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/beast/websocket/teardown.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace boost 
{
    namespace beast 
    {

        void teardown(
            boost::beast::role_type role,
            boost::asio::ssl::stream<boost::asio::ip::tcp::socket>& stream,
            boost::system::error_code& ec)
        {
            // Perform SSL shutdown
            stream.shutdown(ec);
        }

    } // namespace beast
} // namespace boost


using json = nlohmann::json;

Client::Client(const std::string& host, const std::string& port, const std::string& clientId, const std::string& secreatKey)
    : _wsConnected(false), _io_context_ws(), _ssl_context_ws(boost::asio::ssl::context::tlsv12_client), _ws(_io_context_ws, _ssl_context_ws), _ssl_context(boost::asio::ssl::context::tlsv13_client), _host(host), _port(port), _clientId(clientId), _secreatKey(secreatKey)
{
    _ssl_context_ws.set_default_verify_paths();
    _ssl_context_ws.set_verify_mode(boost::asio::ssl::verify_peer);
     _ssl_context.set_options(
        boost::asio::ssl::context::default_workarounds |
        boost::asio::ssl::context::no_sslv3 |
        boost::asio::ssl::context::no_tlsv1 |
        boost::asio::ssl::context::no_tlsv1_1 |
        boost::asio::ssl::context::no_tlsv1_2
    );
    ssl_stream = std::make_shared<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>(_io_context, _ssl_context);
}

void Client::connect() 
{
    try 
    {
        boost::asio::ip::tcp::resolver resolver(_io_context);
        auto endpoints = resolver.resolve(_host, _port);
        boost::asio::connect(ssl_stream->lowest_layer(), endpoints);
        ssl_stream->handshake(boost::asio::ssl::stream_base::client);
        boost::asio::ip::tcp::no_delay option(true);
        ssl_stream->lowest_layer().set_option(option);
        spdlog::info("Connected to {}:{}", _host, _port);
        startPing();
    } 
    catch (const boost::system::system_error& ex) 
    {
        spdlog::error("Connection error: {}", ex.what());
        throw;
    }
}

Client::~Client()
{
    try 
    {
        if (ssl_stream) 
        {
            ssl_stream->lowest_layer().close();
        }
        
        if (_ws.is_open())
        {
            boost::system::error_code ec;
            _ws.close(boost::beast::websocket::close_code::normal, ec);
            
            if (ec) 
            {
                spdlog::error("WebSocket close error: {}", ec.message());
            }
        }

        stopPing();
        spdlog::info("Client resources cleaned up.");
    } 
    catch (const std::exception& ex) 
    {
        spdlog::error("Error during resource cleanup: {}", ex.what());
    }
}

void Client::printMenu() 
{
    std::cout << "\n1. Place Order\n";
    std::cout << "2. Cancel Order\n";
    std::cout << "3. Modify Order\n";
    std::cout << "4. Get Order Book\n";
    std::cout << "5. View Current Positions\n";
    std::cout << "6. Subscribe to Market Data\n";
    std::cout << "7. Exit.\n";
    std::cout << "Enter your choice: ";
}

void Client::pingServer() 
{
    try 
    {
        nlohmann::json payload = {
            {"jsonrpc", "2.0"},
            {"id", 1},
            {"method", "public/test"}, 
            {"params", {}}
        };

        auto response = sendRequest("/api/v2/public/test", "GET", payload);

        if (!response.is_null()) 
        {
            spdlog::info("Ping successful. Connection is alive.");
            std::cin.clear();
        } 
        else 
        {
            spdlog::warn("Ping failed. Reconnecting...");
            connect();
        }
    } 
    catch (const std::exception& ex) 
    {
        spdlog::error("Ping error: {}", ex.what());
        connect();
    }
}

void Client::logLatency(const std::chrono::duration<double>& duration) 
{
    double seconds = duration.count();

    if (seconds >= 1.0) 
    {
        spdlog::info("Program latency: {:.3f} seconds", seconds);
    } 
    else if (seconds >= 0.001) 
    {
        spdlog::info("Program latency: {:.3f} milliseconds", seconds * 1000.0);
    } 
    else 
    {
        spdlog::info("Program latency: {:.3f} microseconds", seconds * 1'000'000.0);
    }
}

void Client::startPing() 
{
    is_running = true;
    ping_timer = std::make_unique<boost::asio::steady_timer>(_io_context);

    ping_thread = std::thread([this]() {
        while (is_running) 
        {
            ping_timer->expires_after(std::chrono::seconds(59));

            ping_timer->async_wait([this](const boost::system::error_code& ec) {
                if (ec) 
                {
                    spdlog::info("Ping timer : {}", ec.message());
                } 
                else 
                {
                    pingServer();
                }
            });

            _io_context.run();
            _io_context.restart();
        }
    });
}

void Client::stopPing() 
{
    is_running = false;
    
    if (ping_timer) 
    {
        ping_timer->cancel();
    }

    if (ping_thread.joinable()) 
    {
        ping_thread.join();
    }
}


std::string Client::getAccessToken()
{
    return _accessToken;
}

void Client::setAccessToken(std::string &token)
{
    _accessToken = token;
}

json Client::sendRequest(const std::string& endpoint, const std::string& method, const json& payload) 
{
    try 
    {
        auto start = std::chrono::high_resolution_clock::now();
        std::string serialized_payload = payload.dump();

        std::ostringstream request_stream;
        request_stream << method << " " << endpoint << " HTTP/1.1\r\n"
                    << "Host: " << _host << "\r\n";

        if (!_accessToken.empty()) 
        {
            request_stream << "Authorization: Bearer " << _accessToken << "\r\n";
        }

        request_stream << "Content-Type: application/json\r\n"
                    << "Content-Length: " << serialized_payload.size() << "\r\n"
                    << "Connection: keep-alive\r\n\r\n"
                    << serialized_payload;

        boost::asio::write(*ssl_stream, boost::asio::buffer(request_stream.str()));
        boost::asio::streambuf response_buffer;
        std::size_t max_buffer_size = 1024;
        response_buffer.prepare(max_buffer_size);

        std::size_t bytes_read = boost::asio::read(*ssl_stream, response_buffer, boost::asio::transfer_at_least(1));
        response_buffer.commit(bytes_read);

        if (response_buffer.size() > max_buffer_size) 
        {
            throw std::runtime_error("Buffer overflow: Maximum size exceeded");
        }
    
        std::istream response_stream(&response_buffer);

        std::string header;
        size_t content_length = 0;
        while (std::getline(response_stream, header) && header != "\r") 
        {
            if (header.find("Content-Length:") == 0) 
            {
                content_length = std::stoul(header.substr(16));
            }
        }

        std::string response_body;
        response_body.reserve(content_length);

        if (response_buffer.size() > 0) 
        {
            response_body.append(boost::asio::buffer_cast<const char*>(response_buffer.data()),
                response_buffer.size()
            );
            response_buffer.consume(response_buffer.size());
        }

        while (response_body.size() < content_length) 
        {
            std::vector<char> temp_buffer(8192);
            size_t bytes_read = boost::asio::read(
                *ssl_stream,
                boost::asio::buffer(temp_buffer),
                boost::asio::transfer_at_least(1)
            );
            response_body.append(temp_buffer.data(), bytes_read);
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        logLatency(duration);
        return json::parse(response_body);
    } 
    catch (const json::exception& ex)
    {
        spdlog::error("JSON parsing error: {}", ex.what());
    } 
    catch (const std::exception& ex)
    {
        spdlog::error("Request error: {}", ex.what());
    }

    return json();
}

void Client::placeOrder(const std::string& instrument_name, double amount, double price, const std::string& order_type)
{
    json params = {
        {"instrument_name", instrument_name},
        {"amount", amount},
        {"type", order_type}
    };

    if (order_type != "market") 
    {
        params["price"] = price;
    }

    json payloadPlaceOrder = getCachedPayload("/api/v2/private/buy", "private/buy", params);

    try
    {
        json response = sendRequest("/api/v2/private/buy", "POST", payloadPlaceOrder);

        if (!response.is_null() && response.contains("error")) 
        {
            spdlog::error("Order placement failed: {}", response["error"].dump(4));
            throw std::runtime_error("Order placement error");
        }
        storeOrder(response);
        spdlog::info("Order placed successfully: {}", response.dump(4));
    }
    catch (const std::exception& e)
    {
        spdlog::error("PlaceOrder Request error: {}", e.what());
        throw;
    }
}

void Client::storeOrder(const json& orderResponse)
{
    try 
    {
        const auto& order = orderResponse["result"]["order"];
        std::string instrumentName = order["instrument_name"];
        std::string orderId = order["order_id"];

        openOrders.insert({instrumentName, orderId});

        std::ofstream outFile("order_history.json", std::ios::app);
        if (outFile) 
        {
            nlohmann::json orderData = {
                {"instrument_name", instrumentName},
                {"order_id", orderId}
            };
            outFile << orderData.dump() << std::endl;
            outFile.close();
        }

        spdlog::info("Order stored: Instrument={}, Order ID={}", instrumentName, orderId);
    } 
    catch (const std::exception& ex) 
    {
        spdlog::error("Error storing order: {}", ex.what());
    }
}

void Client::loadOrderHistory()
{
    std::ifstream inFile("order_history.json");
    if (!inFile) 
    {
        spdlog::info("No order history file found. Starting fresh.");
        return;
    }

    std::string line;
    while (std::getline(inFile, line)) 
    {
        try 
        {
            nlohmann::json orderData = nlohmann::json::parse(line);
            std::string instrumentName = orderData["instrument_name"];
            std::string orderId = orderData["order_id"];
            openOrders.insert({instrumentName, orderId});
        } 
        catch (const std::exception& ex) 
        {
            spdlog::error("Error loading order history: {}", ex.what());
        }
    }

    inFile.close();
}

void Client::listOpenOrders()
{
    if (openOrders.empty()) 
    {
        spdlog::info("No open orders found.");
        return;
    }

    for (const auto& [instrument, orderId] : openOrders) 
    {
        std::cout << "Instrument: " << instrument << ", Order ID: " << orderId << std::endl;
    }
}


void Client::cancelOrder(const std::string& order_id)
{
    json params = {
        {"order_id", order_id}
    };

    json cancelOrderPayload = getCachedPayload("/api/v2/private/cancel", "private/cancel", params);

    try
    {
        json response = sendRequest("/api/v2/private/cancel", "POST", cancelOrderPayload);
        if (response.contains("result")) 
        {
            spdlog::info("Order Cancled Successfully...");
        } 
        else 
        {
            spdlog::warn("Something weird happens : {}", response.dump(4));
        }
    }
    catch(const std::exception& e)
    {
        spdlog::error("Order Cancel Request error: {}", e.what());
        throw;
    }
}

void Client::modifyOrder(const std::string& order_id, double amount, double price)
{
    json params = {
        {"order_id", order_id},
        {"amount", amount},
        {"price", price}
    };

    json orderModifyPayload = getCachedPayload("/api/v2/private/edit", "private/edit", params);

    try
    {
        json response = sendRequest("/api/v2/private/edit", "POST", orderModifyPayload);
        if (response.contains("result")) 
        {
            spdlog::info("Order modified Successfully...");
        } 
        else 
        {
            spdlog::warn("Something weird happens : {}", response.dump(4));
        }
    }
    catch(const std::exception& e)
    {
        spdlog::error("Order modify Request error: {}", e.what());
    }
}

void Client::getOrderBook(const std::string& instrument_name)
{
    json params = {
        {"instrument_name", instrument_name},
        {"depth", 5}
    };

    json orderModifyPayload = getCachedPayload("/api/v2/private/get_open_orders", "private/get_open_orders", params);

    try
    {
        json response = sendRequest("/api/v2/private/get_open_orders", "POST", orderModifyPayload);
        if (response.contains("result")) 
        {
            spdlog::info("Order Book : {}", response.dump(4));
        } 
        else 
        {
            spdlog::warn("No Order Book found.");
        }
    }
    catch(const std::exception& e)
    {
        spdlog::error("Order Book Request error: {}", e.what());
    }
}

void Client::viewCurrentPositions()
{
    json params = {
    };

    json payload = getCachedPayload("/api/v2/private/get_positions", "private/get_positions", params);

    try
    {
        json response = sendRequest("/api/v2/private/get_positions", "POST", payload);

        if (response.contains("result")) 
        {
            spdlog::info("Open Positions: {}", response["result"].dump(4));
        } 
        else 
        {
            spdlog::warn("No open positions found : {}", response.dump(4));
        }
    }
    catch (const std::exception& e)
    {
        spdlog::error("Error fetching open positions: {}", e.what());
    }
}

void Client::authenticate()
{
    nlohmann::json payload = {
        {"jsonrpc", "2.0"},
        {"id", 0},
        {"method", "public/auth"},
        {"params", {
            {"grant_type", "client_credentials"},
            {"client_id", _clientId},
            {"client_secret", _secreatKey}
        }}
    };

    try
    {
        connect();
        nlohmann::json response = sendRequest("/api/v2/public/auth", "POST ", payload);

        if (response.contains("result") && response["result"].contains("access_token")) 
        {
            std::string accessToken = response["result"]["access_token"];
            setAccessToken(accessToken);
            spdlog::info("Authenticated successfully. Access token: {}", accessToken);
        }

        spdlog::info("Connected succesfully...");
    }
    catch(const std::exception& e)
    {
        spdlog::info("Something goes wrong : {}", e.what());
    }
}

void Client::initWebSocket()
{
    try 
    {
        boost::asio::ip::tcp::resolver resolver(_io_context_ws);
        auto results = resolver.resolve(_host, _port);

        auto& raw_socket = _ws.next_layer().next_layer();
        boost::asio::connect(raw_socket, results);

        _ws.next_layer().handshake(boost::asio::ssl::stream_base::client);

        _ws.set_option(boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::client));
        _ws.handshake(_host, "/ws/api/v2");

        spdlog::info("WebSocket connection established with {}", _host);

        nlohmann::json payload = {
            {"jsonrpc", "2.0"},
            {"id", 0},
            {"method", "public/auth"},
            {"params", {
                {"grant_type", "client_credentials"},
                {"client_id", _clientId},
                {"client_secret", _secreatKey}
            }}
        };

        std::string payload_str = payload.dump();
        _ws.write(boost::asio::buffer(payload_str));
        spdlog::info("Authentication payload sent: {}", payload_str);

        boost::beast::flat_buffer buffer;
        _ws.read(buffer);

        std::string response_str = boost::beast::buffers_to_string(buffer.data());
        nlohmann::json response = nlohmann::json::parse(response_str);

        if (response.contains("result")) 
        {
            spdlog::info("WebSocket authenticated successfully.");
            _wsConnected = true;
        } 
        else 
        {
            spdlog::error("WebSocket authentication failed: {}", response.dump());
            throw std::runtime_error("WebSocket authentication failed");
        }
    } 
    catch (const std::exception& ex) 
    {
        spdlog::error("WebSocket initialization error: {}", ex.what());
        throw;
    }
}

void Client::subscribeToMarketData(const std::string& symbol)
{
    try 
    {
        nlohmann::json subscribePayload = {
            {"jsonrpc", "2.0"},
            {"id", 1},
            {"method", "public/subscribe"},
            {"params", {{"channels", {"book." + symbol + ".raw"}}}}
        };

        _ws.write(boost::asio::buffer(subscribePayload.dump()));
        spdlog::info("Subscribed to symbol: {}", symbol);
    } 
    catch (const std::exception& e) 
    {
        spdlog::error("Subscription error: {}", e.what());
        throw;
    }
}

void Client::streamMarketData(const int &seconds)
{
    try 
    {
        auto startTimestamp = std::chrono::high_resolution_clock::now();

        boost::beast::flat_buffer buffer;
        while (true) 
        {
            _ws.read(buffer);
            std::string message = boost::beast::buffers_to_string(buffer.data());
            spdlog::info("Market Data Received: {}", message);
            buffer.clear();

            auto endTimestamp = std::chrono::high_resolution_clock::now();
            auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(endTimestamp - startTimestamp).count();
            if (elapsed_time >= seconds)
            {
                spdlog::info("Market data streaming completed.");
                break;
            }
        }
    } 
    catch (const std::exception& e) 
    {
        spdlog::error("Market data streaming error: {}", e.what());
    }
}

void Client::addToCache(const std::string& key, const std::string& payload)
{
    if (payload.size() > max_payload_size) 
    {
        spdlog::warn("Payload size exceeds the maximum limit of {} characters. Skipping cache.", max_payload_size);
        return;
    }

    if (payload_cache.find(key) != payload_cache.end()) 
    {
        cache_keys.remove(key);
    } 
    else if (cache_keys.size() >= max_cache_size) 
    {
        std::string lru_key = cache_keys.front();
        cache_keys.pop_front();
        payload_cache.erase(lru_key);
    }

    cache_keys.push_back(key);
    payload_cache[key] = payload;
}

std::string Client::getFromCache(const std::string& key)
{
    if (payload_cache.find(key) != payload_cache.end()) 
    {
        cache_keys.remove(key);
        cache_keys.push_back(key);
        return payload_cache[key];
    }
    return "";
}

nlohmann::json Client::getCachedPayload(const std::string& endpoint, const std::string& method, const nlohmann::json& params)
{
    std::string key = endpoint + ":" + method + ":" + params.dump();

    std::string cached_payload = getFromCache(key);
    if (!cached_payload.empty()) 
    {
        spdlog::info("Using cached payload for key: {}", key);
        return nlohmann::json::parse(cached_payload);
    }

    nlohmann::json payload = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", method},
        {"params", params}
    };

    addToCache(key, payload.dump());
    spdlog::info("Added new payload to cache for key: {}", key);

    return payload;
}
