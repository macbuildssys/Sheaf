#include "documentmodel.h"

#include "guiutil.h"
#include "pathutil.h"
#include "sorting.h"

#include <QFileInfo>
#include <QMimeData>
#include <QUrl>

namespace sheaf {
namespace gui {

DocumentModel::DocumentModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void DocumentModel::setDocuments(const QVector<DocumentInfo> &docs)
{
    beginResetModel();
    m_docs = docs;
    endResetModel();
}

int DocumentModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_docs.size();
}

QVariant DocumentModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_docs.size()) {
        return QVariant();
    }
    const DocumentInfo &doc = m_docs.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return doc.displayName();
    case Qt::ToolTipRole:
        return doc.path;
    case Qt::AccessibleTextRole:
        return accessibleSummary(doc);
    case Qt::AccessibleDescriptionRole:
        return doc.path;
    case DocumentRole:
        return QVariant::fromValue(doc);
    case KeyRole:
        return doc.key;
    case PathRole:
        return doc.path;
    default:
        return QVariant();
    }
}

Qt::ItemFlags DocumentModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
}

QStringList DocumentModel::mimeTypes() const
{
    return {QStringLiteral("text/uri-list")};
}

QMimeData *DocumentModel::mimeData(const QModelIndexList &indexes) const
{
    QList<QUrl> urls;
    for (const QModelIndex &index : indexes) {
        if (index.isValid() && index.row() < m_docs.size() && m_docs.at(index.row()).exists) {
            urls << QUrl::fromLocalFile(m_docs.at(index.row()).path);
        }
    }
    if (urls.isEmpty()) {
        return nullptr;
    }
    auto *data = new QMimeData;
    data->setUrls(urls);
    return data;
}

DocumentFilterProxy::DocumentFilterProxy(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(false);
    sort(0, Qt::AscendingOrder);
}

void DocumentFilterProxy::setGroup(const Group &group)
{
    m_group = group;
    m_hasGroup = true;
    m_tag.clear();
    invalidate();
}

void DocumentFilterProxy::setTagFilter(const QString &tag)
{
    m_tag = tag;
    m_hasGroup = false;
    invalidate();
}

void DocumentFilterProxy::setQuery(const QString &text)
{
    m_query = SearchQuery::parse(text);
    invalidate();
}

void DocumentFilterProxy::setSortOptions(const SortOptions &options)
{
    m_sort = options;
    invalidate();
}

const DocumentInfo &DocumentFilterProxy::documentAt(const QModelIndex &proxyIndex) const
{
    const auto *source = static_cast<const DocumentModel *>(sourceModel());
    return source->at(mapToSource(proxyIndex).row());
}

void DocumentFilterProxy::setHideMissing(bool hide)
{
    if (m_hideMissing != hide) {
        m_hideMissing = hide;
        invalidateFilter();
    }
}

bool DocumentFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex &) const
{
    const auto *source = static_cast<const DocumentModel *>(sourceModel());
    const DocumentInfo &doc = source->at(sourceRow);
    if (m_hideMissing && !doc.exists && !m_query.onlyMissing) {
        return false;
    }
    if (m_hasGroup && !groupMatches(m_group, doc)) {
        return false;
    }
    if (!m_tag.isEmpty()) {
        bool found = false;
        for (const QString &tag : doc.tags) {
            if (tag.compare(m_tag, Qt::CaseInsensitive) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return m_query.isEmpty() || m_query.matches(doc);
}

bool DocumentFilterProxy::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    const auto *source = static_cast<const DocumentModel *>(sourceModel());
    return documentLess(source->at(left.row()), source->at(right.row()), m_sort);
}

} // namespace gui
} // namespace sheaf
