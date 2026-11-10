#pragma once
#include <QDir>
#include <QString>
#include <string>
#include <utility>

// Shared read-only identity check for context-owned window geometry.
inline bool targetWindowBindingMatches(const std::pair<std::wstring,std::wstring>& binding,
                                       const std::wstring& title,const std::wstring& path) {
    if(binding.first.empty() || title.find(binding.first)==std::wstring::npos)return false;
    if(binding.second.empty())return true;
    const auto normalized=[](const std::wstring& value){
        return QDir::cleanPath(QDir::fromNativeSeparators(QString::fromStdWString(value))).toCaseFolded();
    };
    return !path.empty() && normalized(binding.second)==normalized(path);
}
