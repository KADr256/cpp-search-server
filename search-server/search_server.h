#pragma once
#include "document.h"
#include "string_processing.h"
#include "log_duration.h"
#include "concurrent_map.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <queue>
#include <execution>
#include <iterator>
#include <future>

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double RELEVANCE_ROUNDING = 1e-6;
const size_t CONCURRENT_MAP_PARTS = 10;

class SearchServer {
public:
	explicit SearchServer(const std::string& stop_words_text);

	explicit SearchServer(std::string_view stop_words_text);

	template <typename StringContainer>
	explicit SearchServer(const StringContainer& stop_words);

	void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

	template <typename DocumentPredicate>
	std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;

	std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;

	std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

	template <typename DocumentPredicate, class ExecutionPolicy>
	std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, DocumentPredicate document_predicate) const;

	template<class ExecutionPolicy>
	std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, DocumentStatus status) const;

	template<class ExecutionPolicy>
	std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query) const;

	int GetDocumentCount() const;

	std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::string_view raw_query, int document_id) const;

	std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::sequenced_policy, std::string_view raw_query, int document_id) const;

	std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::parallel_policy, std::string_view raw_query, int document_id) const;

	auto begin() const {
		return document_ids_.begin();
	};

	auto end() const {
		return document_ids_.end();
	};

	const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

	template<class ExecutionPolicy>
	void RemoveDocument(ExecutionPolicy&& policy, int document_id);

	void RemoveDocument(int document_id);

	std::set<std::string> set_of_string_;
private:
	struct DocumentData {
		int rating;
		DocumentStatus status;
	};
	const std::set<std::string> stop_words_;
	std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
	std::map<int, std::map<std::string_view, double>> id_to_word_freqs_;
	std::map<int, DocumentData> documents_;
	std::set<int> document_ids_;

	template<class ExecutionPolicy>
	bool IsValidWord(ExecutionPolicy&& policy, std::string_view word) const;

	bool IsValidWord(std::string_view word) const;

	bool IsStopWord(const std::string& word) const;

	std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;

	int ComputeAverageRating(const std::vector<int>& ratings) const;

	struct QueryWord {
		std::string data;
		bool is_minus;
		bool is_stop;
	};

	QueryWord ParseQueryWord(std::string_view text) const;

	struct Query {
		std::vector<std::string> plus_words;
		std::vector<std::string> minus_words;
	};

	Query ParseQuery(std::string_view text, bool unique) const;

	Query ParseQuery(std::execution::sequenced_policy, std::string_view text, bool unique) const;

	Query ParseQuery(std::execution::parallel_policy, std::string_view text, bool unique) const;

	double ComputeWordInverseDocumentFreq(std::string_view word) const;

	template <typename DocumentPredicate>
	std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;

	template <typename DocumentPredicate>
	std::vector<Document> FindAllDocuments(std::execution::parallel_policy, const Query& query, DocumentPredicate document_predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
	: stop_words_(MakeUniqueNonEmptyStrings(stop_words))
{
	if (!all_of(stop_words_.begin(), stop_words_.end(), [this](auto& word) {
			return IsValidWord(word);
		})) {
		throw std::invalid_argument("Спецсимвол");
	}
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const {
	//LOG_DURATION_STREAM((std::string)"FTD", std::cerr);
	const Query query = ParseQuery(raw_query, true);
	auto matched_documents = FindAllDocuments(query, document_predicate);

	std::sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
		if (std::abs(lhs.relevance - rhs.relevance) < RELEVANCE_ROUNDING) {
			return lhs.rating > rhs.rating;
		}
		else {
			return lhs.relevance > rhs.relevance;
		}
		});
	if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
		matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
	}
	return matched_documents;
}

template <typename DocumentPredicate, class ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, DocumentPredicate document_predicate) const {
	//LOG_DURATION_STREAM((std::string)"FTD", std::cerr);
	if constexpr (std::is_same_v<std::decay_t<ExecutionPolicy>, std::execution::sequenced_policy>) {
		return FindTopDocuments(raw_query, document_predicate);

	}
	const Query query = ParseQuery(std::execution::par, raw_query, true);
	auto matched_documents = FindAllDocuments(std::execution::par, query, document_predicate);

	std::sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
		if (std::abs(lhs.relevance - rhs.relevance) < RELEVANCE_ROUNDING) {
			return lhs.rating > rhs.rating;
		}
		else {
			return lhs.relevance > rhs.relevance;
		}
		});
	if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
		matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
	}
	return matched_documents;
}

template<class ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, DocumentStatus status) const {
	return SearchServer::FindTopDocuments(policy, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
		return document_status == status;
		});
}

template<class ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query) const {
	return SearchServer::FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
	std::map<int, double> document_to_relevance;
	for (const std::string& word : query.plus_words) {
		if (word_to_document_freqs_.count(word) == 0) {
			continue;
		}
		const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
		for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
			if (term_freq == 0) {
				continue;
			}
			const auto& document_data = documents_.at(document_id);
			if (document_predicate(document_id, document_data.status, document_data.rating)) {
				document_to_relevance[document_id] += term_freq * inverse_document_freq;
			}
		}
	}
	for (const std::string& word : query.minus_words) {
		if (word_to_document_freqs_.count(word) == 0) {
			continue;
		}
		for (const auto& [document_id, _] : word_to_document_freqs_.at(word)) {
			document_to_relevance.erase(document_id);
		}
	}
	std::vector<Document> matched_documents;
	for (const auto& [document_id, relevance] : document_to_relevance) {
		matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
	}
	return matched_documents;
}

template<typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::parallel_policy, const Query& query, DocumentPredicate document_predicate) const {
	ConcurrentMap<int, double> document_to_relevance(CONCURRENT_MAP_PARTS);
	std::for_each(std::execution::par, query.plus_words.begin(), query.plus_words.end(), [&](std::string_view word) {
		if (word_to_document_freqs_.count(word) == 0) {
			return;
		}
		const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
		for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
			if (term_freq == 0) {
				continue;
			}
			const auto& document_data = documents_.at(document_id);
			if (document_predicate(document_id, document_data.status, document_data.rating)) {
				document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
			}
		}
		});
	std::for_each(std::execution::par, query.minus_words.begin(), query.minus_words.end(), [&](std::string_view word) {
		if (word_to_document_freqs_.count(word) == 0) {
			return;
		}
		for (const auto& [document_id, _] : word_to_document_freqs_.at(word)) {
			document_to_relevance.erase(document_id);
		}
		});
	std::vector<Document> matched_documents;
	for (const auto& [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
		matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
	}
	return matched_documents;
}


template<class ExecutionPolicy>
void SearchServer::RemoveDocument(ExecutionPolicy&& policy, int document_id) {
	if (!document_ids_.count(document_id)) {
		throw std::invalid_argument("Документа нет");
	};
	std::vector<std::string_view> vec1(id_to_word_freqs_[document_id].size());
	std::transform(policy, id_to_word_freqs_[document_id].begin(), id_to_word_freqs_[document_id].end(), vec1.begin(), [](auto& data) {
		return data.first;
		});
	for_each(policy, vec1.begin(), vec1.end(), [&](auto& data) {
		word_to_document_freqs_[data].erase(document_id);
		});
	id_to_word_freqs_.erase(document_id);
	documents_.erase(document_id);
	document_ids_.erase(document_id);
}

template<class ExecutionPolicy>
inline bool SearchServer::IsValidWord(ExecutionPolicy&& policy, std::string_view word) const {
	return std::none_of(policy, word.begin(), word.end(), [](char c) {
		return c >= '\0' && c < ' ';
		});
}