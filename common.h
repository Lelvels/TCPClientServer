#pragma once
#include<nlohmann/json.hpp>
#include<string>
#include<map>
#include<algorithm>

using json = nlohmann::json;

#define RESP_OK 200
#define RESP_REGISTRATION_FAILED 100
#define RESP_REGISTRATION_OK 101
#define RESP_NOTFOUND 404
#define RESP_INTERNAL_SERVER_ERROR 500

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
        unsigned int resp_code;
        std::string message;
        unsigned int id;
    };
    struct notification
    {
        std::string device_name;
        std::string dest;
        std::map<std::string, std::string> notifications;
    };

    std::string trim(const std::string &s)
    {
        auto start = s.begin();
        while (start != s.end() && std::isspace(*start)) {
            start++;
        }
    
        auto end = s.end();
        do {
            end--;
        } while (std::distance(start, end) > 0 && std::isspace(*end));
    
        return std::string(start, end + 1);
    }

    std::string serializeRequest(const request& req){
        json j = json{
            {"device_name", trim(req.device_name)},
            {"destination", req.dest},
            {"method", req.req_method},
            {"params", req.params},
            {"id", req.id}
        };
        return j.dump();
    }

    request deserializeRequest(const char *buffer){
        request req;
        json jsonReq = json::parse(buffer);
        req.device_name = jsonReq.at("device_name");
        req.dest = jsonReq.at("destination");
        req.req_method = jsonReq.at("method");
        const json& rh = jsonReq["params"];
        for (auto& element : json::iterator_wrapper(rh)) {
            std::cout << element.key() << " maps to " << element.value() << std::endl;
        }
        req.id = jsonReq.at("id");
        return req;
    }

    std::string serializeResponse(const response& resp){
        json j = json{
            {"device_name", resp.device_name},
            {"resp_code", resp.resp_code},
            {"message", resp.message},
            {"id", resp.id}
        };
        return j.dump();
    }

    response deserializeResponse(const char* buffer){
        response resp;
        json jsonResp = json::parse(buffer);
        resp.device_name = jsonResp.at("device_name");
        resp.resp_code = jsonResp.at("resp_code");
        resp.message = jsonResp.at("message");
        resp.id = jsonResp.at("id");
        return resp;
    }
}
