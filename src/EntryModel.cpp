// SPDX-License-Identifier: AGPL-3.0-or-later
#include "EntryModel.h"

#include "Util.h"

EntryModel::EntryModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int EntryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_entries.size());
}

QVariant EntryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};
    const Entry &e = m_entries[index.row()];
    switch (role) {
    case IsFolderRole: return e.folder;
    case NameRole: return e.name;
    case PathRole: return e.path;
    case RelRole: return e.rel;
    case ComposerRole: return e.composer;
    case PagesRole: return e.pages;
    case CountRole: return e.count;
    case OpenCountRole: return e.openCount;
    case LastOpenedTextRole: return Util::humanizeAgo(e.lastOpened);
    case ThumbRole: return e.thumb;
    case PreviewsRole: return e.previews;
    }
    return {};
}

QHash<int, QByteArray> EntryModel::roleNames() const
{
    return {
        {IsFolderRole, "isFolder"},
        {NameRole, "name"},
        {PathRole, "path"},
        {RelRole, "rel"},
        {ComposerRole, "composer"},
        {PagesRole, "pages"},
        {CountRole, "count"},
        {OpenCountRole, "openCount"},
        {LastOpenedTextRole, "lastOpenedText"},
        {ThumbRole, "thumb"},
        {PreviewsRole, "previews"},
    };
}

void EntryModel::setEntries(QVector<Entry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}
