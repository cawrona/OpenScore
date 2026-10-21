// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "EntryModel.h"

#include <QFutureWatcher>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QSqlDatabase>
#include <QVariantList>
#include <QVector>

struct ScoreRec {
    qint64 id = 0;
    QString path, rel, sha1, title, composer;
    int pages = 0;
    qint64 size = 0, mtime = 0, addedAt = 0, lastOpened = 0;
    int openCount = 0, lastPage = 0;
};

struct ScanItem {
    QString path, sha1, title, composer;
    int pages = 0;
    qint64 size = 0, mtime = 0;
};

struct ScanResult {
    QVector<ScanItem> items;   // new or changed files
    QSet<QString> present;     // every PDF currently on disk
};

// The library mirrors a real folder on disk. Files stay where the user put
// them; the database only adds metadata and history. Browsing sorts by
// recency, so what you played last is always at the top, and a folder is as
// "recent" as the most recently opened score inside it.
class Library : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString rootPath READ rootPath NOTIFY rootChanged)
    Q_PROPERTY(QString folder READ folder NOTIFY folderChanged)
    Q_PROPERTY(QVariantList breadcrumbs READ breadcrumbs NOTIFY folderChanged)
    Q_PROPERTY(EntryModel *browser READ browser CONSTANT)
    Q_PROPERTY(EntryModel *recent READ recent CONSTANT)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(int sortMode READ sortMode WRITE setSortMode NOTIFY sortModeChanged) // 0 recent, 1 name, 2 added
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(int totalScores READ totalScores NOTIFY viewsChanged)
    Q_PROPERTY(int recentCount READ recentCount NOTIFY viewsChanged)
    Q_PROPERTY(bool rootExists READ rootExists NOTIFY rootChanged)

public:
    explicit Library(QObject *parent = nullptr);
    ~Library() override;

    QString rootPath() const { return m_root; }
    QString folder() const { return m_folder; }
    QVariantList breadcrumbs() const;
    EntryModel *browser() { return &m_browser; }
    EntryModel *recent() { return &m_recent; }
    bool scanning() const { return m_scanning; }
    int sortMode() const { return m_sort; }
    void setSortMode(int m);
    QString searchText() const { return m_search; }
    void setSearchText(const QString &t);
    int totalScores() const { return int(m_scores.size()); }
    int recentCount() const { return m_recent.rowCount(); }
    bool rootExists() const;

    Q_INVOKABLE void setRoot(const QString &path); // accepts a path or file:// URL
    Q_INVOKABLE void openFolder(const QString &rel);
    Q_INVOKABLE void rescan();
    Q_INVOKABLE void noteOpened(const QString &path);
    Q_INVOKABLE int lastPage(const QString &path) const;
    Q_INVOKABLE void setLastPage(const QString &path, int page);
    Q_INVOKABLE bool createFolder(const QString &name);
    Q_INVOKABLE bool moveScore(const QString &path, const QString &destRel);
    Q_INVOKABLE QString folderUrl(const QString &path) const;

signals:
    void rootChanged();
    void folderChanged();
    void scanningChanged();
    void sortModeChanged();
    void searchTextChanged();
    void viewsChanged();

private:
    bool openDb();
    void loadAll();
    void refreshViews();
    void applyScan(const ScanResult &r);
    QString thumbPath(const QString &sha1) const;
    QString thumbUrl(const QString &sha1) const;
    ScoreRec *byPath(const QString &path);
    Entry entryFor(const ScoreRec &s) const;

    QSqlDatabase m_db;
    QString m_root;
    QString m_folder; // relative to root, "" = root
    QString m_search;
    int m_sort = 0;
    bool m_scanning = false;
    bool m_rescanQueued = false;
    QVector<ScoreRec> m_scores;
    QHash<QString, int> m_index; // path -> index into m_scores
    EntryModel m_browser;
    EntryModel m_recent;
    QFutureWatcher<ScanResult> m_watcher;
};
