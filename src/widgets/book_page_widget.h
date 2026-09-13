#pragma once
#include "../common.h"
#include "../adapters/reading_view.h"
#include "base/icon_label.h"

#include <QLabel>
#include <QFontMetrics>

struct TwBookPageConfig : TwIconLabelConfig {};

class TwBookPageWidget : public TwIconLabel {
    Q_OBJECT

    bool widthInitialized = false;

public:
    TwBookPageWidget(ReadingView* rdv, ReadingViewAdapters adapters, TwBookPageConfig config, QWidget* parent = nullptr) : TwIconLabel(rdv, adapters, config, parent) {
        currentConfig.showIcon = false;
    }

    void onPageChanged() override {
        if (!widthInitialized) {
            QFontMetrics fm(textLabel->font());
            int minWidth = fm.width(QStringLiteral("9999 OF 9999"));
            textLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
            textLabel->setMinimumWidth(minWidth);
            widthInitialized = true;
        }

        if (!ReadingView_fullBookCurrentPage || !ReadingView_fullBookTotalPages) {
            return;
        }

        int currentPage = ReadingView_fullBookCurrentPage(readingView);
        int totalPages = ReadingView_fullBookTotalPages(readingView);

        if (currentPage < 1 || totalPages < 1 || currentPage > totalPages) {
            if (!textLabel->text().isEmpty()) {
                textLabel->setText(QString());
            }
            return;
        }

        QString newText = QStringLiteral("%1 OF %2").arg(currentPage).arg(totalPages);
        if (textLabel->text() != newText) {
            textLabel->setText(newText);
        }
    }

protected:
    QString iconSrc() const override { return {}; }
    QString iconDarkSrc() const override { return {}; }
};