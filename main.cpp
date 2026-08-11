#include <QApplication>
#include <QStyleFactory>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("YWD Motor Control");
    app.setApplicationVersion("1.0.0");

    // Use Fusion style for a consistent cross-platform look
    app.setStyle(QStyleFactory::create("Fusion"));

    // Dark palette
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window,          QColor(45, 45, 48));
    darkPalette.setColor(QPalette::WindowText,       Qt::white);
    darkPalette.setColor(QPalette::Base,             QColor(30, 30, 33));
    darkPalette.setColor(QPalette::AlternateBase,    QColor(45, 45, 48));
    darkPalette.setColor(QPalette::ToolTipBase,      Qt::white);
    darkPalette.setColor(QPalette::ToolTipText,      Qt::white);
    darkPalette.setColor(QPalette::Text,             Qt::white);
    darkPalette.setColor(QPalette::Button,           QColor(55, 55, 58));
    darkPalette.setColor(QPalette::ButtonText,       Qt::white);
    darkPalette.setColor(QPalette::BrightText,       Qt::red);
    darkPalette.setColor(QPalette::Link,             QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight,        QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText,  Qt::black);
    darkPalette.setColor(QPalette::Disabled, QPalette::Text,       QColor(128, 128, 128));
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(128, 128, 128));

    app.setPalette(darkPalette);

    // Global stylesheet tweaks
    app.setStyleSheet(
        "QToolTip { color: #ffffff; background-color: #2a82da; border: 1px solid white; }"
        "QGroupBox { font-weight: bold; border: 1px solid #555; border-radius: 5px; margin-top: 10px; padding-top: 10px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }"
        "QTabWidget::pane { border: 1px solid #555; border-radius: 4px; top: -1px; }"
        "QTabBar::tab { padding: 6px 16px; background: #37373a; border: 1px solid #555;"
        "  border-bottom: none; border-top-left-radius: 4px; border-top-right-radius: 4px; }"
        "QTabBar::tab:selected { background: #2a82da; }"
        "QSplitter::handle { background: #555; }"
        "QSplitter::handle:horizontal { width: 3px; }"
        "QSplitter::handle:vertical { height: 3px; }"
        "QStatusBar { border-top: 1px solid #555; }"
    );

    MainWindow w;
    w.show();

    return app.exec();
}
