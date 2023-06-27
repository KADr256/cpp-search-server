#include "search_server.h"

#include <numeric>

const int QUERY_VECTOR_COUNT = 500;

SearchServer::SearchServer(const std::string& stop_words_text)
	: SearchServer(SplitIntoWords(stop_words_text))
{
}

SearchServer::SearchServer(std::string_view stop_words_text)
	: SearchServer(SplitIntoWords((std::string)stop_words_text))
{
}

void SearchServer::AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings) {
	if (document_id < 0) {
		throw std::invalid_argument("Отрицательный идентификатор");
	}
	if (document_ids_.count(document_id) == 1) {
		throw std::invalid_argument("Идентификатор используется");
	}
	if (!IsValidWord((std::string)document)) {
		throw std::invalid_argument("Спецсимвол");
	}
	const std::vector<std::string> words = SplitIntoWordsNoStop((std::string)document);
	const double inv_word_count = 1.0 / words.size();
	for (const std::string& word : words) {
		auto some_pair = set_of_string_.insert(word);
		word_to_document_freqs_[*some_pair.first][document_id] += inv_word_count;
		id_to_word_freqs_[document_id][*some_pair.first] += inv_word_count;
	}
	documents_.insert({ document_id, DocumentData{ ComputeAverageRating(ratings), status } });
	document_ids_.insert(document_id);
	return;
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus status) const {
	return SearchServer::FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
		return document_status == status;
		});
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query) const {
	return SearchServer::FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
	return document_ids_.size();
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::string_view raw_query, int document_id) const {
	//LOG_DURATION_STREAM((std::string)"MD", std::cerr);
	if (!IsValidWord(raw_query)) {
		throw std::invalid_argument("Спецсимвол");
	}
	if (document_ids_.count(document_id) == 0) {
		throw std::out_of_range("Нет ID");
	}
	const Query query = ParseQuery(raw_query,true);
	std::vector<std::string_view> matched_words;
	for (std::string_view word : query.minus_words) {
		if (id_to_word_freqs_.at(document_id).count(word) == 0) {
			continue;
		}
		else {
			return { matched_words, documents_.at(document_id).status };
		}
	}
	for (std::string_view word : query.plus_words) {
		if (id_to_word_freqs_.at(document_id).count(word) == 0) {
			continue;
		}
		else {
			matched_words.push_back(*set_of_string_.find((std::string)word));
		}
	}
	return { matched_words, documents_.at(document_id).status };
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::sequenced_policy, std::string_view raw_query, int document_id) const {
	return MatchDocument(raw_query, document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::parallel_policy, std::string_view raw_query, int document_id) const {
	//LOG_DURATION_STREAM((std::string)"MD", std::cerr);
	//auto test = LogDuration("");
	if (document_ids_.count(document_id) == 0) {
		throw std::out_of_range("Нет ID");
	}
	const Query query = ParseQuery(std::execution::par, raw_query, true);
	std::vector<std::string_view> matched_words;
	if (!none_of(std::execution::par, query.minus_words.begin(), query.minus_words.end(), [&](auto& word) {
		return id_to_word_freqs_.at(document_id).count(word);
		})) {
		return { matched_words, documents_.at(document_id).status };
	};

	matched_words.resize(query.plus_words.size());
	auto del = std::copy_if(std::execution::par, query.plus_words.begin(), query.plus_words.end(), matched_words.begin(), [&](auto& word) {
		return id_to_word_freqs_.at(document_id).count(word);
		});
	matched_words.erase(del, matched_words.end());

	return { matched_words, documents_.at(document_id).status };
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
	if (id_to_word_freqs_.at(document_id).empty()) {
		static std::map<std::string_view, double> empty_string_double_map;
		return empty_string_double_map;
	}
	return id_to_word_freqs_.at(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
	RemoveDocument(std::execution::seq, document_id);
}

bool SearchServer::IsValidWord(std::string_view word) const {
	return IsValidWord(std::execution::seq, word);
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

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
	bool is_minus = false;
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
	return { (std::string)text, is_minus, IsStopWord((std::string)text) };
}

SearchServer::Query SearchServer::ParseQuery(std::string_view text,bool unique=false) const {
	Query query;
	if (!IsValidWord(text)) {
		throw std::invalid_argument("Спецсимвол");
	}
	query.plus_words.reserve(QUERY_VECTOR_COUNT);
	query.minus_words.reserve(QUERY_VECTOR_COUNT);
	for (std::string_view word : SplitIntoWordsView(text)) {
		const QueryWord query_word = ParseQueryWord(word);
		if (!query_word.is_stop) {
			if (query_word.is_minus) {
				query.minus_words.push_back(query_word.data);
			}
			else {
				query.plus_words.push_back(query_word.data);
			}
		}
	}
	if (unique) {
		std::sort(std::execution::seq, query.plus_words.begin(), query.plus_words.end());
		auto del = std::unique(std::execution::seq, query.plus_words.begin(), query.plus_words.end());
		query.plus_words.erase(del, query.plus_words.end());

		std::sort(std::execution::seq, query.minus_words.begin(), query.minus_words.end());
		del = std::unique(std::execution::seq, query.minus_words.begin(), query.minus_words.end());
		query.minus_words.erase(del, query.minus_words.end());
	}
	return query;
}

SearchServer::Query SearchServer::ParseQuery(std::execution::sequenced_policy,std::string_view text,bool unique = false) const {
	return ParseQuery(text,unique);
}

SearchServer::Query SearchServer::ParseQuery(std::execution::parallel_policy,std::string_view text,bool unique = false) const {
	Query query;
	if (!IsValidWord(std::execution::par,text)) {
		throw std::invalid_argument("Спецсимвол");
	}
	query.plus_words.reserve(QUERY_VECTOR_COUNT);
	query.minus_words.reserve(QUERY_VECTOR_COUNT);
	for (std::string_view word : SplitIntoWordsView(text)) {
		const QueryWord query_word = ParseQueryWord(word);
		if (!query_word.is_stop) {
			if (query_word.is_minus) {
				query.minus_words.push_back(query_word.data);
			}
			else {
				query.plus_words.push_back(query_word.data);
			}
		}
	}
	if (unique) {
		auto future1 = std::async([&query]() {
			std::sort(std::execution::par, query.plus_words.begin(), query.plus_words.end());
			auto del = std::unique(std::execution::par, query.plus_words.begin(), query.plus_words.end());
			query.plus_words.erase(del, query.plus_words.end());
			});
		std::sort(std::execution::par, query.minus_words.begin(), query.minus_words.end());
		auto del = std::unique(std::execution::par, query.minus_words.begin(), query.minus_words.end());
		query.minus_words.erase(del, query.minus_words.end());
		future1.wait();
		future1.get();
	}
	return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const {
	return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}