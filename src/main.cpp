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

void printMenu() 
{
    std::cout << "\n1. Place Order\n";
    std::cout << "2. Cancel Order\n";
    std::cout << "3. Modify Order\n";
    std::cout << "4. Get Order Book\n";
    std::cout << "5. View current positions\n";
    std::cout << "6. Exit.\n";
    std::cout << "Enter your choice: ";
}

int main() 
{
    std::ios_base::sync_with_stdio(false);

    auto start = std::chrono::high_resolution_clock::now();
    int choice;

    std::string clientId = "cyL_105V";
    std::string clientSecret = "QYDbVHNoHm_6glxbgasvdCnBh3yIr1eQJKelNAi2Ejk";

    setup_logging();

    nlohmann::json payload = {
        {"jsonrpc", "2.0"},
        {"id", 0},
        {"method", "public/auth"},
        {"params", {
            {"grant_type", "client_credentials"},
            {"client_id", clientId},
            {"client_secret", clientSecret}
        }}
    };

    Client client("test.deribit.com", "443", clientId, clientSecret);

    try
    {
        client.connect();
        nlohmann::json response = client.sendRequest("/api/v2/public/auth", "POST ", payload);

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
        printMenu();
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
                return 0;
            }
            default:
                std::cout << "Invalid choice, please try again.\n";
        }
    }

    return 0;
}

