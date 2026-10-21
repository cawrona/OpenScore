// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <QAbstractListModel>
#include <QStringList>
#include <QVector>

struct Entry {
    bool folder = false;
    QString name;      // display name (folder name or score title)
    QString path;      // absolute path
    QString rel;       // path relative to the library root
    QString composer;
    QString sha1;
    QString thumb;     // file:// URL of the cover thumbnail, or empty
    QStringList previews; // folders: up to 3 thumbnails of what is inside
    int pages = 0;
    int count = 0;     // folders: number of scores inside (recursive)
    int openCount = 0;
    qint64 lastOpened = 0; // folders: most recent open among descendants
    qint64 added = 0;
};

class EntryModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        IsFolderRole = Qt::UserRole + 1,
        NameRole,
        PathRole,
        RelRole,
        ComposerRole,
        PagesRole,
        CountRole,
        OpenCountRole,
        LastOpenedTextRole,
        ThumbRole,
        PreviewsRole,
    };

    explicit EntryModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEntries(QVector<Entry> entries);

private:
    QVector<Entry> m_entries;
};
