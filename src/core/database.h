#pragma once
#include <string>
#include <vector>
#include <functional>

// Forward declare MySQL types
struct MYSQL;
struct MYSQL_RES;

namespace sf {

struct DbConfig {
    std::string host = "127.0.0.1";
    int         port = 3306;
    std::string user = "root";
    std::string pass = "123456";
    std::string db   = "xz";
};

// Initialize / shutdown MySQL connection
bool InitDatabase(const DbConfig& cfg = {});
void ShutdownDatabase();

// Get raw MYSQL handle
MYSQL* GetDB();

// Execute SQL (INSERT / UPDATE / DELETE)
bool DbExec(const char* sql);

// Parameterized execute (prevents SQL injection)
bool DbExecParam(const char* sql, const std::vector<std::string>& params);

// Escape a string for safe use in SQL (uses mysql_real_escape_string)
std::string DbEscape(const std::string& input);

// Query with callback per row (column count, column values, column names)
bool DbQuery(const char* sql,
             std::function<void(int cols, char** values, char** names)> rowCb);

// Parameterized query
bool DbQueryParam(const char* sql, const std::vector<std::string>& params,
                  std::function<void(int cols, char** values, char** names)> rowCb);

} // namespace sf
