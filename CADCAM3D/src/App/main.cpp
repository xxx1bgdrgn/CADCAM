#include "UI/MainWindow.h"

#include <QApplication>
#include <QMessageBox>
#include <QTimer>
#include <Standard_Failure.hxx>

#include <cstdlib>
#include <iostream>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("CADCAM3D"));
    cadcam3d::MainWindow window;
    window.show();

    const bool smoke = app.arguments().contains(QStringLiteral("--smoke"));
    QTimer::singleShot(0, &window, [&] {
        const auto fail = [&](const char* detail) {
            std::cerr << detail << '\n';
            if (!smoke) {
                QMessageBox::critical(&window, QStringLiteral("三维工作台启动失败"), QString::fromUtf8(detail));
            }
            app.exit(EXIT_FAILURE);
        };
        try {
            window.loadExample();
            if (smoke) {
                app.exit(window.smoke() ? EXIT_SUCCESS : EXIT_FAILURE);
            }
        } catch (const Standard_Failure& error) {
            fail(error.GetMessageString());
        } catch (const std::exception& error) {
            fail(error.what());
        }
    });
    return app.exec();
}
