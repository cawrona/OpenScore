// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QString>

namespace Util {

// Content hash used to identify a score independent of where it lives on disk.
// Annotations and library history key off this, so moving/renaming a PDF in a
// file manager does not lose anything.
inline QString fileSha1(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash h(QCryptographicHash::Sha1);
    h.addData(&f);
    return QString::fromLatin1(h.result().toHex());
}

inline QString humanizeAgo(qint64 secs)
{
    if (secs <= 0)
        return {};
    qint64 d = QDateTime::currentSecsSinceEpoch() - secs;
    if (d < 0)
        d = 0;
    if (d < 60)
        return QStringLiteral("Just now");
    if (d < 3600)
        return QString::number(d / 60) + QStringLiteral(" min ago");
    if (d < 86400) {
        const qint64 h = d / 3600;
        return h == 1 ? QStringLiteral("1 hour ago") : QString::number(h) + QStringLiteral(" hours ago");
    }
    const qint64 days = d / 86400;
    if (days == 1)
        return QStringLiteral("Yesterday");
    if (days < 7)
        return QString::number(days) + QStringLiteral(" days ago");
    if (days < 30) {
        const qint64 w = days / 7;
        return w == 1 ? QStringLiteral("1 week ago") : QString::number(w) + QStringLiteral(" weeks ago");
    }
    return QDateTime::fromSecsSinceEpoch(secs).date().toString(QStringLiteral("MMM d, yyyy"));
}

} // namespace Util
