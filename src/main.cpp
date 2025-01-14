#include "Client.hpp"
#include <chrono>
#include "spdlog/sinks/basic_file_sink.h"

void setup_logging() 
{
    try 
    {
        auto file_logger = spdlog::basic_logger_mt("file_logger", "../logs/derbit_trading.log", true);
        spdlog::register_logger(file_logger);
        spdlog::set_default_logger(file_logger);

        spdlog::set_level(spdlog::level::debug);
        spdlog::info("Logging setup complete.");
    } 
    catch (const spdlog::spdlog_ex& ex) 
    {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
    }
}

void printMenu() 
{
    std::cout << "1. Connect to Deribit\n";
    std::cout << "2. Place Order\n";
    std::cout << "3. Cancel Order\n";
    std::cout << "4. Modify Order\n";
    std::cout << "5. Exit\n";
    std::cout << "Enter your choice: ";
}

int main() 
{
    std::ios_base::sync_with_stdio(false);

    auto start = std::chrono::high_resolution_clock::now();
    int choice;

    std::string clientId = "cyL_105V";
    std::string clientSecret = "QYDbVHNoHm_6glxbgasvdCnBh3yIr1eQJKelNAi2Ejk";

    spdlog::set_level(spdlog::level::info);
    setup_logging();

    nlohmann::json payload = {
        {"jsonrpc", "2.0"},
        {"id", 0},
        {"method", "public/auth"},
        {"params", {
            {"grant_type", "client_credentials"},
            {"scope", "session:api:console=c5126dsd6sdr expires:2592000"},
            {"client_id", clientId},
            {"client_secret", clientSecret}
        }}
    };

    Client client("test.deribit.com", "443", clientId, clientSecret);

    while (true) 
    {
        printMenu();
        std::cin >> choice;
        switch (choice) 
        {
            case 1:
            {
                auto start = std::chrono::high_resolution_clock::now();
                try 
                {   
                    client.connect();
                    nlohmann::json response = client.sendRequest("/api/v2/public/auth", "POST ", payload);
                    //spdlog::info("Response: {}", response.dump(4));
                    spdlog::info("Connected succesfully...");
                } 
                catch (const std::exception& ex) 
                {
                    spdlog::error("Application error: {}", ex.what());
                }
                auto end = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> duration = end - start;
                spdlog::info("Program latency: {} seconds", duration.count());
                break;
            }
            case 2: 
            {
                auto start = std::chrono::high_resolution_clock::now();
                std::string instrument;
                double amount, price;
                std::string order_type;
                std::cout << "Enter instrument name, amount, price, and order type: ";
                std::cin >> instrument >> amount >> price >> order_type;
                //client.placeOrder(instrument, amount, price, order_type);
                auto end = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> duration = end - start;
                spdlog::info("Program latency: {} seconds", duration.count());
                break;
            }
            case 3: 
            {
                auto start = std::chrono::high_resolution_clock::now();
                std::string order_id;
                std::cout << "Enter order ID to cancel: ";
                std::cin >> order_id;
                auto end = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> duration = end - start;
                spdlog::info("Program latency: {} seconds", duration.count());
                //client.cancelOrder(order_id);
                break;
            }
            case 4: 
            {
                auto start = std::chrono::high_resolution_clock::now();
                std::string order_id;
                double amount, price;
                std::cout << "Enter order ID, new amount, and new price to modify order: ";
                std::cin >> order_id >> amount >> price;
                //client.modifyOrder(order_id, amount, price);
                auto end = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> duration = end - start;
                spdlog::info("Program latency: {} seconds", duration.count());
                break;
            }
            case 5:
            {
                return 0;
            }
            default:
                std::cout << "Invalid choice, please try again.\n";
        }
    }

    return 0;
}

