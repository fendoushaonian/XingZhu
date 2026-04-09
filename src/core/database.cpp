#include "core/database.h"
#include "utils/logger.h"

#include <mysql.h>
#include <memory>

namespace sf {

static MYSQL* g_mysql = nullptr;

bool InitDatabase(const DbConfig& cfg) {
    if (g_mysql) return true;

    g_mysql = mysql_init(nullptr);
    if (!g_mysql) {
        Log(LogLevel::Error, "mysql_init failed");
        return false;
    }

    // Set UTF-8 before connect
    mysql_options(g_mysql, MYSQL_SET_CHARSET_NAME, "utf8mb4");

    // Auto-reconnect
    bool reconnect = true;
    mysql_options(g_mysql, MYSQL_OPT_RECONNECT, &reconnect);

    // Short connect timeout to avoid blocking the UI on startup
    unsigned int connectTimeout = 2;
    mysql_options(g_mysql, MYSQL_OPT_CONNECT_TIMEOUT, &connectTimeout);

    if (!mysql_real_connect(g_mysql, cfg.host.c_str(), cfg.user.c_str(),
                            cfg.pass.c_str(), cfg.db.c_str(), cfg.port,
                            nullptr, 0)) {
        Log(LogLevel::Error, "MySQL connect failed: %s", mysql_error(g_mysql));
        mysql_close(g_mysql);
        g_mysql = nullptr;
        return false;
    }

    mysql_set_character_set(g_mysql, "utf8mb4");

    Log(LogLevel::Info, "MySQL connected: %s@%s:%d/%s",
        cfg.user.c_str(), cfg.host.c_str(), cfg.port, cfg.db.c_str());

    // Auto-migrate: add github_id column if not exists
    mysql_query(g_mysql, "ALTER TABLE users ADD COLUMN github_id VARCHAR(64) DEFAULT NULL");
    // Ignore error if column already exists

    // Auto-migrate: add check-in related columns if not exists
    mysql_query(g_mysql, "ALTER TABLE users ADD COLUMN last_checkin DATE DEFAULT NULL");
    mysql_query(g_mysql, "ALTER TABLE users ADD COLUMN consecutive_days INT DEFAULT 0");
    mysql_query(g_mysql, "ALTER TABLE users ADD COLUMN monthly_checkins INT DEFAULT 0");
    mysql_query(g_mysql, "ALTER TABLE users ADD COLUMN coins INT DEFAULT 0");
    // Ignore errors if columns already exist

    return true;
}

void ShutdownDatabase() {
    if (g_mysql) {
        mysql_close(g_mysql);
        g_mysql = nullptr;
    }
}

MYSQL* GetDB() {
    return g_mysql;
}

std::string DbEscape(const std::string& input) {
    if (!g_mysql) return input;
    std::string out(input.size() * 2 + 1, '\0');
    unsigned long len = mysql_real_escape_string(g_mysql, &out[0], input.c_str(), (unsigned long)input.size());
    out.resize(len);
    return out;
}

bool DbExec(const char* sql) {
    if (!g_mysql) return false;
    if (mysql_query(g_mysql, sql) != 0) {
        Log(LogLevel::Error, "SQL exec error: %s\n  SQL: %.200s", mysql_error(g_mysql), sql);
        return false;
    }
    return true;
}

bool DbExecParam(const char* sql, const std::vector<std::string>& params) {
    if (!g_mysql) {
        Log(LogLevel::Error, "DbExecParam: g_mysql is null");
        return false;
    }

    // Ensure connection is alive (reconnects if MYSQL_OPT_RECONNECT is set)
    if (mysql_ping(g_mysql) != 0) {
        Log(LogLevel::Error, "DbExecParam: mysql_ping failed: %s", mysql_error(g_mysql));
        return false;
    }

    MYSQL_STMT* stmt = mysql_stmt_init(g_mysql);
    if (!stmt) {
        Log(LogLevel::Error, "DbExecParam: mysql_stmt_init failed: %s", mysql_error(g_mysql));
        return false;
    }

    if (mysql_stmt_prepare(stmt, sql, (unsigned long)strlen(sql)) != 0) {
        Log(LogLevel::Error, "SQL prepare error: %s", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    std::vector<MYSQL_BIND> binds(params.size());
    std::vector<unsigned long> lengths(params.size());
    memset(binds.data(), 0, sizeof(MYSQL_BIND) * binds.size());

    for (size_t i = 0; i < params.size(); i++) {
        lengths[i] = (unsigned long)params[i].size();
        binds[i].buffer_type = MYSQL_TYPE_STRING;
        binds[i].buffer = (void*)params[i].c_str();
        binds[i].buffer_length = lengths[i];
        binds[i].length = &lengths[i];
    }

    if (!binds.empty())
        mysql_stmt_bind_param(stmt, binds.data());

    bool ok = (mysql_stmt_execute(stmt) == 0);
    if (!ok) {
        Log(LogLevel::Error, "SQL exec error: %s", mysql_stmt_error(stmt));
    }
    mysql_stmt_close(stmt);
    return ok;
}

bool DbQuery(const char* sql,
             std::function<void(int cols, char** values, char** names)> rowCb) {
    if (!g_mysql) return false;

    if (mysql_query(g_mysql, sql) != 0) {
        Log(LogLevel::Error, "SQL query error: %s", mysql_error(g_mysql));
        return false;
    }

    MYSQL_RES* res = mysql_store_result(g_mysql);
    if (!res) return true; // no result set (e.g. UPDATE)

    int numCols = mysql_num_fields(res);
    MYSQL_FIELD* fields = mysql_fetch_fields(res);

    // Build column names array
    std::vector<char*> colNames(numCols);
    for (int i = 0; i < numCols; i++)
        colNames[i] = fields[i].name;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != nullptr) {
        rowCb(numCols, row, colNames.data());
    }

    mysql_free_result(res);
    return true;
}

bool DbQueryParam(const char* sql, const std::vector<std::string>& params,
                  std::function<void(int cols, char** values, char** names)> rowCb) {
    if (!g_mysql) return false;

    if (mysql_ping(g_mysql) != 0) {
        Log(LogLevel::Error, "DbQueryParam: mysql_ping failed: %s", mysql_error(g_mysql));
        return false;
    }

    MYSQL_STMT* stmt = mysql_stmt_init(g_mysql);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, sql, (unsigned long)strlen(sql)) != 0) {
        Log(LogLevel::Error, "SQL prepare error: %s", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    // Bind input params
    std::vector<MYSQL_BIND> inBinds(params.size());
    std::vector<unsigned long> lengths(params.size());
    memset(inBinds.data(), 0, sizeof(MYSQL_BIND) * inBinds.size());

    for (size_t i = 0; i < params.size(); i++) {
        lengths[i] = (unsigned long)params[i].size();
        inBinds[i].buffer_type = MYSQL_TYPE_STRING;
        inBinds[i].buffer = (void*)params[i].c_str();
        inBinds[i].buffer_length = lengths[i];
        inBinds[i].length = &lengths[i];
    }

    if (!inBinds.empty())
        mysql_stmt_bind_param(stmt, inBinds.data());

    if (mysql_stmt_execute(stmt) != 0) {
        Log(LogLevel::Error, "SQL exec error: %s", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    // Get result metadata
    MYSQL_RES* meta = mysql_stmt_result_metadata(stmt);
    if (!meta) {
        mysql_stmt_close(stmt);
        return true; // no result set
    }

    int numCols = mysql_num_fields(meta);
    MYSQL_FIELD* fields = mysql_fetch_fields(meta);

    std::vector<char*> colNames(numCols);
    for (int i = 0; i < numCols; i++)
        colNames[i] = fields[i].name;

    // Bind output columns
    std::vector<MYSQL_BIND> outBinds(numCols);
    std::vector<std::vector<char>> buffers(numCols);
    std::vector<unsigned long> outLengths(numCols);
    std::unique_ptr<bool[]> nullFlags(new bool[numCols]());

    memset(outBinds.data(), 0, sizeof(MYSQL_BIND) * numCols);

    for (int i = 0; i < numCols; i++) {
        buffers[i].resize(1024, 0);
        outBinds[i].buffer_type = MYSQL_TYPE_STRING;
        outBinds[i].buffer = buffers[i].data();
        outBinds[i].buffer_length = (unsigned long)buffers[i].size();
        outBinds[i].length = &outLengths[i];
        outBinds[i].is_null = &nullFlags[i];
    }

    mysql_stmt_bind_result(stmt, outBinds.data());
    mysql_stmt_store_result(stmt);

    std::vector<char*> rowValues(numCols);
    while (mysql_stmt_fetch(stmt) == 0) {
        for (int i = 0; i < numCols; i++) {
            if (nullFlags[i])
                rowValues[i] = nullptr;
            else {
                buffers[i][outLengths[i]] = '\0';
                rowValues[i] = buffers[i].data();
            }
        }
        rowCb(numCols, rowValues.data(), colNames.data());
    }

    mysql_free_result(meta);
    mysql_stmt_close(stmt);
    return true;
}

} // namespace sf
