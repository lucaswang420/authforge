#pragma once

#include <string>
#include <vector>
#include <map>
#include <json/json.h>

namespace common::documentation
{

// Parameter types supported by OpenAPI
enum class ParameterType
{
    STRING,
    INTEGER,
    NUMBER,
    BOOLEAN,
    ARRAY,
    OBJECT
};

// Parameter locations
enum class ParameterLocation
{
    QUERY,
    HEADER,
    PATH,
    COOKIE
};

// Enhanced parameter information
struct ParameterInfo
{
    std::string name;
    std::string description;
    ParameterType type = ParameterType::STRING;
    ParameterLocation location = ParameterLocation::QUERY;
    bool required = true;
    std::string defaultValue;  // Optional default value
    std::string enumValues;    // Comma-separated enum values (optional)
    std::string format;  // OpenAPI format (e.g., "int64", "email", "uuid")
};

struct EndpointInfo
{
    std::string path;
    std::string method;
    std::string summary;
    std::string description;
    std::vector<std::string> tags;
    std::vector<ParameterInfo> parameters;  // Changed to use ParameterInfo
    std::map<int, std::string> responses;
    std::map<int, Json::Value> responseExamples;  // NEW: Response examples
    bool requiresAuth;
};

class OpenApiGenerator
{
  public:
    static void addEndpoint(const EndpointInfo &endpoint);
    static Json::Value generateOpenApiSpec();
    static bool writeToFile(const std::string &outputPath);
    static void setApiInfo(const std::string &title,
                           const std::string &version,
                           const std::string &description);

    // Helper function to convert ParameterType to string
    static std::string parameterTypeToString(ParameterType type);

    // Helper function to convert ParameterLocation to string
    static std::string parameterLocationToString(ParameterLocation location);

  private:
    static std::vector<EndpointInfo> endpoints_;
    static Json::Value apiInfo_;
    static bool initialized_;

    static Json::Value generatePathItem(const EndpointInfo &endpoint);
    static Json::Value generateParameter(const ParameterInfo &param);
    static Json::Value generateSchema();
};

}  // namespace common::documentation
