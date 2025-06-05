//
// Created by viktorh on 5.6.2025.
//

#pragma once

#include <QTableWidgetItem>
#include <variant>

#include "../ProcessMemory.h"


class MatchTableItem : public QTableWidgetItem
{
public:
    MatchTableItem(const QString &text, Match m)
        : QTableWidgetItem(text), _match(std::move(m))
    {}

    const Match &match() const { return _match; }
    void        setMatch(Match m) { _match = std::move(m); }

private:
    Match _match;
};
