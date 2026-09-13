#pragma once
#include "../common.h"
#include "../adapters/reading_view.h"
#include "base/icon_label.h"

#include <QLabel>
#include <QFontMetrics>

struct TwChapterPageConfig : TwIconLabelConfig {};

class TwChapterPageWidget : public TwIconLabel {
    Q_OBJECT

    bool widthInitialized = false;

public:
    TwChapterPageWidget(ReadingView* rdv, ReadingViewAdapters adapters, TwChapterPageConfig config, QWidget* parent = nullptr) : TwIconLabel(rdv, adapters, config, parent) {
        currentConfig.showIcon = false;
    }

    void onPageChanged() override {
        if (!widthInitialized) {
            QFontMetrics fm(textLabel->font());
            int minWidth = fm.width(QStringLiteral("999 OF 999"));
            textLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
            textLabel->setMinimumWidth(minWidth);
            widthInitialized = true;
        }

        if (!ReadingView_chapterCurrentPage || !ReadingView_chapterTotalPages) {
            return;
        }

        int currentPage = ReadingView_chapterCurrentPage(readingView);
        int totalPages = ReadingView_chapterTotalPages(readingView);
        bool shouldBeVisible = (currentPage >= 1 && totalPages >= 1 && currentPage <= totalPages);

        if (shouldBeVisible) {
            QString newText = QStringLiteral("%1 OF %2").arg(currentPage).arg(totalPages);
            if (textLabel->text() != newText) {
                textLabel->setText(newText);
            }
        }

        if (shouldBeVisible != !isHidden()) {
            setVisible(shouldBeVisible); // Use setVisible for conciseness
            syncSeparatorVisibility();
        }
    }

protected:
    QString iconSrc() const override { return {}; }
    QString iconDarkSrc() const override { return {}; }
};