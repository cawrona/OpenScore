// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Library.h"

#include "PdfDocument.h"
#include "Util.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSettings>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

namespace {

struct KnownInfo {
    qint64 size = 0;
    qint64 mtime = 0;
    QString sha1;
};

QString titleFromFile(const QString &path)
{
    QString t = QFileInfo(path).completeBaseName();
    t.replace(QLatin1Char('_'), QLatin1Char(' '));
    return t.simplified();
}

bool usableTitle(const QString &t)
{
    return !t.isEmpty() && !t.startsWith(QLatin1String("Microsoft Word"), Qt::CaseInsensitive)
        && !t.startsWith(QLatin1String("Untitled"), Qt::CaseInsensitive);
}

bool writeThumb(const PdfDocument &d, const QString &file)
{
    const QVector<QSizeF> sizes = d.pageSizes();
    if (sizes.isEmpty())
        return false;
    const int w = 360;
    const int h = qMax(1, qRound(w * sizes[0].height() / sizes[0].width()));
    const QImage img = d.render(0, QSize(w, h));
    if (img.isNull())
        return false;
    QDir().mkpath(QFileInfo(file).absolutePath());
    return img.save(file, "PNG");
}

// Runs on a worker thread: walks the library folder, reads metadata for new or
// changed PDFs and makes cover thumbnails. Touches no shared state.
ScanResult scanTree(QString root, QHash<QString, KnownInfo> known, QString thumbDir)
{
    ScanResult r;
    QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    while (it.hasNext()) {
        const QString path = it.next();
        const QFileInfo fi = it.fileInfo();
        if (fi.suffix().compare(QLatin1String("pdf"), Qt::CaseInsensitive) != 0)
            continue;
        r.present.insert(path);

        const qint64 size = fi.size();
        const qint64 mtime = fi.lastModified().toSecsSinceEpoch();
        const auto k = known.constFind(path);
        const bool unchanged = k != known.constEnd() && k->size == size && k->mtime == mtime;
        if (unchanged && !k->sha1.isEmpty() && QFileInfo::exists(thumbDir + QLatin1Char('/') + k->sha1 + QLatin1String(".png")))
            continue;

        PdfDocument d;
        if (!d.open(path))
            continue;
        const QString sha = (unchanged && !k->sha1.isEmpty()) ? k->sha1 : Util::fileSha1(path);
        const QString tp = thumbDir + QLatin1Char('/') + sha + QLatin1String(".png");
        if (!QFileInfo::exists(tp))
            writeThumb(d, tp);
        if (unchanged)
            continue;

        ScanItem item;
        item.path = path;
        item.sha1 = sha;
        item.pages = d.pageCount();
        item.size = size;
        item.mtime = mtime;
        const QString meta = d.metadata("info:Title");
        item.title = usableTitle(meta) ? meta : titleFromFile(path);
        item.composer = d.metadata("info:Author");
        r.items.push_back(item);
    }
    return r;
}

} // namespace

Library::Library(QObject *parent)
    : QObject(parent)
{
    QSettings s;
    m_root = s.value(QStringLiteral("libraryRoot")).toString();
    if (m_root.isEmpty()) {
        m_root = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QStringLiteral("/Scores");
        QDir().mkpath(m_root);
    }
    m_root = QDir::cleanPath(m_root);

    openDb();
    loadAll();
    refreshViews();

    connect(&m_watcher, &QFutureWatcher<ScanResult>::finished, this, [this] {
        applyScan(m_watcher.result());
        m_scanning = false;
        emit scanningChanged();
        if (m_rescanQueued) {
            m_rescanQueued = false;
            rescan();
        }
    });
    rescan();
}

Library::~Library()
{
    m_watcher.waitForFinished();
    const QString name = m_db.connectionName();
    m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(name);
}

bool Library::openDb()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("openscore"));
    m_db.setDatabaseName(dir + QStringLiteral("/library.db"));
    if (!m_db.open()) {
        qWarning() << "Cannot open library database:" << m_db.lastError().text();
        return false;
    }
    QSqlQuery q(m_db);
    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS scores("
        " id INTEGER PRIMARY KEY, path TEXT UNIQUE NOT NULL, sha1 TEXT, title TEXT, composer TEXT,"
        " pages INTEGER DEFAULT 0, size INTEGER DEFAULT 0, mtime INTEGER DEFAULT 0, added_at INTEGER DEFAULT 0,"
        " last_opened INTEGER DEFAULT 0, open_count INTEGER DEFAULT 0, last_page INTEGER DEFAULT 0)"));
    q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_scores_sha1 ON scores(sha1)"));
    return true;
}

void Library::loadAll()
{
    m_scores.clear();
    m_index.clear();
    if (!m_db.isOpen())
        return;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT id,path,sha1,title,composer,pages,size,mtime,added_at,last_opened,open_count,last_page FROM scores"));
    const QDir root(m_root);
    const QString prefix = m_root + QLatin1Char('/');
    while (q.next()) {
        ScoreRec s;
        s.id = q.value(0).toLongLong();
        s.path = q.value(1).toString();
        if (!s.path.startsWith(prefix))
            continue; // belongs to a different library root
        s.rel = root.relativeFilePath(s.path);
        s.sha1 = q.value(2).toString();
        s.title = q.value(3).toString();
        s.composer = q.value(4).toString();
        s.pages = q.value(5).toInt();
        s.size = q.value(6).toLongLong();
        s.mtime = q.value(7).toLongLong();
        s.addedAt = q.value(8).toLongLong();
        s.lastOpened = q.value(9).toLongLong();
        s.openCount = q.value(10).toInt();
        s.lastPage = q.value(11).toInt();
        m_index.insert(s.path, int(m_scores.size()));
        m_scores.push_back(s);
    }
}

ScoreRec *Library::byPath(const QString &path)
{
    const auto it = m_index.constFind(path);
    return it == m_index.constEnd() ? nullptr : &m_scores[it.value()];
}

bool Library::rootExists() const
{
    return QDir(m_root).exists();
}

QString Library::thumbPath(const QString &sha1) const
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/thumbs/") + sha1
        + QStringLiteral(".png");
}

QString Library::thumbUrl(const QString &sha1) const
{
    if (sha1.isEmpty())
        return {};
    const QString p = thumbPath(sha1);
    return QFileInfo::exists(p) ? QUrl::fromLocalFile(p).toString() : QString();
}

Entry Library::entryFor(const ScoreRec &s) const
{
    Entry e;
    e.name = s.title;
    e.path = s.path;
    e.rel = s.rel;
    e.composer = s.composer;
    e.sha1 = s.sha1;
    e.thumb = thumbUrl(s.sha1);
    e.pages = s.pages;
    e.openCount = s.openCount;
    e.lastOpened = s.lastOpened;
    e.added = s.addedAt;
    return e;
}

void Library::refreshViews()
{
    // --- Recently viewed strip -------------------------------------------
    QVector<const ScoreRec *> opened;
    for (const ScoreRec &s : std::as_const(m_scores)) {
        if (s.lastOpened > 0)
            opened.push_back(&s);
    }
    std::sort(opened.begin(), opened.end(), [](const ScoreRec *a, const ScoreRec *b) { return a->lastOpened > b->lastOpened; });
    QVector<Entry> recent;
    for (int i = 0; i < opened.size() && i < 12; ++i)
        recent.push_back(entryFor(*opened[i]));
    m_recent.setEntries(recent);

    // --- Folder / search view --------------------------------------------
    QVector<Entry> out;
    const QString needle = m_search.trimmed();
    if (!needle.isEmpty()) {
        for (const ScoreRec &s : std::as_const(m_scores)) {
            if (s.title.contains(needle, Qt::CaseInsensitive) || s.composer.contains(needle, Qt::CaseInsensitive)
                || s.rel.contains(needle, Qt::CaseInsensitive))
                out.push_back(entryFor(s));
        }
    } else {
        const QDir dir(m_folder.isEmpty() ? m_root : m_root + QLatin1Char('/') + m_folder);
        const QString base = m_folder.isEmpty() ? QString() : m_folder + QLatin1Char('/');

        const QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &name : subdirs) {
            const QString prefix = base + name + QLatin1Char('/');
            Entry e;
            e.folder = true;
            e.name = name;
            e.path = dir.filePath(name);
            e.rel = base + name;
            QVector<const ScoreRec *> inside;
            for (const ScoreRec &s : std::as_const(m_scores)) {
                if (s.rel.startsWith(prefix))
                    inside.push_back(&s);
            }
            e.count = int(inside.size());
            for (const ScoreRec *s : std::as_const(inside)) {
                e.lastOpened = qMax(e.lastOpened, s->lastOpened);
                e.added = qMax(e.added, s->addedAt);
            }
            std::sort(inside.begin(), inside.end(), [](const ScoreRec *a, const ScoreRec *b) {
                if (a->lastOpened != b->lastOpened)
                    return a->lastOpened > b->lastOpened;
                return a->title.localeAwareCompare(b->title) < 0;
            });
            for (const ScoreRec *s : std::as_const(inside)) {
                const QString t = thumbUrl(s->sha1);
                if (!t.isEmpty())
                    e.previews << t;
                if (e.previews.size() >= 3)
                    break;
            }
            out.push_back(e);
        }
        for (const ScoreRec &s : std::as_const(m_scores)) {
            const int slash = s.rel.lastIndexOf(QLatin1Char('/'));
            const QString parent = slash < 0 ? QString() : s.rel.left(slash);
            if (parent == m_folder)
                out.push_back(entryFor(s));
        }
    }

    const int mode = m_sort;
    std::stable_sort(out.begin(), out.end(), [mode](const Entry &a, const Entry &b) {
        auto byName = [&] { return a.name.localeAwareCompare(b.name) < 0; };
        if (mode == 1) { // name: folders first
            if (a.folder != b.folder)
                return a.folder;
            return byName();
        }
        if (mode == 2) { // date added
            if (a.added != b.added)
                return a.added > b.added;
            return byName();
        }
        // recent: whatever you touched last comes first, folders and scores mixed
        if (a.lastOpened != b.lastOpened)
            return a.lastOpened > b.lastOpened;
        if (a.folder != b.folder)
            return a.folder;
        return byName();
    });
    m_browser.setEntries(out);
    emit viewsChanged();
}

QVariantList Library::breadcrumbs() const
{
    QVariantList out;
    out.push_back(QVariantMap{{QStringLiteral("name"), QStringLiteral("Library")}, {QStringLiteral("rel"), QString()}});
    QString acc;
    for (const QString &seg : m_folder.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
        acc += (acc.isEmpty() ? QString() : QStringLiteral("/")) + seg;
        out.push_back(QVariantMap{{QStringLiteral("name"), seg}, {QStringLiteral("rel"), acc}});
    }
    return out;
}

void Library::setSortMode(int m)
{
    if (m == m_sort)
        return;
    m_sort = m;
    emit sortModeChanged();
    refreshViews();
}

void Library::setSearchText(const QString &t)
{
    if (t == m_search)
        return;
    m_search = t;
    emit searchTextChanged();
    refreshViews();
}

void Library::setRoot(const QString &path)
{
    QString p = path.startsWith(QLatin1String("file:")) ? QUrl(path).toLocalFile() : path;
    p = QDir::cleanPath(p);
    if (p.isEmpty() || !QDir(p).exists() || p == m_root)
        return;
    m_root = p;
    QSettings().setValue(QStringLiteral("libraryRoot"), m_root);
    m_folder.clear();
    m_search.clear();
    loadAll();
    refreshViews();
    emit rootChanged();
    emit folderChanged();
    emit searchTextChanged();
    rescan();
}

void Library::openFolder(const QString &rel)
{
    m_folder = QDir::cleanPath(rel);
    if (m_folder == QLatin1String("."))
        m_folder.clear();
    if (!m_search.isEmpty()) {
        m_search.clear();
        emit searchTextChanged();
    }
    emit folderChanged();
    refreshViews();
}

void Library::rescan()
{
    if (m_scanning) {
        m_rescanQueued = true;
        return;
    }
    if (!rootExists())
        return;
    QHash<QString, KnownInfo> known;
    for (const ScoreRec &s : std::as_const(m_scores))
        known.insert(s.path, {s.size, s.mtime, s.sha1});
    const QString thumbDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/thumbs");
    m_scanning = true;
    emit scanningChanged();
    m_watcher.setFuture(QtConcurrent::run(&scanTree, m_root, known, thumbDir));
}

void Library::applyScan(const ScanResult &r)
{
    if (!m_db.isOpen())
        return;

    // Files that vanished from their old path but reappeared elsewhere (same
    // content hash) were moved: keep their history instead of starting over.
    QHash<QString, int> missingBySha;
    for (int i = 0; i < m_scores.size(); ++i) {
        const ScoreRec &s = m_scores[i];
        if (!r.present.contains(s.path) && !s.sha1.isEmpty())
            missingBySha.insert(s.sha1, i);
    }
    QSet<qint64> renamed;
    const qint64 now = QDateTime::currentSecsSinceEpoch();

    m_db.transaction();
    QSqlQuery q(m_db);
    for (const ScanItem &it : r.items) {
        if (const ScoreRec *s = byPath(it.path)) {
            q.prepare(QStringLiteral("UPDATE scores SET sha1=?,pages=?,size=?,mtime=? WHERE id=?"));
            q.addBindValue(it.sha1);
            q.addBindValue(it.pages);
            q.addBindValue(it.size);
            q.addBindValue(it.mtime);
            q.addBindValue(s->id);
            q.exec();
            continue;
        }
        const auto moved = missingBySha.find(it.sha1);
        if (moved != missingBySha.end()) {
            const ScoreRec &old = m_scores[moved.value()];
            q.prepare(QStringLiteral("UPDATE scores SET path=?,sha1=?,pages=?,size=?,mtime=? WHERE id=?"));
            q.addBindValue(it.path);
            q.addBindValue(it.sha1);
            q.addBindValue(it.pages);
            q.addBindValue(it.size);
            q.addBindValue(it.mtime);
            q.addBindValue(old.id);
            q.exec();
            renamed.insert(old.id);
            missingBySha.erase(moved);
            continue;
        }
        q.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO scores(path,sha1,title,composer,pages,size,mtime,added_at) VALUES(?,?,?,?,?,?,?,?)"));
        q.addBindValue(it.path);
        q.addBindValue(it.sha1);
        q.addBindValue(it.title);
        q.addBindValue(it.composer);
        q.addBindValue(it.pages);
        q.addBindValue(it.size);
        q.addBindValue(it.mtime);
        q.addBindValue(now);
        q.exec();
    }
    for (const ScoreRec &s : std::as_const(m_scores)) {
        if (!r.present.contains(s.path) && !renamed.contains(s.id)) {
            q.prepare(QStringLiteral("DELETE FROM scores WHERE id=?"));
            q.addBindValue(s.id);
            q.exec();
        }
    }
    m_db.commit();

    loadAll();
    refreshViews();
}

void Library::noteOpened(const QString &path)
{
    ScoreRec *s = byPath(path);
    if (!s)
        return; // opened from outside the library folder: no history
    s->lastOpened = QDateTime::currentSecsSinceEpoch();
    s->openCount++;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE scores SET last_opened=?, open_count=? WHERE id=?"));
    q.addBindValue(s->lastOpened);
    q.addBindValue(s->openCount);
    q.addBindValue(s->id);
    q.exec();
    refreshViews();
}

int Library::lastPage(const QString &path) const
{
    const auto it = m_index.constFind(path);
    return it == m_index.constEnd() ? 0 : m_scores[it.value()].lastPage;
}

void Library::setLastPage(const QString &path, int page)
{
    ScoreRec *s = byPath(path);
    if (!s || s->lastPage == page)
        return;
    s->lastPage = page;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE scores SET last_page=? WHERE id=?"));
    q.addBindValue(page);
    q.addBindValue(s->id);
    q.exec();
}

bool Library::createFolder(const QString &name)
{
    const QString n = name.trimmed();
    if (n.isEmpty() || n.contains(QLatin1Char('/')) || n.startsWith(QLatin1Char('.')))
        return false;
    QDir dir(m_folder.isEmpty() ? m_root : m_root + QLatin1Char('/') + m_folder);
    const bool ok = dir.mkdir(n);
    if (ok)
        refreshViews();
    return ok;
}

bool Library::moveScore(const QString &path, const QString &destRel)
{
    ScoreRec *s = byPath(path);
    if (!s)
        return false;
    const QString destDir = destRel.isEmpty() ? m_root : m_root + QLatin1Char('/') + destRel;
    const QString target = destDir + QLatin1Char('/') + QFileInfo(path).fileName();
    if (QFileInfo::exists(target) || !QDir(destDir).exists())
        return false;
    if (!QFile::rename(path, target))
        return false;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE scores SET path=? WHERE id=?"));
    q.addBindValue(target);
    q.addBindValue(s->id);
    q.exec();
    loadAll();
    refreshViews();
    return true;
}

QString Library::folderUrl(const QString &path) const
{
    const QFileInfo fi(path);
    return QUrl::fromLocalFile(fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath()).toString();
}
