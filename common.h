#pragma once
#include<nlohmann/json.hpp>
#include<string>
#include<map>

using json = nlohmann::json;

namespace message {
    struct request
    {
        std::string device_name;
        std::string dest;
        std::string req_method;
        std::map<std::string, std::string> params;
        unsigned int id;
    };
    struct response
    {
        std::string device_name;
        std::string resp_code;
        std::string message;
    };
    struct notification
    {
        std::string device_name;
        std::string dest;
        std::map<std::string, std::string> notifications;
    };

    void serializeRequest(json& j, const request& req){
        j = json{
            {"device_name", req.device_name},
            {"destination", req.dest},
            {"method", req.req_method},
            {"params", req.params},
            {"id", req.id}
        };
    }
}
