#include "request_queue.h"

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status) {
    return AddFindRequest(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
         });
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query) {
    return AddFindRequest(raw_query, DocumentStatus::ACTUAL);
}

int RequestQueue::GetNoResultRequests() const {
return no_result_count_;
}

void RequestQueue::UpdateQuery() {
    if (requests_.size() + 1 > 1440) {
        if (requests_.front().no_result) {
            no_result_count_--;
        }
        requests_.pop_front();
    }
return;
}