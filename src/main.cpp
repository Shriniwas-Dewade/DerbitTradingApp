#include "Client.hpp"
#include "spdlog/sinks/basic_file_sink.h"

void setup_logging() 
{
    try 
    {
        auto file_logger = spdlog::basic_logger_mt("file_logger", "../logs/derbit_trading.log", true);
        spdlog::register_logger(file_logger);
        spdlog::set_default_logger(file_logger);
        spdlog::set_level(spdlog::level::info);
        spdlog::info("Logging setup complete.");
    } 
    catch (const spdlog::spdlog_ex& ex) 
    {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
    }
}

constexpr const char* clientId = "cyL_105V";
constexpr const char* clientSecret = "QYDbVHNoHm_6glxbgasvdCnBh3yIr1eQJKelNAi2Ejk";

const nlohmann::json Client::payload = {
    {"jsonrpc", "2.0"},
    {"id", 0},
    {"method", "public/auth"},
    {"params", {
        {"grant_type", "client_credentials"},
        {"client_id", clientId},
        {"client_secret", clientSecret}
    }}
};

int main() 
{
    std::ios_base::sync_with_stdio(false);

    setup_logging();

    auto start = std::chrono::high_resolution_clock::now();
    int choice;

    Client client("test.deribit.com", "443", clientId, clientSecret);

    try
    {
        client.connect();
        nlohmann::json response = client.sendRequest("/api/v2/public/auth", "POST ", client.payload);

        if (response.contains("result") && response["result"].contains("access_token")) 
        {
            std::string accessToken = response["result"]["access_token"];
            client.setAccessToken(accessToken);
            spdlog::info("Authenticated successfully. Access token: {}", accessToken);
        }

        spdlog::info("Connected succesfully...");
    }
    catch(const std::exception& e)
    {
        spdlog::info("Something goes wrong : {}", e.what());
    }

    while (true) 
    {
        client.printMenu();
        std::cin >> choice;
        switch (choice) 
        {
            case 1: 
            {
                std::string instrument;
                double amount, price;
                std::string order_type;
                std::cout << "Enter instrument name, amount, price, and order type: ";
                std::cin >> instrument >> amount >> price >> order_type;
                client.placeOrder(instrument, amount, price, order_type);
                break;
            }
            case 2: 
            {
                std::string order_id;
                std::cout << "Enter order ID to cancel: ";
                std::cin >> order_id;
                client.cancelOrder(order_id);
                break;
            }
            case 3: 
            {
                std::string order_id;
                double amount, price;
                std::cout << "Enter order ID, new amount, and new price to modify order: ";
                std::cin >> order_id >> amount >> price;
                client.modifyOrder(order_id, amount, price);
                break;
            }
            case 4:
            {
                std::string instrument_name;
                std::cout<<"Enter Instrument Name : ";
                std::cin >> instrument_name;
                client.getOrderBook(instrument_name);
                break;
            }
            case 5:
            {
                client.viewCurrentPositions();
                break;
            }
            case 6: 
            {
                std::string symbol;
                std::cout << "Enter symbol for subscription (e.g., BTC-PERPETUAL): ";
                std::cin >> symbol;

                try 
                {
                    client.initWebSocket();
                    client.subscribeToMarketData(symbol);
                    client.streamMarketData();
                } 
                catch (const std::exception& e) 
                {
                    spdlog::error("WebSocket streaming error: {}", e.what());
                }
                break;
            }
            case 7:
            {
                return 0;
            }
            default:
                std::cout << "Invalid choice, please try again.\n";
        }
    }

    return 0;
}

