#pragma once
#include "search_server.h"
#include <string>
#include <vector>

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server) :search_server_(search_server) {
    }

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        std::vector<Document> result = search_server_.FindTopDocuments(raw_query, document_predicate);

        if (result.empty()) {
            UpdateQuery();
            no_result_count_++;
            requests_.push_back({ true });
        }
        else {
            UpdateQuery();
            requests_.push_back({ false });
        }
        return result;
    }

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;
private:
    struct QueryResult {
        bool no_result;
    };
    int no_result_count_ = 0;
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    const SearchServer& search_server_;

    void UpdateQuery();
};