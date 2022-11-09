#include <nlohmann/json.hpp>

#include "rpc.hpp"

namespace atomic_dex::mm2
{
    void from_json(const nlohmann::json& j, rpc_basic_error_type& in)
    {
        SPDLOG_INFO("RPC Error JSON: {}", j.dump(4));
        j.at("error").get_to(in.error);
        j.at("error_path").get_to(in.error_path);
        j.at("error_trace").get_to(in.error_trace);
        j.at("error_type").get_to(in.error_type);
        j.at("error_data").get_to(in.error_data);
    }
}