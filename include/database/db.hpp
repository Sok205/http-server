//
// Created by Filip Sokołowski on 02/06/2025.
//
#pragma once
#include <sqlite3.h>
#include <thread>


namespace db
{
    class Database
    {
        private:
            sqlite3* db_;
            std::string dbPath_;

        public:
            Database();
            explicit Database(const std::string& dbPath);
            ~Database();

            Database(const Database&) = delete;
            Database& operator=(const Database&) = delete;
            Database(Database&&) = delete;
            Database& operator=(Database&&) = delete;

            bool connect(const std::string& dbPath);
            void disconnect();
            bool isConnected() const;

            bool createTable(const std::string& query);
            bool executeQuery(const std::string& query);

            bool insert(const std::string& table, const std::vector<std::pair<std::string,std::string>>& data);
            std::vector<std::vector<std::string>> select(const std::string& query);
            bool update(const std::string& query);
            bool rowDelete(const std::string& query);
    };
}//namespace db