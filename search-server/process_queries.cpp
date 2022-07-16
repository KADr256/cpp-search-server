#include "process_queries.h"
#include "log_duration.h"

std::vector<Document> ProcessQueriesJoined(const SearchServer& search_server,const std::vector<std::string>& queries) {
	std::vector<Document> result;
	result.reserve(MAX_RESULT_DOCUMENT_COUNT*queries.size());
	for (auto& el1 : ProcessQueries(search_server, queries)) {
		for (auto& el2 : el1) {
			result.push_back(el2);
		}
	}
	return result;
}

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, const std::vector<std::string>& queries) {
	std::vector<std::vector<Document>> result;
	result.resize(queries.size());

	for (auto& el : result) {
		el.reserve(MAX_RESULT_DOCUMENT_COUNT);
	}

	auto Test = [&search_server](const std::string query) {return search_server.FindTopDocuments(query); };

	std::transform(std::execution::par,queries.begin(), queries.end(), result.begin(),Test);
	return result;
}

