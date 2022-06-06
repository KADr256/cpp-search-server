#include "search_server.h"
#include <numeric>

inline static constexpr int INVALID_DOCUMENT_ID = -1;

SearchServer::SearchServer(const std::string& stop_words_text)
	: SearchServer(SplitIntoWords(stop_words_text))
{
}

void SearchServer::AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings) {
	if (document_id < 0) {
		throw std::invalid_argument("Отрицательный идентификатор");
	}
	if (documents_.count(document_id) == 1) {
		throw std::invalid_argument("Идентификатор используется");
	}
	if (!IsValidWord(document)) {
		throw std::invalid_argument("Спецсимвол");
	}
	const std::vector<std::string> words = SplitIntoWordsNoStop(document);
	const double inv_word_count = 1.0 / words.size();
	for (const std::string& word : words) {
		word_to_document_freqs_[word][document_id] += inv_word_count;
		id_to_word_freqs_[document_id][word] += inv_word_count;
	}
	documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
	document_ids_.insert(document_id);
	return;
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentStatus status) const {
	return SearchServer::FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
		return document_status == status;
		});
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query) const {
	return SearchServer::FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
	return documents_.size();
}

std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string& raw_query, int document_id) const {
	LOG_DURATION_STREAM((std::string)"MD", std::cerr);
	if (!IsValidWord(raw_query)) {
		throw std::invalid_argument("Спецсимвол");
	}
	const Query query = ParseQuery(raw_query);
	std::vector<std::string> matched_words;
	for (const std::string& word : query.plus_words) {
		if (word_to_document_freqs_.count(word) == 0) {
			continue;
		}
		if (word_to_document_freqs_.at(word).count(document_id)) {
			matched_words.push_back(word);
		}
	}
	for (const std::string& word : query.minus_words) {
		if (word_to_document_freqs_.count(word) == 0) {
			continue;
		}
		if (word_to_document_freqs_.at(word).count(document_id)) {
			matched_words.clear();
			break;
		}
	}
	return { matched_words, documents_.at(document_id).status };
}

const std::map<std::string, double>& SearchServer::GetWordFrequencies(int document_id) const {
	if (id_to_word_freqs_.at(document_id).empty()) {
		return empty_string_double_map;
	}
	return id_to_word_freqs_.at(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
	if (!document_ids_.count(document_id)) {
		throw std::invalid_argument("Документа нет");
	};
	document_ids_.erase(document_id);
	for (auto& data : id_to_word_freqs_[document_id]) {
		word_to_document_freqs_[data.first].erase(document_id);
	};
	id_to_word_freqs_.erase(document_id);
	documents_.erase(document_id);
}


bool SearchServer::IsValidWord(const std::string& word) const {
	// A valid word must not contain special characters
	return none_of(word.begin(), word.end(), [](char c) {
		return c >= '\0' && c < ' ';
		});
}

bool SearchServer::IsStopWord(const std::string& word) const {
	return stop_words_.count(word) > 0;
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const {
	std::vector<std::string> words;
	for (const std::string& word : SplitIntoWords(text)) {
		if (!IsStopWord(word)) {
			words.push_back(word);
		}
	}
	return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) const {
	if (ratings.empty()) {
		return 0;
	}
	int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);
	return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string text) const {
	bool is_minus = false;
	// Word shouldn't be empty   
	if (text.empty()) {
		throw std::out_of_range("Пропажа(?) запроса");
	}
	if (text[0] == '-') {
		if (text.size() == 1) {
			throw std::invalid_argument("Ничего после -");
		}
		if (text[1] == '-') {
			throw std::invalid_argument("Больше одного - подряд");
		}
		is_minus = true;
		text = text.substr(1);
	}
	return { text, is_minus, IsStopWord(text) };
}

SearchServer::Query SearchServer::ParseQuery(const std::string& text) const {
	Query query;
	for (const std::string& word : SplitIntoWords(text)) {
		const QueryWord query_word = ParseQueryWord(word);
		if (!query_word.is_stop) {
			if (query_word.is_minus) {
				query.minus_words.insert(query_word.data);
			}
			else {
				query.plus_words.insert(query_word.data);
			}
		}
	}
	return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const {
	return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}