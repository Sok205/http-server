//
// Created by Filip Sokołowski on 02/06/2025.
//

#include <database/db.hpp>
#include <iostream>

namespace db
{
    Database::Database() : db_(nullptr) {}

    Database::Database(const std::string &path) : db_(nullptr), dbPath_(path) //NOLINT
    {
        connect(path);
    }

    Database::~Database()
    {
        disconnect();
    }

    bool Database::connect(const std::string &path)
    {
        if (db_ != nullptr)
        {
            disconnect();
        }

        dbPath_ = path;

        if (const int connection = sqlite3_open(path.c_str(), &db_); connection != SQLITE_OK)
        {
            std::cerr << "Can't open database: " << path << '\n'; //https://cplusplus.com/reference/iostream/cerr/
            sqlite3_close(db_);
            db_ = nullptr;
            return false;
        }

        std::cout << "Connected to database: " << path << '\n';
        return true;
    }

    void Database::disconnect()
    {
        if (db_ != nullptr)
        {
            sqlite3_close(db_);
            db_ = nullptr;
            std::cout << "Database disconnected\n" << '\n';
        }
    }

    bool Database::isConnected() const
    {
        return db_ != nullptr;
    }

    bool Database::createTable(const std::string& query)
    {
        if (!isConnected())
        {
            std::cerr << "Database not connected\n" << '\n';
            return false;
        }

    }
}//namespace db