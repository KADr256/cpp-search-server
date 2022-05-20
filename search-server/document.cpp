#include "document.h"

    Document::Document(int id, double relevance, int rating)
        : id(id)
        , relevance(relevance)
        , rating(rating) {
    }

std::ostream& operator<< (std::ostream& output, const Document& doc) {
    std::cout << "{ document_id = " << doc.id << ", relevance = " << doc.relevance << ", rating = " << doc.rating << " }";
    return output;
}