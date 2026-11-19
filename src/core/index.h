#pragma once

#include "database.h"
#include "scanner.h"
#include "types.h"

#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QObject>
#include <QTimer>

namespace sheaf {

// Owns the merged list of documents. Scans run on a worker thread and are
// coalesced, so a burst of file changes causes one scan and the interface never waits
class DocumentIndex : public QObject {
    Q_OBJECT
public:
    explicit DocumentIndex(Database *database, QObject *parent = nullptr);
    ~DocumentIndex() override;

    void setOptions(const ScanOptions &options);
    ScanOptions options() const { return m_options; }

    // Starts watching the recent documents files and runs the first scan
    void start();

    const QVector<DocumentInfo> &documents() const { return m_docs; }
    const DocumentInfo *find(const QString &key) const;
    bool isBusy() const { return m_busy; }

    bool setFavorite(const DocumentInfo &doc, bool on);
    bool setTags(const DocumentInfo &doc, const QStringList &tags);
    bool recordOpen(const DocumentInfo &doc);

    // Removes the documents from the recent list, the file stays untouched
    bool removeFromRecents(const QList<DocumentInfo> &docs, QString *error = nullptr);
    bool clearRecents(QString *error = nullptr);

    // Removes documents from every list and forgets favorites and tags, used after trashing
    bool removeEverything(const QList<DocumentInfo> &docs, QString *error = nullptr);

    // Forgets everything about documents that no longer exist
    int removeMissing(QString *error = nullptr);

    bool relocate(const DocumentInfo &doc, const QString &newPath);

public slots:
    void refresh();

signals:
    void documentsChanged();
    void busyChanged(bool busy);
    void warning(const QString &message);

private slots:
    void onWatchedChange();
    void onScanFinished();
    void checkSignature();

private:
    void applyUserData();
    void rewatch();
    QString signature() const;
    ScanInput makeInput() const;

    Database *m_db;
    ScanOptions m_options;
    QVector<DocumentInfo> m_docs;
    QFileSystemWatcher m_watcher;
    QFutureWatcher<ScanResult> m_scan;
    QTimer m_debounce;
    QTimer m_poll;
    QString m_signature;
    bool m_busy = false;
    bool m_again = false;
    bool m_started = false;
};

} // namespace sheaf
