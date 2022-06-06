#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
	std::set< std::set<std::string>> unique_set_of_words;
	std::vector<int> id_to_delete;
	for (int document_id : search_server) {
		std::map<std::string, double> check_map = search_server.GetWordFrequencies(document_id);
		std::set<std::string> set_of_document_words;
		for (auto word : check_map) {
			set_of_document_words.insert(word.first);
		}
		if (unique_set_of_words.count(set_of_document_words)) {
			id_to_delete.push_back(document_id);
		}
		else {
			unique_set_of_words.insert(set_of_document_words);
		}
	}
	for (auto id : id_to_delete) {
		search_server.RemoveDocument(id);
		std::cout << "Found duplicate document id " << id << std::endl;
	}
}