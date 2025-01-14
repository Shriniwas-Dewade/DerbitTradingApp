#include "Client.hpp"
#include <boost/asio/connect.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/write.hpp>

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

nlohmann::json Client::sendRequest(const std::string& endpoint, const std::string& method, const nlohmann::json& payload) 
{

    if (ssl_stream->lowest_layer().is_open()) 
    {
        spdlog::info("Reusing existing connection to {}:{}", _host, _port);
        return "";
    }

    try 
    {
        std::string serialized_payload = payload.dump();

        std::ostringstream request_stream;
        request_stream << method << " " << endpoint << " HTTP/1.1\r\n"
                       << "Host: " << _host << "\r\n"
                       << "Content-Type: application/json\r\n"
                       << "Content-Length: " << serialized_payload.size() << "\r\n"
                       << "Connection: keep-alive\r\n\r\n"
                       << serialized_payload;

        boost::asio::write(*ssl_stream, boost::asio::buffer(request_stream.str()));

        boost::asio::streambuf response_buffer;
        boost::asio::read_until(*ssl_stream, response_buffer, "\r\n\r\n");

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

        return nlohmann::json::parse(response_body);
    } 
    catch (const nlohmann::json::exception& ex)
    {
        spdlog::error("JSON parsing error: {}", ex.what());
        throw;
    } 
    catch (const std::exception& ex)
    {
        spdlog::error("Request error: {}", ex.what());
        throw;
    }
}
