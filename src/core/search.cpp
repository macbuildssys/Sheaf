#include "search.h"

namespace sheaf {

QString foldForSearch(const QString &text)
{
    const QString decomposed = text.normalized(QString::NormalizationForm_D);
    QString out;
    out.reserve(decomposed.size());
    for (const QChar ch : decomposed) {
        if (ch.category() != QChar::Mark_NonSpacing) {
            out.append(ch);
        }
    }
    return out.toCaseFolded();
}

static QStringList tokenize(const QString &text)
{
    QStringList tokens;
    QString current;
    bool quoted = false;
    bool hadQuote = false;
    for (const QChar ch : text) {
        if (ch == QLatin1Char('"')) {
            quoted = !quoted;
            hadQuote = true;
            continue;
        }
        if (ch.isSpace() && !quoted) {
            if (!current.isEmpty() || hadQuote) {
                tokens << current;
            }
            current.clear();
            hadQuote = false;
            continue;
        }
        current.append(ch);
    }
    if (!current.isEmpty() || hadQuote) {
        tokens << current;
    }
    tokens.removeAll(QString());
    return tokens;
}

SearchQuery SearchQuery::parse(const QString &text)
{
    SearchQuery query;
    const QStringList tokens = tokenize(text.left(1000));
    for (const QString &token : tokens) {
        const int colon = token.indexOf(QLatin1Char(':'));
        if (colon > 0 && colon < token.size() - 1) {
            const QString op = token.left(colon).toLower();
            const QString value = foldForSearch(token.mid(colon + 1));
            if (op == QLatin1String("tag")) {
                query.tags << value;
                continue;
            }
            if (op == QLatin1String("type")) {
                query.types << value;
                continue;
            }
            if (op == QLatin1String("in")) {
                query.folders << value;
                continue;
            }
            if (op == QLatin1String("is")) {
                if (value == QLatin1String("fav") || value == QLatin1String("favorite") || value == QLatin1String("favourite")) {
                    query.onlyFavorites = true;
                    continue;
                }
                if (value == QLatin1String("missing")) {
                    query.onlyMissing = true;
                    continue;
                }
            }
        }
        if (token.size() > 1 && token.startsWith(QLatin1Char('-'))) {
            query.excluded << foldForSearch(token.mid(1));
            continue;
        }
        query.words << foldForSearch(token);
    }
    return query;
}

bool SearchQuery::isEmpty() const
{
    return words.isEmpty() && excluded.isEmpty() && tags.isEmpty() && types.isEmpty() && folders.isEmpty() && !onlyFavorites &&
           !onlyMissing;
}

QString SearchQuery::buildHaystack(const DocumentInfo &doc)
{
    QStringList parts{doc.name, doc.title, doc.author, doc.path, doc.mimeComment, doc.tags.join(QLatin1Char(' '))};
    return foldForSearch(parts.join(QLatin1Char('\n')));
}

bool SearchQuery::matches(const DocumentInfo &doc) const
{
    if (onlyFavorites && !doc.favorite) {
        return false;
    }
    if (onlyMissing && doc.exists) {
        return false;
    }
    const QString &hay = doc.haystack.isEmpty() ? buildHaystack(doc) : doc.haystack;
    for (const QString &word : words) {
        if (!hay.contains(word)) {
            return false;
        }
    }
    for (const QString &word : excluded) {
        if (hay.contains(word)) {
            return false;
        }
    }
    for (const QString &wanted : tags) {
        bool found = false;
        for (const QString &tag : doc.tags) {
            if (foldForSearch(tag) == wanted) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    for (const QString &type : types) {
        const QString mime = doc.mime.toLower();
        const QString comment = foldForSearch(doc.mimeComment);
        if (doc.extension != type && !mime.contains(type) && !comment.contains(type)) {
            return false;
        }
    }
    for (const QString &folder : folders) {
        if (!foldForSearch(doc.folder()).contains(folder)) {
            return false;
        }
    }
    return true;
}

} // namespace sheaf
