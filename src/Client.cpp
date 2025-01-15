#include "Client.hpp"
#include <chrono>
#include <boost/asio/connect.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/write.hpp>

using json = nlohmann::json;

Client::Client(const std::string& host, const std::string& port, const std::string& clientId, const std::string& secreatKey)
    : _ssl_context(boost::asio::ssl::context::sslv23), _host(host), _port(port), _clientId(clientId), _secreatKey(secreatKey)
{
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
        spdlog::info("Connected to {}:{}", _host, _port);
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
        spdlog::info("Client resources cleaned up.");
    } 
    catch (const std::exception& ex) 
    {
        spdlog::error("Error during resource cleanup: {}", ex.what());
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
    auto start = std::chrono::high_resolution_clock::now();
    try 
    {
        std::string serialized_payload = payload.dump();

        std::ostringstream request_stream;
        request_stream << method << " " << endpoint << " HTTP/1.1\r\n"
                    << "Host: " << _host << "\r\n";

        if (!_accessToken.empty()) 
        {
            request_stream << "Authorization: Bearer " << _accessToken << "\r\n";
            spdlog::info("Access token has been applied.");
        }

        request_stream << "Content-Type: application/json\r\n"
                    << "Content-Length: " << serialized_payload.size() << "\r\n"
                    << "Connection: keep-alive\r\n\r\n"
                    << serialized_payload;


        boost::asio::write(*ssl_stream, boost::asio::buffer(request_stream.str()));

        boost::asio::streambuf response_buffer;

        try
		{
    		boost::asio::read_until(*ssl_stream, response_buffer, "\r\n\r\n");
		}
		catch (const boost::system::system_error& e)
		{
    		if (e.code() == boost::asio::error::eof)
    		{
        		spdlog::error("Connection closed by server before reading response: {}", e.what());
    		} 
    		else 
    		{
        		spdlog::error("Error while reading response: {}", e.what());
    		}
    		throw;
		}

        std::istream response_stream(&response_buffer);
        std::string http_version;
        unsigned int status_code;
        response_stream >> http_version >> status_code;

        if (http_version.substr(0, 5) != "HTTP/") 
        {
            throw std::runtime_error("Invalid HTTP response");
        }

        if (status_code != 200) 
        {
            throw std::runtime_error("Request failed with status code " + std::to_string(status_code));
        }

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
        if (response_buffer.size() > 0) 
        {
            std::istreambuf_iterator<char> it(&response_buffer), end;
            response_body.assign(it, end);
        }

        while (response_body.size() < content_length) 
        {
            boost::asio::read(*ssl_stream, response_buffer, boost::asio::transfer_at_least(1));
            std::istreambuf_iterator<char> it(&response_buffer), end;
            response_body.append(it, end);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        spdlog::info("Program latency: {} seconds", duration.count());
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
    json payloadPlaceOrder = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "private/buy"},
        {"params", {
            {"instrument_name", instrument_name},
            {"amount", amount},
            {"type", order_type}
        }}
    };

    if (order_type != "market") {
        payloadPlaceOrder["params"]["price"] = price;
    }

    try
    {
        json response = sendRequest("/api/v2/private/buy", "POST", payloadPlaceOrder);

        if (!response.is_null() && response.contains("error")) 
        {
            spdlog::error("Order placement failed: {}", response["error"].dump(4));
            throw std::runtime_error("Order placement error");
        }

        spdlog::info("Order placed successfully: {}", response.dump(4));
    }
    catch (const std::exception& e)
    {
        spdlog::error("PlaceOrder Request error: {}", e.what());
        throw;
    }
}

void Client::cancelOrder(const std::string& order_id)
{
    json cancelOrderPayload = {
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "private/cancel"},
        {"params", {
            {"order_id", order_id}
        }}
    };

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
    json orderModifyPayload = {
        {"jsonrpc", "2.0"},
        {"id", 4},
        {"method", "private/edit"},
        {"params", {
            {"order_id", order_id},
            {"amount", amount},
            {"price", price}
        }}
    };

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
    json orderModifyPayload = {
        {"jsonrpc", "2.0"},
        {"id", 4},
        {"method", "public/get_open_orders"},
        {"params", {
            {"instrument_name", instrument_name}
        }}
    };

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
     json payload = {
        {"jsonrpc", "2.0"},
        {"id", 5},
        {"method", "private/get_positions"},
        {"params", {}}
    };

    try
    {
        json response = sendRequest("/api/v2/private/get_positions", "POST", payload);

        if (response.contains("result")) 
        {
            spdlog::info("Open Positions: {}", response["result"].dump(4));
        } 
        else 
        {
            spdlog::warn("No open positions found.");
        }
    }
    catch (const std::exception& e)
    {
        spdlog::error("Error fetching open positions: {}", e.what());
    }
}