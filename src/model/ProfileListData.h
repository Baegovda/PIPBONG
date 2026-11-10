#pragma once
#include <QIcon>
#include <QString>

// Value-only presentation data; no widgets or persistence operations.
struct ProfileListRow {
    QString id,name,targetTitle,processPath;
    QIcon cachedIcon;
    bool isDefault=false;
};
