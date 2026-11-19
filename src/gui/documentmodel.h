#pragma once

#include "groups.h"
#include "search.h"
#include "types.h"

#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include <QVector>

namespace sheaf {
namespace gui {

class DocumentModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role {
        DocumentRole = Qt::UserRole + 1,
        KeyRole,
        PathRole,
    };

    explicit DocumentModel(QObject *parent = nullptr);

    void setDocuments(const QVector<DocumentInfo> &docs);
    const QVector<DocumentInfo> &documents() const { return m_docs; }
    const DocumentInfo &at(int row) const { return m_docs.at(row); }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;

private:
    QVector<DocumentInfo> m_docs;
};

class DocumentFilterProxy : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit DocumentFilterProxy(QObject *parent = nullptr);

    void setGroup(const Group &group);
    void setTagFilter(const QString &tag);
    void setQuery(const QString &text);
    void setSortOptions(const SortOptions &options);
    // Files that are gone stay out of the list unless the search asks for them with is:missing
    void setHideMissing(bool hide);
    SortOptions sortOptions() const { return m_sort; }
    bool hasQuery() const { return !m_query.isEmpty(); }

    const DocumentInfo &documentAt(const QModelIndex &proxyIndex) const;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    Group m_group;
    bool m_hasGroup = false;
    QString m_tag;
    SearchQuery m_query;
    SortOptions m_sort;
    bool m_hideMissing = true;
};

} // namespace gui
} // namespace sheaf
