#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <cpprest/filestream.h>
#include <cpprest/uri.h>
#include <cpprest/asyncrt_utils.h>
#include <iostream>
#include <string>
#include <memory>
#include "db_manager.h"
#include "sentinel_processor.h"
#include "model_inference.h"

using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;

class RouterServer {
private:
    http_listener listener;
    std::unique_ptr<DBManager> db_manager;
    std::unique_ptr<SentinelProcessor> sentinel_processor;
    std::unique_ptr<ModelInference> model_inference;

    void handle_get_image(http_request request) {
        auto query = uri::split_query(request.request_uri().query());
        std::string image_id = query[U("id")];
        
        try {
            auto image_data = db_manager->get_image(image_id);
            if (image_data.empty()) {
                request.reply(status_codes::NotFound, U("Image not found"));
                return;
            }
            
            // Получаем спектры из Sentinel-2 архива
            auto spectrums = sentinel_processor->get_spectrums(image_id);
            
            // Создаем бинарную маску
            auto mask = model_inference->create_mask(spectrums);
            
            // Отправляем результат
            json::value response;
            response[U("image_id")] = json::value::string(U(image_id));
            response[U("mask")] = json::value::string(U(mask));
            
            request.reply(status_codes::OK, response);
        }
        catch (const std::exception& e) {
            request.reply(status_codes::InternalError, U("Error processing request"));
        }
    }

    void handle_put_image(http_request request) {
        try {
            auto body = request.extract_json().get();
            std::string image_id = body[U("image_id")].as_string();
            std::string image_data = body[U("image_data")].as_string();
            
            bool success = db_manager->put_image(image_id, image_data);
            if (success) {
                request.reply(status_codes::OK, U("Image stored successfully"));
            } else {
                request.reply(status_codes::InternalError, U("Failed to store image"));
            }
        }
        catch (const std::exception& e) {
            request.reply(status_codes::InternalError, U("Error processing request"));
        }
    }

    void handle_gossip(http_request request) {
        try {
            auto body = request.extract_json().get();
            // Обработка gossip сообщений
            request.reply(status_codes::OK, U("Gossip message received"));
        }
        catch (const std::exception& e) {
            request.reply(status_codes::InternalError, U("Error processing gossip message"));
        }
    }

public:
    RouterServer(const std::string& url) : listener(url) {
        db_manager = std::make_unique<DBManager>();
        sentinel_processor = std::make_unique<SentinelProcessor>();
        model_inference = std::make_unique<ModelInference>("best_unet_resnet34.pth");

        listener.support(methods::GET, std::bind(&RouterServer::handle_get_image, this, std::placeholders::_1));
        listener.support(methods::PUT, std::bind(&RouterServer::handle_put_image, this, std::placeholders::_1));
        listener.support(methods::POST, std::bind(&RouterServer::handle_gossip, this, std::placeholders::_1));
    }

    void start() {
        try {
            listener.open().wait();
            std::cout << "Router server started at: " << listener.uri().to_string() << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "Error starting server: " << e.what() << std::endl;
        }
    }

    void stop() {
        listener.close().wait();
    }
};

int main() {
    RouterServer server("http://localhost:8080");
    server.start();
    
    std::cout << "Press ENTER to exit." << std::endl;
    std::string line;
    std::getline(std::cin, line);
    
    server.stop();
    return 0;
} 