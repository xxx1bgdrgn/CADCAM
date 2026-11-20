#include "UI/MainWindow.h"

#include <QApplication>
#include <QTimer>

#include <cstdlib>
#include <iostream>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("CADCAM"));
    cadcam::MainWindow window;
    window.show();

    const bool smoke = app.arguments().contains(QStringLiteral("--smoke"));
    QTimer::singleShot(0, &window, [&] {
        try {
            window.loadExample();
            if (smoke) {
                app.exit(window.smoke() ? EXIT_SUCCESS : EXIT_FAILURE);
            }
        } catch (...) {
            std::cerr << "CADCAM failed to initialize or render a workspace view.\n";
            app.exit(EXIT_FAILURE);
        }
    });
    return app.exec();
}
