#pragma once
#include<nlohmann/json.hpp>
#include<string>
#include<map>
#include<algorithm>

using json = nlohmann::json;

#define RESP_OK 200
#define RESP_REGISTRATION_FAILED 401
#define RESP_REGISTRATION_OK 201
#define RESP_NOTFOUND 404
#define RESP_INTERNAL_SERVER_ERROR 500
#define RESP_BAD_METHOD 400

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
        std::string from;
        std::string to;
        std::map<std::string, std::string> message;
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
            req.params.insert(std::pair<std::string, std::string>(element.key(), element.value()));
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

    std::string serializeNotification(const notification& notification){
        json j = json{
            {"from", notification.from},
            {"to", notification.to},
            {"message", notification.message}
        };
        return j.dump();
    }

    notification deserializeNotification(const char* buffer){
        notification noti;
        json jsonNoti = json::parse(buffer);
        noti.from = jsonNoti.at("from");
        noti.to = jsonNoti.at("to");
        const json& rh = jsonNoti["message"];
        for (auto& element : json::iterator_wrapper(rh)) {
            noti.message.insert(std::pair<std::string, std::string>(element.key(), element.value()));
        }
        return noti;
    }
}
